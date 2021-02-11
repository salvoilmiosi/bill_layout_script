#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"): {
        std::string endelse_label = fmt::format("__endelse_{}", m_code.size());
        std::string endif_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool has_endelse = false;
        while (in_loop) {
            bool has_endif = false;
            endif_label = fmt::format("__endif_{}", m_code.size());
            m_lexer.require(TOK_PAREN_BEGIN);
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(condition_positive ? opcode::JZ : opcode::JNZ, jump_address{endif_label, 0});
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == TOK_FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance(tok_if);
                    has_endelse = true;
                    has_endif = true;
                    add_line(opcode::JMP, jump_address{endelse_label, 0});
                    add_label(endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    has_endelse = true;
                    add_line(opcode::JMP, jump_address{endelse_label, 0});
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_endif) add_label(endif_label);
        }
        if (has_endelse) add_label(endelse_label);
        break;
    }
    case hash("while"): {
        std::string while_label = fmt::format("__while_{}", m_code.size());
        std::string endwhile_label = fmt::format("__endwhile_{}", m_code.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        add_label(while_label);
        read_expression();
        add_line(opcode::JZ, jump_address{endwhile_label, 0});
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line(opcode::JMP, jump_address{while_label, 0});
        add_label(endwhile_label);
        break;
    }
    case hash("for"): {
        std::string for_label = fmt::format("__for_{}", m_code.size());
        std::string endfor_label = fmt::format("__endfor_{}", m_code.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        read_statement();
        m_lexer.require(TOK_COMMA);
        add_label(for_label);
        read_expression();
        add_line(opcode::JZ, jump_address{endfor_label, 0});
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = m_code.size();
        read_statement();
        size_t increase_stmt_end = m_code.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(m_code, increase_stmt_begin, increase_stmt_end);
        add_line(opcode::JMP, jump_address{for_label, 0});
        add_label(endfor_label);
        break;
    }
    case hash("goto"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line(opcode::JMP, jump_address{fmt::format("__label_{}", tok.value), 0});
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("function"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto name = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

        add_line(opcode::JMP, jump_address{endfun_label, 0});
        add_label(fun_label);
        read_statement();
        add_line(opcode::RET);
        add_label(endfun_label);
        break;
    }
    case hash("call"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line(opcode::JSR, jump_address{fmt::format("__function_{}", tok.value), 0});
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("foreach"): {
        std::string lines_label = fmt::format("__lines_{}", m_code.size());
        std::string endlines_label = fmt::format("__endlines_{}", m_code.size());
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(opcode::MOVCONTENT);
            pushed_content = true;
        }
        add_line(opcode::SUBVIEW);
        add_label(lines_label);
        add_line(opcode::NEXTRESULT);
        add_line(opcode::JTE, jump_address{endlines_label, 0});
        read_statement();
        add_line(opcode::JMP, jump_address{lines_label, 0});
        add_label(endlines_label);
        if (pushed_content) {
            add_line(opcode::POPCONTENT);
        } else {
            add_line(opcode::RESETVIEW);
        }
        break;
    }
    case hash("with"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::MOVCONTENT);
        read_statement();
        add_line(opcode::POPCONTENT);
        break;
    }
    case hash("between"): {
        add_line(opcode::NEWVIEW);
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line(opcode::PUSHVIEW);
        read_expression();
        add_line(opcode::CALL, command_call{"indexof", 2});
        add_line(opcode::SETBEGIN);
        m_lexer.require(TOK_COMMA);
        add_line(opcode::PUSHVIEW);
        read_expression();
        add_line(opcode::CALL, command_call{"indexof", 2});
        add_line(opcode::SETEND);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line(opcode::RESETVIEW);
        break;
    }
    case hash("step"): {
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(opcode::MOVCONTENT);
            pushed_content = true;
        }

        add_line(opcode::SUBVIEW);
        
        m_lexer.require(TOK_BRACE_BEGIN);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            add_line(opcode::NEXTRESULT);
            read_statement();
        }

        if (pushed_content) {
            add_line(opcode::POPCONTENT);
        } else {
            add_line(opcode::RESETVIEW);
        }
        break;
    }
    case hash("setbegin"):
    case hash("setend"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(fun_name == "setbegin" ? opcode::SETBEGIN : opcode::SETEND);
        break;
    case hash("newview"):
        add_line(opcode::NEWVIEW);
        read_statement();
        add_line(opcode::RESETVIEW);
        break;
    case hash("clear"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_variable(true);
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::CLEAR);
        break;
    case hash("nexttable"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::NEXTTABLE);
        break;
    case hash("error"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::THROWERR);
        break;
    case hash("warning"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::ADDWARNING);
        break;
    case hash("skip"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        break;
    case hash("return"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::RET);
        break;
    case hash("import"):
    case hash("setlayout"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(TOK_STRING);
        m_lexer.require(TOK_PAREN_END);
        auto imported_file = std::filesystem::canonical(m_layout->m_filename.parent_path() / (tok_layout_name.parse_string() + ".bls"));
        if (fun_name == "import") {
            add_line(opcode::IMPORT, imported_file);
            if (intl::valid_language(m_layout->language_code)) {
                add_line(opcode::SETLANG, m_layout->language_code);
            }
        } else {
            add_line(opcode::SETLAYOUT, imported_file);
            add_line(opcode::HLT);
        }
        break;
    }
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}