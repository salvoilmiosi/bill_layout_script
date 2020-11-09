#include "parser.h"

#include <fmt/format.h>
#include <iostream>

void parser::read_layout(const bill_layout_script &layout) {
    try {
        for (size_t i=0; i<layout.boxes.size(); ++i) {
            read_box(layout.boxes[i]);
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("In {0}: {1}\n{2}", current_box->name, error.message, error.line));
    }
}

void parser::read_box(const layout_box &box) {
    current_box = &box;

    tokens = tokenizer(box.script);
    if (!box.goto_label.empty()) output_asm.push_back(fmt::format("{0}:", box.goto_label));
    output_asm.push_back(fmt::format("RDBOX {0},{1},{2},{3},{4},{5},{6},{7}", box.x, box.y, box.w, box.h, box.page, box.mode, box.type, box.spacers));
    while (!tokens.ate()) exec_line();
}

void parser::add_value(variable_ref ref) {
    if (ref.flags & FLAGS_NUMBER) {
        output_asm.push_back("CALL num,1");
    }

    if (ref.flags & FLAGS_GLOBAL) {
        output_asm.push_back(fmt::format("SETGLOBAL {0}", ref.name));
        return;
    }

    if (ref.flags & FLAGS_DEBUG) {
        output_asm.push_back("SETDEBUG");
    }

    if (ref.flags & FLAGS_CLEAR) {
        output_asm.push_back(fmt::format("CLEAR {0}", ref.name));
    }

    if (ref.flags & FLAGS_APPEND) {
        output_asm.push_back(fmt::format("APPEND {0}", ref.name));
    } else {
        output_asm.push_back(fmt::format("SETVAR {0}", ref.name));
    }
}

void parser::exec_line() {
    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_BRACE_BEGIN:
        tokens.advance();
        while (true) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            exec_line();
        }
        break;
    case TOK_COMMENT:
        tokens.advance();
        break;
    case TOK_FUNCTION:
        exec_function();
        break;
    default:
    {
        variable_ref ref = get_variable();
        tokens.next(true);
        switch (tokens.current().type) {
        case TOK_EQUALS:
            tokens.advance();
            evaluate();
            add_value(ref);
            break;
        case TOK_END_OF_FILE:
        default:
            output_asm.push_back("PUSHCONTENT");
            add_value(ref);
            break;
        }
    }
    }
}

void parser::evaluate() {
    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_FUNCTION:
        exec_function();
        break;
    case TOK_NUMBER:
        tokens.advance();
        output_asm.push_back(fmt::format("PUSHNUM {0}", tokens.current().value));
        break;
    case TOK_STRING:
        tokens.advance();
        output_asm.push_back(fmt::format("PUSHSTR {0}", tokens.current().value));
        break;
    case TOK_CONTENT:
        tokens.advance();
        output_asm.push_back("PUSHCONTENT");
        break;
    default:
    {
        auto ref = get_variable();
        if (ref.flags & FLAGS_GLOBAL) {
            output_asm.push_back(fmt::format("PUSHGLOBAL {0}", ref.name));
        } else {
            output_asm.push_back(fmt::format("PUSHVAR {0}", ref.name));
        }
    }
    }
}

variable_ref parser::get_variable() {
    variable_ref ref;
    bool in_loop = true;
    while (in_loop && tokens.next()) {
        switch (tokens.current().type) {
        case TOK_GLOBAL:
            ref.flags |= FLAGS_GLOBAL;
            break;
        case TOK_DEBUG:
            ref.flags |= FLAGS_DEBUG;
            break;
        case TOK_PERCENT:
            ref.flags |= FLAGS_NUMBER;
            break;
        default:
            in_loop = false;
        }
    }

    if (tokens.current().type != TOK_IDENTIFIER) {
        throw parsing_error("Token imprevisto", tokens.getLocation(tokens.current()));
    }
    ref.name = tokens.current().value;
    if (ref.flags & FLAGS_GLOBAL) {
        return ref;
    }

    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_BRACKET_BEGIN:
        tokens.advance();
        evaluate();
        output_asm.push_back("SETINDEX");
        tokens.require(TOK_BRACKET_END);
        break;
    case TOK_APPEND:
        ref.flags |= FLAGS_APPEND;
        tokens.advance();
        break;
    case TOK_CLEAR:
        ref.flags |= FLAGS_CLEAR;
        tokens.advance();
        break;
    default:
        break;
    }

    return ref;
}

