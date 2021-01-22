#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"):
    {
        std::string endelse_label = fmt::format("__endelse_{}", m_lines.size());
        std::string endif_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool has_endelse = false;
        while (in_loop) {
            bool has_endif = false;
            endif_label = fmt::format("__endif_{}", m_lines.size());
            m_lexer.require(TOK_PAREN_BEGIN);
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(condition_positive ? "JZ {}" : "JNZ {}", endif_label);
            read_statement();
            if (m_lexer.peek().type == TOK_FUNCTION) {
                fun_name = m_lexer.current().value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance();
                    has_endelse = true;
                    has_endif = true;
                    add_line("JMP {}", endelse_label);
                    add_line("LABEL {}", endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance();
                    condition_positive = fun_name == "elif";
                    has_endelse = true;
                    add_line("JMP {}", endelse_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_endif) add_line("LABEL {}", endif_label);
        }
        if (has_endelse) add_line("LABEL {}", endelse_label);
        break;
    }
    case hash("while"):
    {
        std::string while_label = fmt::format("__while_{}", m_lines.size());
        std::string endwhile_label = fmt::format("__endwhile_{}", m_lines.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line("LABEL {}", while_label);
        read_expression();
        add_line("JZ {}", endwhile_label);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line("JMP {}", while_label);
        add_line("LABEL {}", endwhile_label);
        break;
    }
    case hash("for"):
    {
        std::string for_label = fmt::format("__for_{}", m_lines.size());
        std::string endfor_label = fmt::format("__endfor_{}", m_lines.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        read_statement();
        m_lexer.require(TOK_COMMA);
        add_line("LABEL {}", for_label);
        read_expression();
        add_line("JZ {}", endfor_label);
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = m_lines.size();
        read_statement();
        size_t increase_stmt_end = m_lines.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(m_lines, increase_stmt_begin, increase_stmt_end);
        add_line("JMP {}", for_label);
        add_line("LABEL {}", endfor_label);
        break;
    }
    case hash("goto"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line("JMP {}", m_lexer.require(TOK_IDENTIFIER).value);
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("lines"):
    {
        std::string lines_label = fmt::format("__lines_{}", m_lines.size());
        std::string endlines_label = fmt::format("__endlines_{}", m_lines.size());
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line("MOVCONTENT");
            pushed_content = true;
        }
        add_line("NEWTOKENS");
        add_line("LABEL {}", lines_label);
        add_line("NEXTLINE");
        add_line("JTE {}", endlines_label);
        read_statement();
        add_line("JMP {}", lines_label);
        add_line("LABEL {}", endlines_label);
        if (pushed_content) {
            add_line("POPCONTENT");
        } else {
            add_line("RESETVIEW");
        }
        break;
    }
    case hash("with"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line("MOVCONTENT");
        read_statement();
        add_line("POPCONTENT");
        break;
    }
    case hash("between"):
    {
        add_line("NEWVIEW");
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line("PUSHVIEW");
        read_expression();
        add_line("CALL indexof,2");
        add_line("SETBEGIN");
        m_lexer.require(TOK_COMMA);
        add_line("PUSHVIEW");
        read_expression();
        add_line("CALL indexof,2");
        add_line("SETEND");
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line("RESETVIEW");
        break;
    }
    case hash("tokens"):
    {
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line("MOVCONTENT");
            pushed_content = true;
        }

        add_line("NEWTOKENS");
        
        m_lexer.require(TOK_BRACE_BEGIN);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            add_line("NEXTTOKEN");
            read_statement();
        }

        if (pushed_content) {
            add_line("POPCONTENT");
        } else {
            add_line("RESETVIEW");
        }
        break;
    }
    case hash("setbegin"):
    case hash("setend"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(fun_name == "setbegin" ? "SETBEGIN" : "SETEND");
        break;
    case hash("newview"):
        add_line("NEWVIEW");
        read_statement();
        add_line("RESETVIEW");
        break;
    case hash("clear"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_variable(true);
        m_lexer.require(TOK_PAREN_END);
        add_line("CLEAR");
        break;
    case hash("nexttable"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line("NEXTTABLE");
        break;
    case hash("error"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line("THROWERR");
        break;
    case hash("warning"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line("ADDWARNING");
        break;
    case hash("skip"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        break;
    case hash("halt"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line("RET");
        break;
    case hash("import"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(TOK_STRING);
        m_lexer.require(TOK_PAREN_END);
        std::string layout_name;
        parse_string_token(layout_name, tok_layout_name.value);
        add_line("IMPORT {}", layout_name);
        break;
    }
    default:
        throw parsing_error(fmt::format("Parola chiave sconosciuta: {}", fun_name), tok_name);
    }
}