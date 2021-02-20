#include "parser.h"

#include <iostream>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const bill_layout_script &layout) {
    m_layout = &layout;
    
    try {
        if (intl::valid_language(layout.language_code)) {
            add_line<opcode::SETLANG>(layout.language_code);
        }
        for (auto &box : layout.m_boxes) {
            read_box(*box);
        }
        add_line<opcode::RET>();
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    for (auto line = m_code.begin(); line != m_code.end(); ++line) {
        if (line->command() == opcode::UNEVAL_JUMP) {
            auto &addr = line->get_args<opcode::UNEVAL_JUMP>();
            if (auto it = m_labels.find(addr.label); it != m_labels.end()) {
                *line = command_args(addr.cmd, jump_address(it->second - (line - m_code.begin())));
            } else {
                throw layout_error(fmt::format("Etichetta sconosciuta: {}", addr.label));
            }
        }
    }
}

void parser::add_label(const std::string &label) {
    if (!m_labels.try_emplace(label, m_code.size()).second) {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    }
}

void parser::read_box(const layout_box &box) {
    if (m_flags & PARSER_ADD_COMMENTS && !box.name.empty()) {
        add_line<opcode::COMMENT>("### " + box.name);
    }
    current_box = &box;

    m_lexer.set_bytecode(nullptr);
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == TOK_IDENTIFIER) {
        add_label(fmt::format("__label_{}", tok_label.value));
        m_lexer.require(TOK_END_OF_FILE);
    } else if (tok_label.type != TOK_END_OF_FILE) {
        throw unexpected_token(tok_label, TOK_IDENTIFIER);
    }

    if (m_flags & PARSER_ADD_COMMENTS) {
        m_lexer.set_bytecode(&m_code);
    }
    m_lexer.set_script(box.spacers);
    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == TOK_IDENTIFIER) {
            spacer_index index;
            switch (hash(tok.value)) {
            case hash("p"):
            case hash("page"):
                index = spacer_index::SPACER_PAGE;
                break;
            case hash("x"):
                index = spacer_index::SPACER_X;
                break;
            case hash("y"):
                index = spacer_index::SPACER_Y;
                break;
            case hash("w"):
            case hash("width"):
                index = spacer_index::SPACER_W;
                break;
            case hash("h"):
            case hash("height"):
                index = spacer_index::SPACER_H;
                break;
            case hash("t"):
            case hash("top"):
                index = spacer_index::SPACER_TOP;
                break;
            case hash("r"):
            case hash("right"):
                index = spacer_index::SPACER_RIGHT;
                break;
            case hash("b"):
            case hash("bottom"):
                index = spacer_index::SPACER_BOTTOM;
                break;
            case hash("l"):
            case hash("left"):
                index = spacer_index::SPACER_LEFT;
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
            if (negative) add_line<opcode::NEG>();
            add_line<opcode::MVBOX>(index);
        } else if (tok.type != TOK_END_OF_FILE) {
            throw unexpected_token(tok, TOK_IDENTIFIER);
        } else {
            break;
        }
    }

    add_line<opcode::RDBOX>(pdf_rect(box));

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
        opcode assign_op = opcode::SETVAR;
        
        switch (tok.type) {
        case TOK_ADD_ASSIGN:
        case TOK_SUB_ASSIGN:
            assign_op = tok.type == TOK_ADD_ASSIGN ? opcode::INC : opcode::DEC;
            [[fallthrough]];
        case TOK_ASSIGN:
            m_lexer.advance(tok);
            read_expression();
            break;
        default:
            add_line<opcode::PUSHVIEW>();
            break;
        }

        if (prefixes & VP_AGGREGATE)  add_line<opcode::AGGREGATE>();
        if (prefixes & VP_PARSENUM)   add_line<opcode::PARSENUM>();

        if (prefixes & VP_OVERWRITE)  {
            if (assign_op == opcode::DEC) {
                add_line<opcode::NEG>();
            }
            assign_op = opcode::RESETVAR;
        }
        vector_move_to_end(m_code, selvar_begin, selvar_end);
        m_code.emplace_back(assign_op);
    }
    }
    return true;
}

