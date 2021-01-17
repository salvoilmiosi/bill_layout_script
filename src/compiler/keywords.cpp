#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"):
    {
        std::string endelse_label = fmt::format("__endelse_{0}", output_asm.size());
        std::string endif_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool has_endelse = false;
        while (in_loop) {
            bool has_endif = false;
            endif_label = fmt::format("__endif_{0}", output_asm.size());
            m_lexer.require(TOK_PAREN_BEGIN);
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line(condition_positive ? "JZ {0}" : "JNZ {0}", endif_label);
            read_statement();
            if (m_lexer.peek().type == TOK_FUNCTION) {
                fun_name = m_lexer.current().value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance();
                    has_endelse = true;
                    has_endif = true;
                    add_line("JMP {0}", endelse_label);
                    add_line("LABEL {0}", endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance();
                    condition_positive = fun_name == "elif";
                    has_endelse = true;
                    add_line("JMP {0}", endelse_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_endif) add_line("LABEL {0}", endif_label);
        }
        if (has_endelse) add_line("LABEL {0}", endelse_label);
        break;
    }
    case hash("while"):
    {
        std::string while_label = fmt::format("__while_{0}", output_asm.size());
        std::string endwhile_label = fmt::format("__endwhile_{0}", output_asm.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line("LABEL {0}", while_label);
        read_expression();
        add_line("JZ {0}", endwhile_label);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line("JMP {0}", while_label);
        add_line("LABEL {0}", endwhile_label);
        break;
    }
    case hash("for"):
    {
        std::string for_label = fmt::format("__for_{0}", output_asm.size());
        std::string endfor_label = fmt::format("__endfor_{0}", output_asm.size());
        m_lexer.require(TOK_PAREN_BEGIN);
        read_statement();
        m_lexer.require(TOK_COMMA);
        add_line("LABEL {0}", for_label);
        read_expression();
        add_line("JZ {0}", endfor_label);
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = output_asm.size();
        read_statement();
        size_t increase_stmt_end = output_asm.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(output_asm, increase_stmt_begin, increase_stmt_end);
        add_line("JMP {0}", for_label);
        add_line("LABEL {0}", endfor_label);
        break;
    }
    case hash("goto"):
    {
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line("JMP {0}", m_lexer.require(TOK_IDENTIFIER).value);
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("lines"):
    {
        std::string lines_label = fmt::format("__lines_{0}", output_asm.size());
        std::string endlines_label = fmt::format("__endlines_{0}", output_asm.size());
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line("MOVCONTENT");
            pushed_content = true;
        }
        add_line("NEWTOKENS");
        add_line("LABEL {0}", lines_label);
        add_line("NEXTLINE");
        add_line("JTE {0}", endlines_label);
        read_statement();
        add_line("JMP {0}", lines_label);
        add_line("LABEL {0}", endlines_label);
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
        add_line("HLT");
        break;
    default:
        throw parsing_error(fmt::format("Parola chiave sconosciuta: {0}", fun_name), tok_name);
    }
}