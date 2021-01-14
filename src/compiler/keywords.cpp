#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = tokens.require(TOK_FUNCTION);
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
            tokens.require(TOK_PAREN_BEGIN);
            read_expression();
            tokens.require(TOK_PAREN_END);
            add_line(condition_positive ? "JZ {0}" : "JNZ {0}", endif_label);
            read_statement();
            if (tokens.peek().type == TOK_FUNCTION) {
                fun_name = tokens.current().value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    tokens.advance();
                    has_endelse = true;
                    has_endif = true;
                    add_line("JMP {0}", endelse_label);
                    add_line("LABEL {0}", endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    tokens.advance();
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
        tokens.require(TOK_PAREN_BEGIN);
        add_line("LABEL {0}", while_label);
        read_expression();
        add_line("JZ {0}", endwhile_label);
        tokens.require(TOK_PAREN_END);
        read_statement();
        add_line("JMP {0}", while_label);
        add_line("LABEL {0}", endwhile_label);
        break;
    }
    case hash("for"):
    {
        std::string for_label = fmt::format("__for_{0}", output_asm.size());
        std::string endfor_label = fmt::format("__endfor_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        read_statement();
        tokens.require(TOK_COMMA);
        add_line("LABEL {0}", for_label);
        read_expression();
        add_line("JZ {0}", endfor_label);
        tokens.require(TOK_COMMA);
        size_t increase_stmt_begin = output_asm.size();
        read_statement();
        size_t increase_stmt_end = output_asm.size();
        tokens.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(output_asm, increase_stmt_begin, increase_stmt_end);
        add_line("JMP {0}", for_label);
        add_line("LABEL {0}", endfor_label);
        break;
    }
    case hash("goto"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        add_line("JMP {0}", tokens.require(TOK_IDENTIFIER).value);
        tokens.require(TOK_PAREN_END);
        break;
    }
    case hash("lines"):
    {
        std::string lines_label = fmt::format("__lines_{0}", output_asm.size());
        std::string endlines_label = fmt::format("__endlines_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("MOVCONTENT");
        add_line("LABEL {0}", lines_label);
        add_line("NEXTLINE");
        add_line("JTE {0}", endlines_label);
        read_statement();
        add_line("JMP {0}", lines_label);
        add_line("LABEL {0}", endlines_label);
        add_line("POPCONTENT");
        break;
    }
    case hash("with"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("MOVCONTENT");
        read_statement();
        add_line("POPCONTENT");
        break;
    }
    case hash("tokens"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("MOVCONTENT");

        tokens.require(TOK_BRACE_BEGIN);

        while (!tokens.check_next(TOK_BRACE_END)) {
            add_line("NEXTTOKEN");

            read_statement();
        }
        add_line("POPCONTENT");
        break;
    }
    case hash("setbegin"):
    case hash("setend"):
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line(fun_name == "setbegin" ? "SETBEGIN" : "SETEND");
        break;
    case hash("clear"):
        tokens.require(TOK_PAREN_BEGIN);
        read_variable(true);
        tokens.require(TOK_PAREN_END);
        add_line("CLEAR");
        break;
    case hash("newvalues"):
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("NEWVALUES");
        break;
    case hash("error"):
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("THROWERR");
        break;
    case hash("warning"):
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("ADDWARNING");
        break;
    case hash("skip"):
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        break;
    case hash("halt"):
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("HLT");
        break;
    default:
        throw parsing_error(fmt::format("Parola chiave sconosciuta: {0}", fun_name), tokens.getLocation(tok_name));
    }
}