void parser::read_expression() {
    auto op_prec = [](token_type op_type) {
        switch (op_type) {
        case TOK_ASTERISK:
        case TOK_SLASH:
            return 6;
        case TOK_PLUS:
        case TOK_MINUS:
            return 5;
        case TOK_LESS:
        case TOK_LESS_EQ:
        case TOK_GREATER:
        case TOK_GREATER_EQ:
            return 4;
        case TOK_EQUALS:
        case TOK_NOT_EQUALS:
            return 3;
        case TOK_AND:
            return 2;
        case TOK_OR:
            return 1;
        default:
            return 0;
        }
    };

    auto op_opcode = [](token_type op_type) {
        switch (op_type) {
        case TOK_ASTERISK:      return opcode::MUL;
        case TOK_SLASH:         return opcode::DIV;
        case TOK_PLUS:          return opcode::ADD;
        case TOK_MINUS:         return opcode::SUB;
        case TOK_LESS:          return opcode::LT;
        case TOK_LESS_EQ:       return opcode::LEQ;
        case TOK_GREATER:       return opcode::GT;
        case TOK_GREATER_EQ:    return opcode::GEQ;
        case TOK_EQUALS:        return opcode::EQ;
        case TOK_NOT_EQUALS:    return opcode::NEQ;
        case TOK_AND:           return opcode::AND;
        case TOK_OR:            return opcode::OR;
        default:
            throw layout_error("Operatore non valido");
        }
    };

    sub_expression();

    std::vector<token_type> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        if (op_prec(tok_op.type) > 0) {
            m_lexer.advance(tok_op);
            if (!op_stack.empty() && op_prec(op_stack.back()) >= op_prec(tok_op.type)) {
                m_code.emplace_back(op_opcode(op_stack.back()));
                op_stack.pop_back();
            }
            op_stack.push_back(tok_op.type);
            sub_expression();
        } else {
            break;
        }
    }

    while (!op_stack.empty()) {
        m_code.emplace_back(op_opcode(op_stack.back()));
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
        add_line<opcode::NOT>();
        break;
    case TOK_MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            m_lexer.advance(tok_num);
            add_line<opcode::PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line<opcode::NEG>();
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case TOK_SLASH: 
        add_line<opcode::PUSHSTR>(m_lexer.require(TOK_REGEXP).parse_string());
        break;
    case TOK_STRING:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHSTR>(tok_first.parse_string());
        break;
    case TOK_CONTENT:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHVIEW>();
        break;
    default: {
        int prefixes = read_variable(true);
        if (prefixes & VP_MOVE) {
            add_line<opcode::MOVEVAR>();
        } else {
            add_line<opcode::PUSHVAR>();
        }
    }
    }
}

int parser::read_variable(bool read_only) {
    int prefixes = 0;

    variable_selector var_idx;

    token tok_prefix;

    auto add_flags_to = [&](auto &out, auto flags, bool condition) {
        if ((out & flags) || !condition) throw unexpected_token(tok_prefix, TOK_IDENTIFIER);
        out |= flags;
    };
    
    bool in_loop = true;
    while (in_loop) {
        tok_prefix = m_lexer.next();
        switch (tok_prefix.type) {
        case TOK_GLOBAL:    add_flags_to(var_idx.flags, SEL_GLOBAL, true); break;
        case TOK_PERCENT:   add_flags_to(prefixes, VP_PARSENUM, !read_only); break;
        case TOK_CARET:     add_flags_to(prefixes, VP_AGGREGATE, !read_only); break;
        case TOK_TILDE:     add_flags_to(prefixes, VP_OVERWRITE, !read_only); break;
        case TOK_AMPERSAND: add_flags_to(prefixes, VP_MOVE, read_only); break;
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

    add_line<opcode::SELVAR>(var_idx);

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
        add_line<opcode::SELVAR>(std::string(tok_var.value), small_int(0), small_int(0), uint8_t(SEL_GLOBAL & (-isglobal)));
        m_code.emplace_back(fun_name == "isset" ? opcode::ISSET : opcode::GETSIZE);
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

        auto call_op = [&] (int should_be, auto && ... args) {
            if (num_args != should_be) {
                throw invalid_numargs(fun_name, should_be, should_be, tok_fun_name);
            }
            m_code.emplace_back(std::forward<decltype(args)>(args) ...);
        };

        switch (hash(fun_name)) {
        case hash("num"):       call_op(1, opcode::PARSENUM); break;
        case hash("int"):       call_op(1, opcode::PARSEINT); break;
        case hash("aggregate"): call_op(1, opcode::AGGREGATE); break;
        case hash("null"):      call_op(0, opcode::PUSHNULL); break;
        case hash("ate"):       call_op(0, opcode::ATE); break;
        case hash("boxwidth"):  call_op(0, opcode::PUSHNUM, fixed_point(current_box->w)); break;
        case hash("boxheight"): call_op(0, opcode::PUSHNUM, fixed_point(current_box->h)); break;
        default:
            try {
                const auto &fun = find_function(fun_name);
                if (num_args < fun.minargs || num_args > fun.maxargs) {
                    throw invalid_numargs(fun_name, fun.minargs, fun.maxargs, tok_fun_name);
                }
                add_line<opcode::CALL>(fun_name, num_args);
            } catch (const std::out_of_range &error) {
                throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
            }
        }
    }
    }
}