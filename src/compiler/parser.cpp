#include "parser.h"

#include <iostream>
#include "../shared/utils.h"
#include "../shared/bytecode.h"

void parser::read_layout(const bill_layout_script &layout) {
    try {
        for (size_t i=0; i<layout.boxes.size(); ++i) {
            read_box(layout.boxes[i]);
        }
        add_line("HLT");
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("In {0}: {1}\n{2}", current_box->name, error.message, error.line));
    }
}

void parser::read_box(const layout_box &box) {
    current_box = &box;
    if (!box.goto_label.empty()) add_line("{0}:", box.goto_label);

    tokens = tokenizer(box.spacers);

    while(!tokens.ate()) {
        auto tok_num = tokens.require(TOK_IDENTIFIER);
        tokens.next();
        bool negative = false;
        switch (tokens.current().type) {
        case TOK_PLUS:
            break;
        case TOK_MINUS:
            negative = true;
            break;
        default:
            throw parsing_error{"Spaziatore incorretto", tokens.getLocation(tokens.current())};
        }
        read_expression();
        if (negative) add_line("NEG");
        switch (hash(string_tolower(std::string(tok_num.value)))) {
        case hash("p"):
        case hash("page"):
            add_line("MVBOX {0}", SPACER_PAGE);
            break;
        case hash("x"): add_line("MVBOX {0}", SPACER_X); break;
        case hash("y"): add_line("MVBOX {0}", SPACER_Y); break;
        case hash("w"):
        case hash("width"):
            add_line("MVBOX {0}", SPACER_W);
            break;
        case hash("h"):
        case hash("height"):
            add_line("MVBOX {0}", SPACER_H);
            break;
        default:
            throw parsing_error{"Spaziatore incorretto", tokens.getLocation(tok_num)};
        }
    }

    tokens = tokenizer(box.script);
    switch (box.type) {
    case BOX_DISABLED:
        break;
    case BOX_RECTANGLE:
        add_line("RDBOX {0},{1},{2},{3},{4},{5}", box.mode, box.page, box.x, box.y, box.w, box.h);
        break;
    case BOX_PAGE:
        add_line("RDPAGE {0},{1}", box.mode, box.page);
        break;
    case BOX_FILE:
        add_line("RDFILE {0}", box.mode);
        break;
    }
    while (!tokens.ate()) read_statement();
}

void parser::read_statement() {
    auto add_value = [&](int flags) {
        if (flags & VAR_NUMBER) {
            add_line("PARSENUM");
        }

        if (flags & VAR_DEBUG) {
            add_line("SETDEBUG");
        }

        if (flags & VAR_APPEND) {
            add_line("APPEND");
        } else if (flags & VAR_CLEAR) {
            add_line("RESETVAR");
        } else {
            add_line("SETVAR");
        }
    };

    tokens.peek();
    switch (tokens.current().type) {
    case TOK_BRACE_BEGIN:
        tokens.advance();
        while (true) {
            tokens.peek();
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            read_statement();
        }
        break;
    case TOK_COMMENT:
        tokens.advance();
        break;
    case TOK_FUNCTION:
        read_function();
        break;
    default:
    {
        int flags = read_variable();
        tokens.peek();
        switch (tokens.current().type) {
        case TOK_EQUALS:
            tokens.advance();
            read_expression();
            add_value(flags);
            break;
        case TOK_END_OF_FILE:
        default:
            add_line("COPYCONTENT");
            add_value(flags);
            break;
        }
    }
    }
}

void parser::read_expression() {
    tokens.peek();
    switch (tokens.current().type) {
    case TOK_FUNCTION:
        read_function();
        break;
    case TOK_NUMBER:
        tokens.advance();
        if (tokens.current().value.find('.') == std::string::npos) {
            add_line("PUSHINT {0}", tokens.current().value);
        } else {
            add_line("PUSHFLOAT {0}", tokens.current().value);
        }
        break;
    case TOK_STRING:
        tokens.advance();
        add_line("PUSHSTR {0}", tokens.current().value);
        break;
    case TOK_CONTENT:
        tokens.advance();
        add_line("COPYCONTENT");
        break;
    default:
        read_variable();
        add_line("PUSHVAR");
    }
}

int parser::read_variable() {
    int flags = 0;
    bool isglobal = false;
    bool getindex = false;
    int index = 0;

    bool in_loop = true;
    while (in_loop && tokens.next()) {
        switch (tokens.current().type) {
        case TOK_GLOBAL:
            isglobal = true;
            break;
        case TOK_DEBUG:
            flags |= VAR_DEBUG;
            break;
        case TOK_PERCENT:
            flags |= VAR_NUMBER;
            break;
        default:
            in_loop = false;
        }
    }

    if (tokens.current().type != TOK_IDENTIFIER) {
        throw tokens.unexpected_token(TOK_IDENTIFIER);
    }
    
    std::string name(tokens.current().value);
    if (isglobal) {
        add_line("SELGLOBAL {0}", name);
        return flags;
    }

    tokens.peek();
    switch (tokens.current().type) {
    case TOK_BRACKET_BEGIN:
        tokens.advance();
        tokens.peek();
        switch (tokens.current().type) {
        case TOK_NUMBER:
            tokens.advance();
            index = std::stoi(std::string(tokens.current().value));
            break;
        case TOK_BRACKET_END:
            flags |= VAR_APPEND;
            break;
        default:
            read_expression();
            getindex = true;
        }
        tokens.require(TOK_BRACKET_END);
        break;
    case TOK_CLEAR:
        flags |= VAR_CLEAR;
        tokens.advance();
        break;
    default:
        break;
    }

    if (getindex) {
        add_line("SELVAR {0}", name);
    } else {
        add_line("SELVARIDX {0},{1}", name, index);
    }
    return flags;
}

