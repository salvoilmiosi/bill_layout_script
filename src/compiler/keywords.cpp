#include "parser.h"

#include "utils.h"

void parser::read_keyword() {
    auto tok_name = tokens.require(TOK_FUNCTION);
    std::string name(tok_name.value.substr(1));

    switch(hash(name)) {
    case hash("if"):
    case hash("ifnot"):
    {
        std::string endelse_label = fmt::format("__endelse_{0}", output_asm.size());
        std::string endif_label;
        bool condition_positive = name == "if";
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
            tokens.peek();
            if (tokens.current().type == TOK_FUNCTION) {
                switch (hash(std::string(tokens.current().value.substr(1)))) {
                case hash("else"):
                    tokens.next();
                    has_endelse = true;
                    has_endif = true;
                    add_line("JMP {0}", endelse_label);
                    add_line("{0}:", endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    tokens.next();
                    condition_positive = tokens.current().value.substr(1) == "elif";
                    has_endelse = true;
                    add_line("JMP {0}", endelse_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_endif) add_line("{0}:", endif_label);
        }
        if (has_endelse) add_line("{0}:", endelse_label);
        break;
    }
    case hash("while"):
    {
        std::string while_label = fmt::format("__while_{0}", output_asm.size());
        std::string endwhile_label = fmt::format("__endwhile_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        add_line("{0}:", while_label);
        read_expression();
        add_line("JZ {0}", endwhile_label);
        tokens.require(TOK_PAREN_END);
        read_statement();
        add_line("JMP {0}", while_label);
        add_line("{0}:", endwhile_label);
        break;
    }
    case hash("for"):
    {
        std::string for_label = fmt::format("__for_{0}", output_asm.size());
        std::string endfor_label = fmt::format("__endfor_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        auto idx_name = tokens.require(TOK_IDENTIFIER);
        tokens.require(TOK_COMMA);
        add_line("{0}:", for_label);
        add_line("SELVARIDX {0},0", idx_name.value);
        add_line("PUSHVAR");
        read_expression();
        add_line("LT");
        add_line("JZ {0}", endfor_label);
        tokens.require(TOK_PAREN_END);
        read_statement();
        add_line("SELVARIDX {0},0", idx_name.value);
        add_line("INC 1");
        add_line("JMP {0}", for_label);
        add_line("{0}:", endfor_label);
        add_line("SELVARIDX {0},0", idx_name.value);
        add_line("CLEAR", idx_name.value);
        break;
    }
    case hash("goto"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_IDENTIFIER);
        add_line("JMP {0}", tokens.current().value);
        tokens.require(TOK_PAREN_END);
        break;
    }
    case hash("inc"):
    case hash("dec"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        int flags = read_variable(false);
        tokens.next();
        bool inc_amt = true;
        std::string amount = "1";
        switch(tokens.current().type) {
        case TOK_COMMA:
            tokens.peek();
            if (tokens.current().type == TOK_NUMBER) {
                tokens.advance();
                inc_amt = true;
                amount = tokens.current().value;
            } else {
                read_expression();
                inc_amt = false;
            }
            tokens.require(TOK_PAREN_END);
            break;
        case TOK_PAREN_END:
            break;
        default:
            throw tokens.unexpected_token(TOK_PAREN_END);
        }
        if (inc_amt) {
            add_line(name == "inc" ? "INC {0}" : "DEC {0}", amount);
        } else {
            if (flags & VAR_NUMBER) {
                add_line("PARSENUM");
            }
            add_line(name == "inc" ? "INCTOP" : "DECTOP");
        }
        break;
    }
    case hash("lines"):
    {
        std::string lines_label = fmt::format("__lines_{0}", output_asm.size());
        std::string endlines_label = fmt::format("__endlines_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("PUSHCONTENT");
        add_line("{0}:", lines_label);
        add_line("NEXTLINE");
        add_line("JTE {0}", endlines_label);
        read_statement();
        add_line("JMP {0}", lines_label);
        add_line("{0}:", endlines_label);
        add_line("POPCONTENT");
        break;
    }
    case hash("with"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("PUSHCONTENT");
        read_statement();
        add_line("POPCONTENT");
        break;
    }
    case hash("tokens"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("PUSHCONTENT");

        tokens.require(TOK_BRACE_BEGIN);

        while (true) {
            tokens.peek();
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            add_line("NEXTTOKEN");
            read_statement();
        }
        add_line("POPCONTENT");
        break;
    }
    case hash("clear"):
        tokens.require(TOK_PAREN_BEGIN);
        read_variable(true);
        tokens.require(TOK_PAREN_END);
        add_line("CLEAR");
        break;
    case hash("nextpage"):
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("NEXTPAGE");
        break;
    case hash("error"):
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line("ERROR");
        break;
    default:
        throw parsing_error(fmt::format("Parola chiave sconosciuta: {0}", name), tokens.getLocation(tok_name));
    }
}