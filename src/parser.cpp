#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const std::filesystem::path &path, const box_vector &layout) {
    m_path = path;
    m_layout = &layout;
    
    try {
        if (intl::valid_language(layout.language_code)) {
            add_line<OP_SETLANG>(layout.language_code);
        }
        for (auto &box : layout) {
            read_box(box);
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    if (!(m_flags & PARSER_NO_EVAL_JUMPS)) {
        for (auto line = m_code.begin(); line != m_code.end(); ++line) {
            if (line->command() == OP_UNEVAL_JUMP) {
                auto &addr = line->get_args<OP_UNEVAL_JUMP>();
                if (auto it = m_labels.find(addr.label); it != m_labels.end()) {
                    *line = command_args(addr.cmd, jump_address(it->second - (line - m_code.begin())));
                } else {
                    throw layout_error(fmt::format("Etichetta sconosciuta: {}", addr.label));
                }
            }
        }
    }
}

void parser::add_label(const std::string &label) {
    if (!m_labels.try_emplace(label, m_code.size()).second) {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    }
}

void parser::add_comment(const std::string &line) {
    m_comments.emplace(m_code.size(), line);
}

void parser::read_box(const layout_box &box) {
    if (m_flags & PARSER_ADD_COMMENTS && !box.name.empty()) {
        add_comment("### " + box.name);
    }
    current_box = &box;

    if (m_flags & PARSER_ADD_COMMENTS) {
        m_lexer.set_comment_callback(nullptr);
    }
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == TOK_IDENTIFIER) {
        add_label(fmt::format("__label_{}", tok_label.value));
        m_lexer.require(TOK_END_OF_FILE);
    } else if (tok_label.type != TOK_END_OF_FILE) {
        throw unexpected_token(tok_label, TOK_IDENTIFIER);
    }

    if (m_flags & PARSER_ADD_COMMENTS) {
        m_lexer.set_comment_callback([this](const std::string &line){
            add_comment(line);
        });
    }
    m_lexer.set_script(box.spacers);
    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == TOK_IDENTIFIER) {
            spacer_index index;
            switch (hash(tok.value)) {
            case hash("p"):
            case hash("page"):
                index = SPACER_PAGE;
                break;
            case hash("x"):
                index = SPACER_X;
                break;
            case hash("y"):
                index = SPACER_Y;
                break;
            case hash("w"):
            case hash("width"):
                index = SPACER_W;
                break;
            case hash("h"):
            case hash("height"):
                index = SPACER_H;
                break;
            case hash("t"):
            case hash("top"):
                index = SPACER_TOP;
                break;
            case hash("r"):
            case hash("right"):
                index = SPACER_RIGHT;
                break;
            case hash("b"):
            case hash("bottom"):
                index = SPACER_BOTTOM;
                break;
            case hash("l"):
            case hash("left"):
                index = SPACER_LEFT;
                break;
            default:
                throw unexpected_token(tok);
            }

            bool negative = false;
            auto tok_sign = m_lexer.next();
            switch (tok_sign.type) {
            case TOK_PLUS:
                break;
            case TOK_MINUS:
                negative = true;
                break;
            default:
                throw unexpected_token(tok_sign, TOK_PLUS);
            }
            read_expression();
            if (negative) add_line<OP_CALL>("neg", 1);
            add_line<OP_MVBOX>(index);
        } else if (tok.type != TOK_END_OF_FILE) {
            throw unexpected_token(tok, TOK_IDENTIFIER);
        } else {
            break;
        }
    }

    add_line<OP_RDBOX>(pdf_rect(box));

    m_lexer.set_script(box.script);
    while (read_statement(false));
}