void parser::read_function() {
    tokens.require(TOK_FUNCTION);
    tokens.require(TOK_IDENTIFIER);
    auto tok_fun_name = tokens.current();
    std::string fun_name = std::string(tok_fun_name.value);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"):
    {
        std::string else_label = fmt::format("__else_{0}", output_asm.size());
        std::string endif_label = fmt::format("__endif_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        read_expression();
        tokens.require(TOK_PAREN_END);
        add_line(fun_name == "if" ? "JZ {0}" : "JNZ {0}", else_label);
        read_statement();
        tokens.next();
        token tok_else = tokens.current();
        bool has_else = false;
        if (tokens.current().type == TOK_FUNCTION) {
            tokens.next();
            if (tokens.current().value == "else") {
                has_else = true;
            }
        }
        if (has_else) {
            add_line("JMP {0}", endif_label);
            add_line("{0}:", else_label);
            read_statement();
            add_line("{0}:", endif_label);
        } else {
            add_line("{0}:", else_label);
            tokens.gotoTok(tok_else);
        }
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
        tokens.next();
        switch (tokens.current().type) {
        case TOK_IDENTIFIER:
        case TOK_NUMBER:
            add_line("JMP {0}", tokens.current().value);
            break;
        default:
            throw parsing_error{"Indirizzo goto invalido", tokens.getLocation(tokens.current())};
        }
        tokens.require(TOK_PAREN_END);
        break;
    }
    case hash("inc"):
    case hash("dec"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        int flags = read_variable();
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
            add_line(fun_name == "inc" ? "INC {0}" : "DEC {0}", amount);
        } else {
            if (flags & VAR_NUMBER) {
                add_line("PARSENUM");
            }
            add_line(fun_name == "inc" ? "INCTOP" : "DECTOP");
        }
        break;
    }
    case hash("isset"):
        tokens.require(TOK_PAREN_BEGIN);
        read_variable();
        tokens.require(TOK_PAREN_END);
        add_line("ISSET");
        break;
    case hash("size"):
        tokens.require(TOK_PAREN_BEGIN);
        read_variable();
        tokens.require(TOK_PAREN_END);
        add_line("SIZE");
        break;
    case hash("clear"):
        tokens.require(TOK_PAREN_BEGIN);
        read_variable();
        tokens.require(TOK_PAREN_END);
        add_line("CLEAR");
        break;
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
    case hash("error"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        auto tok = tokens.require(TOK_STRING);
        tokens.require(TOK_PAREN_END);

        add_line("ERROR {0}", tok.value);
        break;
    }
    case hash("nextpage"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("NEXTPAGE");
        break;
    }
    case hash("ate"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("ATE");
        break;
    }
    case hash("boxwidth"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("PUSHFLOAT {0}", current_box->w);
        break;
    }
    case hash("boxheight"):
    {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line("PUSHFLOAT {0}", current_box->h);
        break;
    }
    default:
    {
        int num_args = 0;
        tokens.require(TOK_PAREN_BEGIN);
        bool in_fun_loop = true;
        while (in_fun_loop) {
            tokens.peek();
            if (tokens.current().type == TOK_PAREN_END) {
                tokens.advance();
                break;
            }
            ++num_args;
            read_expression();
            tokens.next();
            switch (tokens.current().type) {
            case TOK_COMMA:
                break;
            case TOK_PAREN_END:
                in_fun_loop=false;
                break;
            default:
                throw tokens.unexpected_token(TOK_PAREN_END);
            }
        }
        auto check_args = [&](int should_be) {
            if (num_args != should_be) {
                throw parsing_error{fmt::format("La funzione {0} richiede {1} argomenti", fun_name, should_be), tokens.getLocation(tok_fun_name)};
            }
        };
        switch (hash(fun_name)) {
        case hash("num"): check_args(1); add_line("PARSENUM"); break;
        case hash("int"): check_args(1); add_line("PARSEINT"); break;
        case hash("eq"):  check_args(2); add_line("EQ"); break;
        case hash("neq"): check_args(2); add_line("NEQ"); break;
        case hash("and"): check_args(2); add_line("AND"); break;
        case hash("or"):  check_args(2); add_line("OR"); break;
        case hash("not"): check_args(1); add_line("NOT"); break;
        case hash("add"): check_args(2); add_line("ADD"); break;
        case hash("sub"): check_args(2); add_line("SUB"); break;
        case hash("mul"): check_args(2); add_line("MUL"); break;
        case hash("div"): check_args(2); add_line("DIV"); break;
        case hash("gt"):  check_args(2); add_line("GT"); break;
        case hash("lt"):  check_args(2); add_line("LT"); break;
        case hash("geq"): check_args(2); add_line("GEQ"); break;
        case hash("leq"): check_args(2); add_line("LEQ"); break;
        case hash("max"): check_args(2); add_line("MAX"); break;
        case hash("min"): check_args(2); add_line("MIN"); break;
        default:
            add_line("CALL {0},{1}", fun_name,num_args);
        }
    }
    }
}