#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto make_label = [&](std::string_view label) {
        return fmt::format("__{}_{}_{}", m_parser_id, m_code.size(), label);
    };

    auto tok_name = m_lexer.require(token_type::FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    enum function_type {
        FUN_EXPRESSION,
        FUN_2_EXPRESSIONS,
        FUN_VARIABLE,
        FUN_VOID,
    };
    
    static const std::map<std::string, std::tuple<function_type, command_args>, std::less<>> simple_functions = {
        {"error",       {FUN_2_EXPRESSIONS, make_command<opcode::THROWERROR>()}},
        {"note",        {FUN_EXPRESSION, make_command<opcode::ADDNOTE>()}},
        {"setbegin",    {FUN_EXPRESSION, make_command<opcode::SETBEGIN>()}},
        {"setend",      {FUN_EXPRESSION, make_command<opcode::SETEND>()}},
        {"clear",       {FUN_VARIABLE,   make_command<opcode::CLEAR>()}},
        {"nexttable",   {FUN_VOID,       make_command<opcode::NEXTTABLE>()}},
        {"halt",        {FUN_VOID,       make_command<opcode::HLT>()}}
    };

    if (auto it = simple_functions.find(fun_name); it != simple_functions.end()) {
        m_lexer.require(token_type::PAREN_BEGIN);
        switch (std::get<function_type>(it->second)) {
        case FUN_EXPRESSION:
            read_expression();
            break;
        case FUN_2_EXPRESSIONS:
            read_expression();
            m_lexer.require(token_type::COMMA);
            read_expression();
            break;
        case FUN_VARIABLE:
            read_variable(false);
            break;
        case FUN_VOID:
            break;
        }
        m_lexer.require(token_type::PAREN_END);
        m_lexer.require(token_type::SEMICOLON);
        m_code.push_back(std::get<command_args>(it->second));
    } else switch (hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"): {
        string_ptr endif_label = make_label("endif");
        string_ptr else_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool add_endif = false;
        while (in_loop) {
            bool has_else = false;
            else_label = make_label("else");
            m_lexer.require(token_type::PAREN_BEGIN);
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            if (condition_positive) {
                m_code.add_line<opcode::JZ>(else_label);
            } else {
                m_code.add_line<opcode::JNZ>(else_label);
            }
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == token_type::FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance(tok_if);
                    add_endif = true;
                    has_else = true;
                    m_code.add_line<opcode::JMP>(endif_label);
                    m_code.add_label(else_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    add_endif = true;
                    m_code.add_line<opcode::JMP>(endif_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_else) m_code.add_label(else_label);
        }
        if (add_endif) m_code.add_label(endif_label);
        break;
    }
    case hash("while"): {
        string_ptr while_label = make_label("while");
        string_ptr endwhile_label = make_label("endwhile");
        m_loop_labels.push(loop_label_pair{while_label, endwhile_label});
        m_lexer.require(token_type::PAREN_BEGIN);
        m_code.add_label(while_label);
        read_expression();
        m_code.add_line<opcode::JZ>(endwhile_label);
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        m_code.add_line<opcode::JMP>(while_label);
        m_code.add_label(endwhile_label);
        m_loop_labels.pop();
        break;
    }
    case hash("for"): {
        string_ptr for_label = make_label("for");
        string_ptr endfor_label = make_label("endfor");
        m_loop_labels.push(loop_label_pair{for_label, endfor_label});
        m_lexer.require(token_type::PAREN_BEGIN);
        if (!m_lexer.check_next(token_type::SEMICOLON)) {
            sub_statement();
            m_lexer.require(token_type::SEMICOLON);
        }
        m_code.add_label(for_label);
        if (!m_lexer.check_next(token_type::SEMICOLON)) {
            read_expression();
            m_code.add_line<opcode::JZ>(endfor_label);
            m_lexer.require(token_type::SEMICOLON);
        }
        auto increase_stmt_begin = m_code.size();
        if (!m_lexer.check_next(token_type::PAREN_END)) {
            sub_statement();
            m_lexer.require(token_type::PAREN_END);
        }
        auto increase_stmt_end = m_code.size();
        read_statement();
        m_code.move_not_comments(increase_stmt_begin, increase_stmt_end);
        m_code.add_line<opcode::JMP>(for_label);
        m_code.add_label(endfor_label);
        m_loop_labels.pop();
        break;
    }
    case hash("goto"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        for (size_t i=0; i<m_content_level; ++i) {
            m_code.add_line<opcode::POPCONTENT>();
        }
        m_code.add_line<opcode::JMP>(fmt::format("__{}_box_{}", m_parser_id, tok.value));
        m_lexer.require(token_type::PAREN_END);
        m_lexer.require(token_type::SEMICOLON);
        break;
    }
    case hash("function"): {
        bool has_content = false;
        m_lexer.require(token_type::PAREN_BEGIN);
        auto name = m_lexer.require(token_type::IDENTIFIER);
        small_int num_args = 0;
        while (!m_lexer.check_next(token_type::PAREN_END)) {
            m_lexer.require(token_type::COMMA);
            auto tok = m_lexer.next();
            switch (tok.type) {
            case token_type::CONTENT:
                if (num_args == 0 && !has_content) {
                    has_content = true;
                    ++m_content_level;
                } else {
                    throw unexpected_token(tok, token_type::IDENTIFIER);
                }
                break;
            case token_type::IDENTIFIER:
                if (std::ranges::find(m_fun_args, tok.value) != m_fun_args.end()) {
                    throw parsing_error(fmt::format("Argomento funzione duplicato: {}", tok.value), tok);
                }
                m_fun_args.emplace_back(tok.value);
                ++num_args;
                break;
            default:
                throw unexpected_token(tok, token_type::IDENTIFIER);
            }
        }
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

        m_code.add_line<opcode::JMP>(endfun_label);

        m_code.add_label(fun_label);
        m_functions.emplace(std::string(name.value), function_info{num_args, has_content});

        read_statement();
        m_code.add_line<opcode::RET>();
        m_code.add_label(endfun_label);
        m_fun_args.resize(m_fun_args.size() - num_args);
        if (has_content) {
            --m_content_level;
        }
        break;
    }
    case hash("call"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        small_int numargs = 0;
        while (!m_lexer.check_next(token_type::PAREN_END)) {
            m_lexer.require(token_type::COMMA);
            read_expression();
            ++numargs;
        }
        m_lexer.require(token_type::SEMICOLON);
        if (auto it = m_functions.find(tok.value); it != m_functions.end()) {
            if (it->second.numargs != numargs) {
                throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", tok.value, it->second.numargs), tok);
            }
            if (it->second.has_contents && m_content_level == 0) {
                throw parsing_error(fmt::format("Impossibile chiamare {}, stack contenuti vuoto", tok.value), tok);
            }
            m_code.add_line<opcode::JSR>(fmt::format("__function_{}", tok.value), numargs);
        } else {
            throw parsing_error(fmt::format("Funzione {} non dichiarata", tok.value), tok);
        }
        break;
    }
    case hash("foreach"): {
        string_ptr begin_label = make_label("foreach");
        string_ptr continue_label = make_label("foreach_continue");
        string_ptr end_label = make_label("endforeach");
        m_loop_labels.push(loop_label_pair{continue_label, end_label});
        bool pushed_content = false;
        if (m_lexer.check_next(token_type::PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            ++m_content_level;
            m_code.add_line<opcode::ADDCONTENT>();
            pushed_content = true;
        }
        m_code.add_line<opcode::SPLITVIEW>();
        m_code.add_label(begin_label);
        read_statement();
        m_code.add_label(continue_label);
        m_code.add_line<opcode::NEXTRESULT>();
        m_code.add_line<opcode::JNTE>(begin_label);
        m_code.add_label(end_label);
        if (pushed_content) {
            --m_content_level;
            m_code.add_line<opcode::POPCONTENT>();
        } else {
            m_code.add_line<opcode::RESETVIEW>();
        }
        m_loop_labels.pop();
        break;
    }
    case hash("with"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        ++m_content_level;
        m_code.add_line<opcode::ADDCONTENT>();
        read_statement();
        --m_content_level;
        m_code.add_line<opcode::POPCONTENT>();
        break;
    }
    case hash("between"): {
        m_code.add_line<opcode::NEWVIEW>();
        m_lexer.require(token_type::PAREN_BEGIN);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_name);
        }
        auto begin_str = m_lexer.require(token_type::STRING).parse_string();
        int begin_len = begin_str.size();
        if (begin_len != 0) {
            m_code.add_line<opcode::PUSHVIEW>();
            m_code.add_line<opcode::PUSHSTR>(std::move(begin_str));
            m_code.add_line<opcode::CALL>("indexof", 2);
            m_code.add_line<opcode::SETBEGIN>();
        }
        if (m_lexer.check_next(token_type::COMMA)) {
            auto end_str = m_lexer.require(token_type::STRING).parse_string();
            if (!end_str.empty()) {
                m_code.add_line<opcode::PUSHVIEW>();
                m_code.add_line<opcode::PUSHSTR>(std::move(end_str));
                if (begin_len != 0) {
                    m_code.add_line<opcode::PUSHINT>(begin_len);
                    m_code.add_line<opcode::CALL>("indexof", 3);
                } else {
                    m_code.add_line<opcode::CALL>("indexof", 2);
                }
                m_code.add_line<opcode::SETEND>();
            }
        }
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        m_code.add_line<opcode::RESETVIEW>();
        break;
    }
    case hash("step"): {
        bool pushed_content = false;
        if (m_lexer.check_next(token_type::PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            ++m_content_level;
            m_code.add_line<opcode::ADDCONTENT>();
            pushed_content = true;
        }

        m_lexer.require(token_type::BRACE_BEGIN);
        m_code.add_line<opcode::SPLITVIEW>();
        
        while (true) {
            read_statement();
            if (! m_lexer.check_next(token_type::BRACE_END)) {
                m_code.add_line<opcode::NEXTRESULT>();
            } else {
                break;
            }
        }

        if (pushed_content) {
            --m_content_level;
            m_code.add_line<opcode::POPCONTENT>();
        } else {
            m_code.add_line<opcode::RESETVIEW>();
        }
        break;
    }
    case hash("newview"):
        m_code.add_line<opcode::NEWVIEW>();
        read_statement();
        m_code.add_line<opcode::RESETVIEW>();
        break;
    case hash("import"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(token_type::STRING);
        m_lexer.require(token_type::PAREN_END);
        m_lexer.require(token_type::SEMICOLON);
        auto imported_file = m_path.parent_path() / (tok_layout_name.parse_string() + ".bls");
        if (m_flags & parser_flags::RECURSIVE_IMPORTS) {
            parser imported;
            imported.m_flags = m_flags;
            imported.read_layout(imported_file, layout_box_list::from_file(imported_file));
            auto code_len = m_code.size();
            std::ranges::move(imported.m_code, std::back_inserter(m_code));
        } else {
            m_code.add_line<opcode::IMPORT>(imported_file.string());
        }
        break;
    }
    case hash("break"):
    case hash("continue"):
        m_lexer.require(token_type::SEMICOLON);
        if (m_loop_labels.empty()) {
            throw parsing_error("Non in un loop", tok_name);
        }
        m_code.add_line<opcode::JMP>(fun_name == "break" ? m_loop_labels.top().break_label : m_loop_labels.top().continue_label);
        break;
    case hash("return"):
        if (m_lexer.check_next(token_type::SEMICOLON)) {
            m_code.add_line<opcode::RET>();
        } else {
            read_expression();
            m_lexer.require(token_type::SEMICOLON);
            if (m_code.back().command() == opcode::PUSHVAR) {
                m_code.back() = make_command<opcode::RETVAR>();
            } else {
                m_code.add_line<opcode::RETVAL>();
            }
        }
        break;
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}