bool parser::read_statement(bool throw_on_eof) {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case TOK_BRACE_BEGIN:
        m_lexer.advance(tok_first);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            read_statement();
        }
        break;
    case TOK_FUNCTION:
        read_keyword();
        break;
    case TOK_END_OF_FILE:
        if (throw_on_eof) {
            throw unexpected_token(tok_first);
        }
        return false;
    default: {
        size_t selvar_begin = m_code.size();
        int prefixes = read_variable(false);
        size_t selvar_end = m_code.size();
        
        auto tok = m_lexer.peek();
        flags_t flags = 0;
        
        switch (tok.type) {
        case TOK_ADD_ASSIGN:
        case TOK_SUB_ASSIGN:
            flags |= tok.type == TOK_ADD_ASSIGN ? SET_INCREASE : SET_DECREASE;
            [[fallthrough]];
        case TOK_ASSIGN:
            m_lexer.advance(tok);
            read_expression();
            break;
        default:
            add_line<OP_PUSHVIEW>();
            break;
        }

        if (prefixes & VP_AGGREGATE)  add_line<OP_CALL>("aggregate", 1);
        if (prefixes & VP_PARSENUM)   add_line<OP_CALL>("num", 1);
        if (prefixes & VP_OVERWRITE)  flags |= SET_OVERWRITE;
        if (prefixes & VP_FORCE)      flags |= SET_FORCE;

        vector_move_to_end(m_code, selvar_begin, selvar_end);
        add_line<OP_SETVAR>(flags);
    }
    }
    return true;
}

void parser::read_expression() {
    std::tuple<int, command_args, token_type> operators[] = {
        {6, make_command<OP_CALL>("mul", 2),    TOK_ASTERISK},
        {6, make_command<OP_CALL>("div", 2),    TOK_SLASH},
        {5, make_command<OP_CALL>("add", 2),    TOK_PLUS},
        {5, make_command<OP_CALL>("sub", 2),    TOK_MINUS},
        {4, make_command<OP_CALL>("lt", 2),     TOK_LESS},
        {4, make_command<OP_CALL>("leq", 2),    TOK_LESS_EQ},
        {4, make_command<OP_CALL>("gt", 2),     TOK_GREATER},
        {4, make_command<OP_CALL>("geq", 2),    TOK_GREATER_EQ},
        {3, make_command<OP_CALL>("eq", 2),     TOK_EQUALS},
        {3, make_command<OP_CALL>("neq", 2),    TOK_NOT_EQUALS},
        {2, make_command<OP_CALL>("and", 2),    TOK_AND},
        {1, make_command<OP_CALL>("or", 2),     TOK_OR}
    };

    sub_expression();

    std::vector<decltype(+operators)> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        auto it = std::find_if(std::begin(operators), std::end(operators), [&](const auto &it) {
            return std::get<token_type>(it) == tok_op.type;
        });
        if (it == std::end(operators)) break;
        
        m_lexer.advance(tok_op);
        if (!op_stack.empty() && std::get<int>(*op_stack.back()) >= std::get<int>(*it)) {
            m_code.push_back(std::get<command_args>(*op_stack.back()));
            op_stack.pop_back();
        }
        op_stack.push_back(it);
        sub_expression();
    }

    while (!op_stack.empty()) {
        m_code.push_back(std::get<command_args>(*op_stack.back()));
        op_stack.pop_back();
    }
}

void parser::sub_expression() {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case TOK_PAREN_BEGIN:
        m_lexer.advance(tok_first);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        break;
    case TOK_FUNCTION:
        read_function();
        break;
    case TOK_NOT:
        m_lexer.advance(tok_first);
        sub_expression();
        add_line<OP_CALL>("not", 1);
        break;
    case TOK_MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            m_lexer.advance(tok_num);
            add_line<OP_PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line<OP_CALL>("neg", 1);
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case TOK_SLASH: 
        add_line<OP_PUSHSTR>(m_lexer.require(TOK_REGEXP).parse_string());
        break;
    case TOK_STRING:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHSTR>(tok_first.parse_string());
        break;
    case TOK_CONTENT:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHVIEW>();
        break;
    default: {
        int prefixes = read_variable(true);
        if (prefixes & VP_REF) {
            add_line<OP_PUSHREF>();
        } else {
            add_line<OP_PUSHVAR>();
        }
    }
    }
}

