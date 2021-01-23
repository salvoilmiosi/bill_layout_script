#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"):
    {
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
            add_line(condition_positive ? opcode::JZ : opcode::JNZ, endif_label);
            read_statement();
            if (m_lexer.peek().type == TOK_FUNCTION) {
                fun_name = m_lexer.current().value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance();
                    has_endelse = true;
                    has_endif = true;
                    add_line(opcode::JMP, endelse_label);
                    add_label(endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance();
                    condition_positive = fun_name == "elif";
                    has_endelse = true;
                    add_line(opcode::JMP, endelse_label);
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
    case hash("while"):
    {
        std::string while_label = fmt::format("__while_{}", m_code.size());
        std::string endwhile_label = fmt::format("__endwhile_{}", m_code.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        add_label(while_label);
        read_expression();
        add_line(opcode::JZ, endwhile_label);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line(opcode::JMP, while_label);
        add_label(endwhile_label);
        break;
    }
    case hash("for"):
    {
        std::string for_label = fmt::format("__for_{}", m_code.size());
        std::string endfor_label = fmt::format("__endfor_{}", m_code.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        read_statement();
        m_lexer.require(TOK_COMMA);
        add_label(for_label);
        read_expression();
        add_line(opcode::JZ, endfor_label);
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = m_code.size();
        read_statement();
        size_t increase_stmt_end = m_code.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(m_code, increase_stmt_begin, increase_stmt_end);
        add_line(opcode::JMP, for_label);
        add_label(endfor_label);
        break;
    }
    case hash("goto"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line(opcode::JMP, std::string(m_lexer.require(TOK_IDENTIFIER).value));
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("lines"):
    {
        std::string lines_label = fmt::format("__lines_{}", m_code.size());
        std::string endlines_label = fmt::format("__endlines_{}", m_code.size());
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(opcode::MOVCONTENT);
            pushed_content = true;
        }
        add_line(opcode::NEWTOKENS);
        add_label(lines_label);
        add_line(opcode::NEXTLINE);
        add_line(opcode::JTE, endlines_label);
        read_statement();
        add_line(opcode::JMP, lines_label);
        add_label(endlines_label);
        if (pushed_content) {
            add_line(opcode::POPCONTENT);
        } else {
            add_line(opcode::RESETVIEW);
        }
        break;
    }
    case hash("with"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::MOVCONTENT);
        read_statement();
        add_line(opcode::POPCONTENT);
        break;
    }
    case hash("between"):
    {
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
    case hash("tokens"):
    {
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(opcode::MOVCONTENT);
            pushed_content = true;
        }

        add_line(opcode::NEWTOKENS);
        
        m_lexer.require(TOK_BRACE_BEGIN);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            add_line(opcode::NEXTTOKEN);
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
    case hash("halt"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line(opcode::RET);
        break;
    case hash("import"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(TOK_STRING);
        m_lexer.require(TOK_PAREN_END);
        std::string layout_name;
        if (!parse_string_token(layout_name, tok_layout_name.value)) {
            throw parsing_error("Costante stringa non valida", tok_layout_name);
        }
        add_line(opcode::IMPORT, layout_name);
        break;
    }
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}