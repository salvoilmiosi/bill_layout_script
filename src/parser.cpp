#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const std::filesystem::path &path, const layout_box_list &layout) {
    m_path = path;
    m_layout = &layout;
    
    try {
        m_code.add_line<opcode::LAYOUTNAME>(std::filesystem::canonical(path).string());
        if (layout.setlayout) {
            m_code.add_line<opcode::SETLAYOUT>();
        }
        for (auto &box : layout) {
            read_box(box);
        }
        if (layout.setlayout) {
            m_code.add_line<opcode::HLT>();
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    for (auto it = m_code.begin(); it != m_code.end(); ++it) {
        it->visit([&](auto &label) {
            if constexpr (std::is_base_of_v<jump_address, std::remove_reference_t<decltype(label)>>) {
                if (auto *str = std::get_if<string_ptr>(&label)) {
                    if (auto label_it = m_code.find_label(*str); label_it != m_code.end()) {
                        static_cast<jump_address &>(label) = ptrdiff_t(label_it - it);
                    } else {
                        throw layout_error(fmt::format("Etichetta sconosciuta: {}", **str));
                    }
                }
            }
        });
    }
}

void parser::read_box(const layout_box &box) {
    if (box.flags & box_flags::DISABLED) {
        return;
    }
    
    if (m_flags & parser_flags::ADD_COMMENTS && !box.name.empty()) {
        m_code.add_line<opcode::COMMENT>("### " + box.name);
    }
    current_box = &box;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback(nullptr);
    }
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == token_type::IDENTIFIER) {
        m_code.add_label(fmt::format("__{}_box_{}", m_parser_id, tok_label.value));
        m_lexer.require(token_type::END_OF_FILE);
    } else if (tok_label.type != token_type::END_OF_FILE) {
        throw unexpected_token(tok_label, token_type::IDENTIFIER);
    }

    if (!(box.flags & box_flags::NOREAD) || box.flags & box_flags::SPACER) {
        m_code.add_line<opcode::NEWBOX>();
        if (box.type == box_type::RECTANGLE) {
            m_code.add_line<opcode::PUSHDOUBLE>(box.x);
            m_code.add_line<opcode::MVBOX>(spacer_index::X);
            m_code.add_line<opcode::PUSHDOUBLE>(box.y);
            m_code.add_line<opcode::MVBOX>(spacer_index::Y);
            m_code.add_line<opcode::PUSHDOUBLE>(box.w);
            m_code.add_line<opcode::MVBOX>(spacer_index::WIDTH);
            m_code.add_line<opcode::PUSHDOUBLE>(box.h);
            m_code.add_line<opcode::MVBOX>(spacer_index::HEIGHT);
        }
        if (box.type != box_type::WHOLEFILE) {
            m_code.add_line<opcode::PUSHINT>(box.page);
            m_code.add_line<opcode::MVBOX>(spacer_index::PAGE);
        }
    }
    m_content_level = 0;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback([this](const std::string &line){
            m_code.add_line<opcode::COMMENT>(line);
        });
    }
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == token_type::IDENTIFIER) {
            try {
                auto idx = find_enum_index<spacer_index>(tok.value);
                bool negative = false;
                auto tok_sign = m_lexer.next();
                switch (tok_sign.type) {
                case token_type::PLUS:
                    break;
                case token_type::MINUS:
                    negative = true;
                    break;
                default:
                    throw unexpected_token(tok_sign, token_type::PLUS);
                }
                read_expression();
                m_lexer.require(token_type::SEMICOLON);
                if (negative) m_code.add_line<opcode::CALL>("neg", 1);
                m_code.add_line<opcode::MVBOX>(idx);
            } catch (std::out_of_range) {
                throw parsing_error(fmt::format("Flag spacer non valido: {}", tok.value), tok);
            }
        } else if (tok.type != token_type::END_OF_FILE) {
            throw unexpected_token(tok, token_type::IDENTIFIER);
        } else {
            break;
        }
    }

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        ++m_content_level;
        m_code.add_line<opcode::RDBOX>(box.mode, box.type, box.flags);
    }

    m_lexer.set_script(box.script);
    while (!m_lexer.check_next(token_type::END_OF_FILE)) {
        read_statement();
    }

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        m_code.add_line<opcode::POPCONTENT>();
    }
}

void parser::read_statement() {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case token_type::BRACE_BEGIN:
        m_lexer.advance(tok_first);
        while (!m_lexer.check_next(token_type::BRACE_END)) {
            read_statement();
        }
        break;
    case token_type::FUNCTION:
        read_keyword();
        break;
    case token_type::END_OF_FILE:
        throw unexpected_token(tok_first);
    case token_type::SEMICOLON:
        m_lexer.advance(tok_first);
        break;
    default:
        sub_statement();
        m_lexer.require(token_type::SEMICOLON);
    }
}

