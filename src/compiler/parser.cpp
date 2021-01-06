#include "parser.h"

#include <iostream>

#include "utils.h"
#include "bytecode.h"
#include "parsestr.h"
#include "decimal.h"

void parser::read_layout(const bill_layout_script &layout) {
    try {
        for (auto &box : layout) {
            read_box(*box);
        }
        add_line("HLT");
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("In {0}: {1}\n{2}", current_box->name, error.message, error.line));
    }
}

void parser::read_box(const layout_box &box) {
    if (m_flags & FLAGS_DEBUG && !box.name.empty()) {
        add_line("COMMENT ## {}", box.name);
    }
    current_box = &box;
    if (!box.goto_label.empty()) add_line("LABEL {0}", box.goto_label);

    tokens.setScript(box.spacers);

    while(!tokens.ate()) {
        spacer_index index;
        switch (hash(tokens.require(TOK_IDENTIFIER).value)) {
        case hash("p"):
        case hash("page"):
            index = spacer_index::SPACER_PAGE;
            break;
        case hash("x"):
            index = spacer_index::SPACER_X;
            break;
        case hash("y"):
            index = spacer_index::SPACER_Y;
            break;
        case hash("w"):
        case hash("width"):
            index = spacer_index::SPACER_W;
            break;
        case hash("h"):
        case hash("height"):
            index = spacer_index::SPACER_H;
            break;
        default:
            throw tokens.unexpected_token();
        }
        bool negative = false;
        switch (tokens.next().type) {
        case TOK_PLUS:
            break;
        case TOK_MINUS:
            negative = true;
            break;
        default:
            throw tokens.unexpected_token(TOK_PLUS);
        }
        read_expression();
        if (negative) add_line("NEG");
        add_line("MVBOX {0}", index);
    }

    switch (box.type) {
    case box_type::BOX_RECTANGLE:
        add_line("RDBOX {0},{1},{2:.{6}f},{3:.{6}f},{4:.{6}f},{5:.{6}f}", box.mode, box.page, box.x, box.y, box.w, box.h, fixed_point::decimal_points);
        break;
    case box_type::BOX_PAGE:
        add_line("RDPAGE {0},{1}", box.mode, box.page);
        break;
    case box_type::BOX_FILE:
        add_line("RDFILE {0}", box.mode);
        break;
    case box_type::BOX_NO_READ:
        add_line("SETPAGE {}", box.page);
        break;
    }

    tokens.setScript(box.script);
    while (!tokens.ate()) read_statement();
}