int parser::read_variable(bool read_only) {
    int prefixes = 0;

    variable_selector var_idx;

    token tok_prefix;

    auto add_flags_to = [&](auto &out, auto flags, bool condition = true) {
        if ((out & flags) || !condition) throw unexpected_token(tok_prefix, TOK_IDENTIFIER);
        out |= flags;
    };
    
    bool in_loop = true;
    while (in_loop) {
        tok_prefix = m_lexer.next();
        switch (tok_prefix.type) {
        case TOK_GLOBAL:    add_flags_to(var_idx.flags, SEL_GLOBAL); break;
        case TOK_PERCENT:   add_flags_to(prefixes, VP_PARSENUM, !read_only); break;
        case TOK_CARET:     add_flags_to(prefixes, VP_AGGREGATE, !read_only); break;
        case TOK_TILDE:     add_flags_to(prefixes, VP_OVERWRITE, !read_only); break;
        case TOK_NOT:       add_flags_to(prefixes, VP_FORCE, !read_only); break;
        case TOK_AMPERSAND: add_flags_to(prefixes, VP_REF, read_only); break;
        case TOK_IDENTIFIER:
            var_idx.name = std::string(tok_prefix.value);
            in_loop = false;
            break;
        default:
            throw unexpected_token(tok_prefix, TOK_IDENTIFIER);
        }
    }

    if (m_lexer.check_next(TOK_BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case TOK_COLON: {
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            m_lexer.advance(tok);
            tok = m_lexer.peek();
            switch (tok.type) {
            case TOK_BRACKET_END:
                var_idx.flags |= SEL_EACH; // variable[:]
                break;
            case TOK_INTEGER: // variable[:N] -- append N times
                m_lexer.advance(tok);
                var_idx.flags |= SEL_APPEND;
                var_idx.length = cstoi(tok.value);
                break;
            default:
                read_expression();
                var_idx.flags |= SEL_APPEND | SEL_DYN_LEN;
            }
            break;
        }
        case TOK_BRACKET_END:
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            var_idx.flags |= SEL_APPEND; // variable[] -- append
            break;
        default:
            if (tok = m_lexer.check_next(TOK_INTEGER)) { // variable[N]
                var_idx.index = cstoi(tok.value);
            } else {
                read_expression();
                var_idx.flags |= SEL_DYN_IDX;
            }
            if (tok = m_lexer.check_next(TOK_COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw unexpected_token(tok, TOK_BRACKET_END);
                if (tok = m_lexer.check_next(TOK_INTEGER)) {
                    var_idx.length = cstoi(tok.value);
                } else {
                    read_expression();
                    var_idx.flags |= SEL_DYN_LEN;
                }
            }
        }
        m_lexer.require(TOK_BRACKET_END);
    }

    add_line<OP_SELVAR>(var_idx);

    return prefixes;
}

void parser::read_function() {
    auto tok_fun_name = m_lexer.require(TOK_FUNCTION);
    std::string fun_name(tok_fun_name.value.substr(1));

    switch (hash(fun_name)) {
    case hash("isset"):
    case hash("size"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        bool isglobal = m_lexer.check_next(TOK_GLOBAL);
        auto tok_var = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        add_line<OP_SELVAR>(std::string(tok_var.value), small_int(0), small_int(0), flags_t(SEL_GLOBAL & (-isglobal)));
        if (fun_name == "isset") {
            add_line<OP_ISSET>();
        } else {
            add_line<OP_GETSIZE>();
        }
        break;
    }
    default: {
        small_int num_args = 0;
        m_lexer.require(TOK_PAREN_BEGIN);
        while (!m_lexer.check_next(TOK_PAREN_END)) {
            ++num_args;
            read_expression();
            auto tok_comma = m_lexer.peek();
            switch (tok_comma.type) {
            case TOK_COMMA:
                m_lexer.advance(tok_comma);
                break;
            case TOK_PAREN_END:
                break;
            default:
                throw unexpected_token(tok_comma, TOK_PAREN_END);
            }
        }

        auto call_op = [&] (int should_be, command_args &&cmd) {
            if (num_args != should_be) {
                throw invalid_numargs(fun_name, should_be, should_be, tok_fun_name);
            }
            m_code.push_back(std::move(cmd));
        };

        switch (hash(fun_name)) {
        case hash("ate"):       call_op(0, make_command<OP_ATE>()); break;
        case hash("boxwidth"):  call_op(0, make_command<OP_PUSHNUM>(current_box->w)); break;
        case hash("boxheight"): call_op(0, make_command<OP_PUSHNUM>(current_box->h)); break;
        default:
            try {
                const auto &fun = find_function(fun_name);
                if (num_args < fun.minargs || num_args > fun.maxargs) {
                    throw invalid_numargs(fun_name, fun.minargs, fun.maxargs, tok_fun_name);
                }
                add_line<OP_CALL>(fun_name, num_args);
            } catch (const std::out_of_range &error) {
                throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
            }
        }
    }
    }
}