void parser::exec_function() {
    tokens.require(TOK_FUNCTION);
    tokens.require(TOK_IDENTIFIER);
    std::string fun_name = std::string(tokens.current().value);
    
    if (fun_name == "if") {
        std::string else_label = fmt::format("__else_{0}", output_asm.size());
        std::string endif_label = fmt::format("__endif_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        evaluate();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back(fmt::format("JZ {0}", else_label));
        exec_line();
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
            output_asm.push_back(fmt::format("JMP {0}", endif_label));
            output_asm.push_back(fmt::format("{0}:", else_label));
            exec_line();
            output_asm.push_back(fmt::format("{0}:", endif_label));
        } else {
            output_asm.push_back(fmt::format("{0}:", else_label));
            tokens.gotoTok(tok_else);
        }
    } else if (fun_name == "while") {
        std::string while_label = fmt::format("__while_{0}", output_asm.size());
        std::string endwhile_label = fmt::format("__endwhile_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        output_asm.push_back(fmt::format("{0}:", while_label));
        evaluate();
        output_asm.push_back(fmt::format("JZ {0}", endwhile_label));
        tokens.require(TOK_PAREN_END);
        exec_line();
        output_asm.push_back(fmt::format("JMP {0}", while_label));
        output_asm.push_back(fmt::format("{0}:", endwhile_label));
    } else if (fun_name == "for") {
        std::string for_label = fmt::format("__for_{0}", output_asm.size());
        std::string endfor_label = fmt::format("__endfor_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        auto idx_name = tokens.require(TOK_IDENTIFIER);
        tokens.require(TOK_COMMA);
        output_asm.push_back(fmt::format("{0}:", for_label));
        output_asm.push_back(fmt::format("PUSHVAR {0}", idx_name.value));
        evaluate();
        output_asm.push_back("CALL lt,2");
        output_asm.push_back(fmt::format("JZ {0}", endfor_label));
        tokens.require(TOK_PAREN_END);
        exec_line();
        output_asm.push_back(fmt::format("INC {0}", idx_name.value));
        output_asm.push_back(fmt::format("JMP {0}", for_label));
        output_asm.push_back(fmt::format("{0}:", endfor_label));
        output_asm.push_back(fmt::format("CLEAR {0}", idx_name.value));
    } else if (fun_name == "goto") {
        tokens.next();
   
        switch (tokens.current().type) {
        case TOK_IDENTIFIER:
            output_asm.push_back(fmt::format("JMP {0}", tokens.current().value));
            break;
        case TOK_NUMBER:
            output_asm.push_back(fmt::format("JMP {0}", tokens.current().value));
            break;
        default:
            throw parsing_error("Indirizzo goto invalido", tokens.getLocation(tokens.current()));
        }
    } else if (fun_name == "inc") {
        tokens.require(TOK_PAREN_BEGIN);
        auto ref = get_variable();
        tokens.next();
        bool inc_one = true;
        switch(tokens.current().type) {
        case TOK_COMMA:
            evaluate();
            inc_one = false;
            tokens.require(TOK_PAREN_END);
            break;
        case TOK_PAREN_END:
            break;
        default:
            throw parsing_error("Token inaspettato", tokens.getLocation(tokens.current()));
        }
        if (ref.flags & FLAGS_NUMBER) {
            output_asm.push_back("CALL num,1");
        }
        if (ref.flags & FLAGS_GLOBAL) {
            if (inc_one) {
                output_asm.push_back(fmt::format("INCG {0}", ref.name));
            } else {
                output_asm.push_back(fmt::format("INCGTOP {0}", ref.name));
            }
        } else {
            if (inc_one) {
                output_asm.push_back(fmt::format("INC {0}", ref.name));
            } else {
                output_asm.push_back(fmt::format("INCTOP {0}", ref.name));
            }
        }
    } else if (fun_name == "dec") {
        tokens.require(TOK_PAREN_BEGIN);
        auto ref = get_variable();
        tokens.next();
        bool inc_one = true;
        switch(tokens.current().type) {
        case TOK_COMMA:
            evaluate();
            inc_one = false;
            tokens.require(TOK_PAREN_END);
            break;
        case TOK_PAREN_END:
            break;
        default:
            throw parsing_error("Token inaspettato", tokens.getLocation(tokens.current()));
        }
        if (ref.flags & FLAGS_NUMBER) {
            output_asm.push_back("CALL num,1");
        }
        if (ref.flags & FLAGS_GLOBAL) {
            if (inc_one) {
                output_asm.push_back(fmt::format("DECG {0}", ref.name));
            } else {
                output_asm.push_back(fmt::format("DECGTOP {0}", ref.name));
            }
        } else {
            if (inc_one) {
                output_asm.push_back(fmt::format("DEC {0}", ref.name));
            } else {
                output_asm.push_back(fmt::format("DECTOP {0}", ref.name));
            }
        }
    } else if (fun_name == "isset") {
        tokens.require(TOK_PAREN_BEGIN);
        auto ref = get_variable();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back(fmt::format("ISSET {0}", ref.name));
    } else if (fun_name == "size") {
        tokens.require(TOK_PAREN_BEGIN);
        auto ref = get_variable();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back(fmt::format("SIZE {0}", ref.name));
    } else if (fun_name == "clear") {
        tokens.require(TOK_PAREN_BEGIN);
        auto ref = get_variable();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back(fmt::format("CLEAR {0}", ref.name));
    } else if (fun_name == "addspacer") {
        tokens.require(TOK_IDENTIFIER);
        output_asm.push_back(fmt::format("SPACER {0},{1},{2}", tokens.current().value, current_box->w, current_box->h));
    } else if (fun_name == "lines") {
        std::string lines_label = fmt::format("__lines_{0}", output_asm.size());
        std::string endlines_label = fmt::format("__endlines_{0}", output_asm.size());
        tokens.require(TOK_PAREN_BEGIN);
        evaluate();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back("CONTENTVIEW");

        tokens.require(TOK_BRACE_BEGIN);
        output_asm.push_back(fmt::format("{0}:", lines_label));
        output_asm.push_back("NEXTLINE");
        output_asm.push_back(fmt::format("JTE {0}", endlines_label));

        while (true) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            exec_line();
        }

        output_asm.push_back(fmt::format("JMP {0}", lines_label));
        output_asm.push_back(fmt::format("{0}:", endlines_label));
        output_asm.push_back("POPCONTENT");
    } else if (fun_name == "tokens") {
        tokens.require(TOK_PAREN_BEGIN);
        evaluate();
        tokens.require(TOK_PAREN_END);
        output_asm.push_back("CONTENTVIEW");

        tokens.require(TOK_BRACE_BEGIN);

        while (true) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            output_asm.push_back("NEXTTOKEN");
            exec_line();
        }
        output_asm.push_back("POPCONTENT");
    } else {
        int num_args = 0;
        tokens.require(TOK_PAREN_BEGIN);
        bool in_fun_loop = true;
        while (in_fun_loop) {
            ++num_args;
            evaluate();
            tokens.next();
            switch (tokens.current().type) {
            case TOK_COMMA:
                break;
            case TOK_PAREN_END:
                in_fun_loop=false;
                break;
            default:
                throw parsing_error("Token imprevisto", tokens.getLocation(tokens.current()));
            }
        }
        output_asm.push_back(fmt::format("CALL {0},{1}", fun_name,num_args));
    }
}