void parser::read_statement() {
    switch (tokens.next(false).type) {
    case TOK_BRACE_BEGIN:
        tokens.advance();
        while (true) {
            if (tokens.next(false).type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            read_statement();
        }
        break;
    case TOK_FUNCTION:
        read_keyword();
        break;
    case TOK_END_OF_FILE:
        tokens.advance();
        break;
    default:
    {
        int flags = read_variable(false);
        
        switch (tokens.next(false).type) {
        case TOK_ASSIGN:
            tokens.advance();
            read_expression();
            break;
        case TOK_END_OF_FILE:
        default:
            add_line("PUSHVIEW");
            break;
        }

        if (flags & VAR_NUMBER) {
            add_line("PARSENUM");
        }
        
        if (flags & VAR_RESET) {
            add_line("RESETVAR");
        } else if (flags & VAR_INCREASE) {
            add_line("INC");
        } else if (flags & VAR_DECREASE) {
            add_line("DEC");
        } else {
            add_line("SETVAR");
        }
    }
    }
}

void parser::read_expression() {
    auto op_prec = [](token_type op_type) {
        switch (op_type) {
        case TOK_ASTERISK:
        case TOK_SLASH:
            return 6;
        case TOK_PLUS:
        case TOK_MINUS:
            return 5;
        case TOK_LESS:
        case TOK_LESS_EQ:
        case TOK_GREATER:
        case TOK_GREATER_EQ:
            return 4;
        case TOK_EQUALS:
        case TOK_NOT_EQUALS:
            return 3;
        case TOK_AND:
            return 2;
        case TOK_OR:
            return 1;
        default:
            return 0;
        }
    };

    auto op_opcode = [](token_type op_type) {
        switch (op_type) {
        case TOK_ASTERISK:      return "MUL";
        case TOK_SLASH:         return "DIV";
        case TOK_PLUS:          return "ADD";
        case TOK_MINUS:         return "SUB";
        case TOK_LESS:          return "LT";
        case TOK_LESS_EQ:       return "LEQ";
        case TOK_GREATER:       return "GT";
        case TOK_GREATER_EQ:    return "GEQ";
        case TOK_EQUALS:        return "EQ";
        case TOK_NOT_EQUALS:    return "NEQ";
        case TOK_AND:           return "AND";
        case TOK_OR:            return "OR";
        default:
            throw layout_error("Operatore non valido");
        }
    };

    sub_expression();

    std::vector<token_type> op_stack;
    
    while (true) {
        token_type op_type = tokens.next(false).type;
        if (op_prec(op_type) > 0) {
            tokens.advance();
            if (!op_stack.empty() && op_prec(op_stack.back()) >= op_prec(op_type)) {
                add_line(op_opcode(op_stack.back()));
                op_stack.pop_back();
            }
            op_stack.push_back(op_type);
            sub_expression();
        } else {
            break;
        }
    }

    while (!op_stack.empty()) {
        add_line(op_opcode(op_stack.back()));
        op_stack.pop_back();
    }
}

void parser::sub_expression() {
    switch (tokens.next(false).type) {
    case TOK_PAREN_BEGIN:
        tokens.advance();
        read_expression();
        tokens.require(TOK_PAREN_END);
        break;
    case TOK_FUNCTION:
        read_function();
        break;
    case TOK_NOT:
        tokens.advance();
        sub_expression();
        add_line("NOT");
        break;
    case TOK_MINUS:
    {
        tokens.advance();
        switch (tokens.next(false).type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            tokens.advance();
            add_line("PUSHNUM -{0}", tokens.current().value);
            break;
        default:
            sub_expression();
            add_line("NEG");
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        tokens.advance();
        add_line("PUSHNUM {0}", tokens.current().value);
        break;
    case TOK_SLASH:
        tokens.require(TOK_REGEXP);
        [[fallthrough]];
    case TOK_STRING:
        tokens.advance();
        add_line("PUSHSTR {0}", tokens.current().value);
        break;
    case TOK_CONTENT:
        tokens.advance();
        add_line("PUSHVIEW");
        break;
    default:
    {
        int flags = read_variable(true);
        if (flags & VAR_MOVE) {
            add_line("MOVEVAR");
        } else {
            add_line("PUSHVAR");
        }
    }
    }
}

int parser::read_variable(bool read_only) {
    int flags = 0;
    bool isglobal = false;
    bool getindex = false;
    bool getindexlast = false;
    bool rangeall = false;
    int index = 0;
    int index_last = -1;

    bool in_loop = true;
    while (in_loop) {
        switch (tokens.next().type) {
        case TOK_GLOBAL:
            isglobal = true;
            break;
        case TOK_PERCENT:
            if (read_only) throw tokens.unexpected_token(TOK_IDENTIFIER);
            flags |= VAR_NUMBER;
            break;
        case TOK_AMPERSAND:
            if (!read_only) throw tokens.unexpected_token(TOK_IDENTIFIER);
            flags |= VAR_MOVE;
            break;
        case TOK_IDENTIFIER:
            in_loop = false;
            break;
        default:
            throw tokens.unexpected_token(TOK_IDENTIFIER);
        }
    }
    
    std::string name(tokens.current().value);

    if (tokens.next(false).type == TOK_BRACKET_BEGIN) { // variable[
        tokens.advance();
        switch (tokens.next(false).type) {
        case TOK_BRACKET_END: // variable[] -- aggiunge alla fine
            if (read_only) throw tokens.unexpected_token(TOK_INTEGER);
            index = -1;
            break;
        case TOK_COLON: // variable[:] -- seleziona range intero
            if (read_only) throw tokens.unexpected_token(TOK_INTEGER);
            tokens.advance();
            rangeall = true;
            break;
        case TOK_INTEGER: // variable[int
            tokens.advance();
            index = std::stoi(std::string(tokens.current().value));
            if (tokens.next(false).type == TOK_COLON && !read_only) { // variable[int:
                tokens.advance();
                if (tokens.next(false).type == TOK_INTEGER) { // variable[a:b] -- seleziona range
                    tokens.advance();
                    index_last = std::stoi(std::string(tokens.current().value));
                } else { // variable[int:expr] -- seleziona range
                    add_line("PUSHNUM {0}", index);
                    read_expression();
                    getindex = true;
                    getindexlast = true;
                }
            }
            break;
        default: // variable[expr
            read_expression();
            getindex = true;
            if (tokens.next(false).type == TOK_COLON && !read_only) { // variable[expr:expr]
                tokens.advance();
                read_expression();
                getindexlast = true;
            }
        }
        tokens.require(TOK_BRACKET_END);
    }

    if (!read_only) {
        switch(tokens.next(false).type) {
        case TOK_PLUS:
            flags |= VAR_INCREASE;
            tokens.advance();
            break;
        case TOK_MINUS:
            flags |= VAR_DECREASE;
            tokens.advance();
            break;
        case TOK_COLON:
            flags |= VAR_RESET;
            tokens.advance();
            break;
        default:
            break;
        }
    }

    if (isglobal) {
        name = "*" + name;
    }
    if (rangeall) {
        add_line("SELRANGEALL {0}", name);
    } else if (getindex) {
        if (getindexlast) {
            add_line("SELRANGETOP {0}", name);
        } else {
            add_line("SELVARTOP {0}", name);
        }
    } else if (index_last >= 0) {
        add_line("SELRANGE {0},{1},{2}", name, index, index_last);
    } else {
        add_line("SELVAR {0},{1}", name, index);
    }

    return flags;
}

void parser::read_function() {
    auto tok_fun_name = tokens.require(TOK_FUNCTION);
    std::string fun_name(tok_fun_name.value.substr(1));

    auto var_function = [&](const auto & ... cmd) {
        tokens.require(TOK_PAREN_BEGIN);
        read_variable(true);
        tokens.require(TOK_PAREN_END);
        add_line(cmd ...);
    };

    auto void_function = [&](const auto & ... cmd) {
        tokens.require(TOK_PAREN_BEGIN);
        tokens.require(TOK_PAREN_END);
        add_line(cmd ...);
    };

    switch (hash(fun_name)) {
    case hash("isset"):     var_function("ISSET"); break;
    case hash("size"):      var_function("GETSIZE"); break;
    case hash("ate"):       void_function("ATE"); break;
    case hash("boxwidth"):  void_function("PUSHNUM {0:.{1}f}", current_box->w, fixed_point::decimal_points); break;
    case hash("boxheight"): void_function("PUSHNUM {0:.{1}f}", current_box->h, fixed_point::decimal_points); break;
    case hash("date"):
    case hash("month"):
        read_date_fun(fun_name);
        break;
    default:
    {
        int num_args = 0;
        tokens.require(TOK_PAREN_BEGIN);
        bool in_fun_loop = true;
        while (in_fun_loop) {
            if (tokens.next(false).type == TOK_PAREN_END) {
                tokens.advance();
                break;
            }
            ++num_args;
            read_expression();
            switch (tokens.next().type) {
            case TOK_COMMA:
                break;
            case TOK_PAREN_END:
                in_fun_loop=false;
                break;
            default:
                throw tokens.unexpected_token(TOK_PAREN_END);
            }
        }
        auto call_op = [&](int should_be, auto & ... args) {
            if (num_args != should_be) {
                throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", fun_name, should_be), tokens.getLocation(tok_fun_name));
            }
            add_line(args...);
        };
        switch (hash(fun_name)) {
        case hash("num"): call_op(1, "PARSENUM"); break;
        case hash("int"): call_op(1, "PARSEINT"); break;
        default:
            add_line("CALL {0},{1}", fun_name,num_args);
        }
    }
    }
}

void parser::read_date_fun(const std::string &fun_name) {
    tokens.require(TOK_PAREN_BEGIN);
    read_expression();
    switch (tokens.next().type) {
    case TOK_COMMA:
    {
        auto date_fmt = tokens.require(TOK_STRING).value;
        add_line("PUSHSTR {}", date_fmt);
        std::string regex = "/(%D)/";
        int idx = -1;
        switch (tokens.next().type) {
        case TOK_COMMA:
        {
            tokens.require(TOK_REGEXP);
            regex = std::string(tokens.current().value);
            switch (tokens.next().type) {
            case TOK_INTEGER:
                idx = std::stoi(std::string(tokens.current().value));
                break;
            case TOK_PAREN_END:
                break;
            default:
                throw tokens.unexpected_token(TOK_PAREN_END);
            }
            break;
        }
        case TOK_PAREN_END:
            break;
        default:
            throw tokens.unexpected_token(TOK_PAREN_END);
        }
        
        std::string date_regex;
        parse_string(date_regex, date_fmt);
        string_replace(date_regex, ".", "\\.");
        string_replace(date_regex, "/", "\\/");
        string_replace(date_regex, "%b", "\\w+");
        string_replace(date_regex, "%B", "\\w+");
        string_replace(date_regex, "%d", "\\d{2}");
        string_replace(date_regex, "%m", "\\d{2}");
        string_replace(date_regex, "%y", "\\d{2}");
        string_replace(date_regex, "%Y", "\\d{4}");

        string_replace(regex, "%D", date_regex);
        add_line("PUSHSTR {}", regex);
        if (idx >= 0) {
            add_line("PUSHNUM {}", idx);
            add_line("CALL {},4", fun_name);
        } else {
            add_line("CALL {},3", fun_name);
        }
        break;
    }
    case TOK_PAREN_END:
        add_line("CALL {},1", fun_name);
        break;
    default:
        throw tokens.unexpected_token(TOK_PAREN_END);
    }
}