void parser::sub_statement() {
    auto tok = m_lexer.peek();
    bool dot = tok.type == token_type::DOT;
    if (dot) {
        m_lexer.advance(tok);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok);
        }
    }

    auto selvar_begin = m_code.size();
    auto prefixes = read_variable(false);
    auto selvar_end = m_code.size();

    if (dot) {
        m_code.add_line<opcode::PUSHVIEW>();
    } else {
        tok = m_lexer.next();
        switch (tok.type) {
        case token_type::SUB_ASSIGN:
            prefixes.flags |= setvar_flags::DECREASE;
            read_expression();
            break;
        case token_type::ADD_ASSIGN:
            prefixes.flags |= setvar_flags::INCREASE;
            read_expression();
            break;
        case token_type::ADD_ONE:
            prefixes.flags |= setvar_flags::INCREASE;
            m_code.add_line<opcode::PUSHINT>(1);
            break;
        case token_type::SUB_ONE:
            prefixes.flags |= setvar_flags::DECREASE;
            m_code.add_line<opcode::PUSHINT>(1);
            break;
        case token_type::ASSIGN:
            read_expression();
            break;
        default:
            throw unexpected_token(tok, token_type::ASSIGN);
        }
    }

    if (prefixes.call.command() != opcode::NOP) {
        m_code.push_back(prefixes.call);
    }

    m_code.move_not_comments(selvar_begin, selvar_end);
    m_code.add_line<opcode::SETVAR>(prefixes.flags);
}

void parser::read_expression() {
    struct operator_precedence {
        command_args command;
        int precedence;

        operator_precedence(std::string_view fun_name, int precedence)
            : command(make_command<opcode::CALL>(fun_name, 2))
            , precedence(precedence) {}
    };
    static const std::map<token_type, operator_precedence> operators = {
        {token_type::ASTERISK,      {"mul", 6}},
        {token_type::SLASH,         {"div", 6}},
        {token_type::PLUS,          {"add", 5}},
        {token_type::MINUS,         {"sub", 5}},
        {token_type::LESS,          {"lt",  4}},
        {token_type::LESS_EQ,       {"leq", 4}},
        {token_type::GREATER,       {"gt",  4}},
        {token_type::GREATER_EQ,    {"geq", 4}},
        {token_type::EQUALS,        {"eq",  3}},
        {token_type::NOT_EQUALS,    {"neq", 3}},
        {token_type::AND,           {"and", 2}},
        {token_type::OR,            {"or",  1}},
    };

    sub_expression();

    simple_stack<decltype(operators)::const_iterator> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        auto it = operators.find(tok_op.type);
        if (it == std::end(operators)) break;
        
        m_lexer.advance(tok_op);
        if (!op_stack.empty() && op_stack.back()->second.precedence >= it->second.precedence) {
            m_code.push_back(op_stack.back()->second.command);
            op_stack.pop_back();
        }
        op_stack.push_back(it);
        sub_expression();
    }

    while (!op_stack.empty()) {
        m_code.push_back(op_stack.back()->second.command);
        op_stack.pop_back();
    }
}

void parser::sub_expression() {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case token_type::PAREN_BEGIN:
        m_lexer.advance(tok_first);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        break;
    case token_type::FUNCTION:
        read_function();
        break;
    case token_type::NOT:
        m_lexer.advance(tok_first);
        sub_expression();
        m_code.add_line<opcode::CALL>("not", 1);
        break;
    case token_type::PLUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(string_to<big_int>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("num", 1);
        }
        break;
    }
    case token_type::MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(-string_to<big_int>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("neg", 1);
        }
        break;
    }
    case token_type::INTEGER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHINT>(string_to<big_int>(tok_first.value));
        break;
    case token_type::NUMBER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case token_type::SLASH: 
        m_code.add_line<opcode::PUSHSTR>(m_lexer.require(token_type::REGEXP).parse_string());
        break;
    case token_type::STRING:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHSTR>(tok_first.parse_string());
        break;
    case token_type::CONTENT:
        m_lexer.advance(tok_first);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_first);
        }
        m_code.add_line<opcode::PUSHVIEW>();
        break;
    default: {
        auto prefixes = read_variable(true);
        if (!prefixes.function_arg) {
            m_code.add_line<opcode::PUSHVAR>();
        }
    }
    }
}

variable_prefixes parser::read_variable(bool read_only) {
    variable_prefixes prefixes;
    variable_selector selvar;

    token tok_prefix;

    auto read_only_error = [](const token &tok) {
        return parsing_error("Contesto di sola lettura", tok);
    };

    auto add_flags_to = [&](auto &out, auto flags, bool read_write = false) {
        if (out & flags) throw parsing_error("Prefisso duplicato", tok_prefix);
        if (!read_write && read_only) throw read_only_error(tok_prefix);
        out |= flags;
    };

    auto add_function_call = [&](std::string_view fun_name) {
        if (read_only) throw read_only_error(tok_prefix);
        if (prefixes.call.command() == opcode::NOP) {
            prefixes.call = make_command<opcode::CALL>(fun_name, 1);
        } else {
            throw parsing_error("Ammesso solo un prefisso di funzione", tok_prefix);
        }
    };
    
    bool in_loop = true;
    while (in_loop) {
        tok_prefix = m_lexer.next();
        switch (tok_prefix.type) {
        case token_type::ASTERISK:      add_flags_to(selvar.flags, selvar_flags::GLOBAL, true); break;
        case token_type::TILDE:         add_flags_to(prefixes.flags, setvar_flags::OVERWRITE); break;
        case token_type::NOT:           add_flags_to(prefixes.flags, setvar_flags::FORCE); break;
        case token_type::PERCENT:       add_function_call("num"); break;
        case token_type::CARET:         add_function_call("aggregate"); break;
        case token_type::SINGLE_QUOTE:  add_function_call("capitalize"); break;
        case token_type::BRACKET_BEGIN:
            read_expression();
            m_lexer.require(token_type::BRACKET_END);
            selvar.flags |= selvar_flags::DYN_NAME;
            in_loop = false;
            break;
        case token_type::IDENTIFIER:
            selvar.name = tok_prefix.value;
            in_loop = false;
            break;
        default:
            throw unexpected_token(tok_prefix, token_type::IDENTIFIER);
        }
    }

    if (read_only) {
        if (auto it = std::ranges::find(m_fun_args, selvar.name); it != m_fun_args.end()) {
            m_code.add_line<opcode::PUSHARG>(it - m_fun_args.begin());
            prefixes.function_arg = true;
            return prefixes;
        }
    }

    if (m_lexer.check_next(token_type::BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case token_type::COLON: {
            if (read_only) throw read_only_error(tok);
            m_lexer.advance(tok);
            tok = m_lexer.peek();
            switch (tok.type) {
            case token_type::BRACKET_END:
                selvar.flags |= selvar_flags::EACH; // variable[:]
                break;
            case token_type::INTEGER: // variable[:N] -- append N times
                m_lexer.advance(tok);
                selvar.flags |= selvar_flags::APPEND;
                selvar.length = string_to<int>(tok.value);
                break;
            default:
                read_expression();
                selvar.flags |= selvar_flags::APPEND;
                selvar.flags |= selvar_flags::DYN_LEN;
            }
            break;
        }
        case token_type::BRACKET_END:
            if (read_only) throw read_only_error(tok);
            selvar.flags |= selvar_flags::APPEND; // variable[] -- append
            break;
        default:
            if (tok = m_lexer.check_next(token_type::INTEGER)) { // variable[N]
                selvar.index = string_to<int>(tok.value);
            } else {
                read_expression();
                selvar.flags |= selvar_flags::DYN_IDX;
            }
            if (tok = m_lexer.check_next(token_type::COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw read_only_error(tok);
                if (tok = m_lexer.check_next(token_type::INTEGER)) {
                    selvar.length = string_to<int>(tok.value);
                } else {
                    read_expression();
                    selvar.flags |= selvar_flags::DYN_LEN;
                }
            }
        }
        m_lexer.require(token_type::BRACKET_END);
    }

    m_code.add_line<opcode::SELVAR>(selvar);

    return prefixes;
}

template<opcode Cmd>
void parser::add_enum_index_command() {
    m_lexer.require(token_type::DOT);
    auto tok = m_lexer.require(token_type::IDENTIFIER);
    try {
        m_code.add_line<Cmd>(find_enum_index<EnumType<Cmd>>(tok.value));
    } catch (std::out_of_range) {
        throw parsing_error(fmt::format("Argomento non valido: {}", tok.value), tok);
    }
};

void parser::read_function() {
    auto tok_fun_name = m_lexer.require(token_type::FUNCTION);
    auto fun_name = tok_fun_name.value.substr(1);
    
    switch (hash(fun_name)) {
    case hash("box"): add_enum_index_command<opcode::GETBOX>(); break;
    case hash("doc"): add_enum_index_command<opcode::GETDOC>(); break;
    default: {
        small_int num_args = 0;
        m_lexer.require(token_type::PAREN_BEGIN);
        while (!m_lexer.check_next(token_type::PAREN_END)) {
            ++num_args;
            read_expression();
            auto tok_comma = m_lexer.peek();
            switch (tok_comma.type) {
            case token_type::COMMA:
                m_lexer.advance(tok_comma);
                break;
            case token_type::PAREN_END:
                break;
            default:
                throw unexpected_token(tok_comma, token_type::PAREN_END);
            }
        }

        if (auto it = function_lookup.find(fun_name); it != function_lookup.end()) {
            const auto &fun = it->second;
            if (num_args < fun.minargs || num_args > fun.maxargs) {
                throw invalid_numargs(std::string(fun_name), fun.minargs, fun.maxargs, tok_fun_name);
            }
            m_code.add_line<opcode::CALL>(it, num_args);
        } else if (auto it = m_functions.find(fun_name); it != m_functions.end()) {
            if (it->second.numargs != num_args) {
                throw invalid_numargs(std::string(fun_name), it->second.numargs, it->second.numargs, tok_fun_name);
            }
            if (it->second.has_contents && m_content_level == 0) {
                throw parsing_error(fmt::format("Impossibile chiamare {}, stack contenuti vuoto", fun_name), tok_fun_name);
            }
            m_code.add_line<opcode::JSRVAL>(fmt::format("__function_{}", fun_name), num_args);
        } else {
            throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
        }
    }
    }
}