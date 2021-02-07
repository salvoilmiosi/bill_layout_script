#include "parser.h"

#include <iostream>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const bill_layout_script &layout) {
    m_layout = &layout;
    
    try {
        if (layout.language_code != 0) {
            add_line(opcode::SETLANG, layout.language_code);
        }
        for (auto &box : layout.m_boxes) {
            read_box(*box);
        }
        add_line(opcode::RET);
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    for (auto line = m_code.begin(); line != m_code.end(); ++line) {
        if (line->type() == typeid(jump_address)) {
            auto &addr = line->get<jump_address>();
            if (auto it = m_labels.find(addr.label); it != m_labels.end()) {
                addr.address = it->second - (line - m_code.begin());
            } else {
                throw layout_error(fmt::format("Etichetta sconosciuta: {}", addr.label));
            }
        }
    }
}

void parser::add_label(const std::string &label) {
    if (!m_labels.try_emplace(label, m_code.size()).second) {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    }
}

void parser::read_box(const layout_box &box) {
    if (m_flags & FLAGS_DEBUG && !box.name.empty()) {
        add_line(opcode::COMMENT, "### " + box.name);
    }
    current_box = &box;

    m_lexer.set_bytecode(nullptr);
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == TOK_IDENTIFIER) {
        add_label(fmt::format("__label_{}", tok_label.value));
        m_lexer.require(TOK_END_OF_FILE);
    } else if (tok_label.type != TOK_END_OF_FILE) {
        throw unexpected_token(tok_label, TOK_IDENTIFIER);
    }

    if (m_flags & FLAGS_DEBUG) {
        m_lexer.set_bytecode(&m_code);
    }
    m_lexer.set_script(box.spacers);
    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == TOK_IDENTIFIER) {
            spacer_index index;
            switch (hash(tok.value)) {
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
                throw unexpected_token(tok);
            }

            bool negative = false;
            auto tok_sign = m_lexer.next();
            switch (tok_sign.type) {
            case TOK_PLUS:
                break;
            case TOK_MINUS:
                negative = true;
                break;
            default:
                throw unexpected_token(tok_sign, TOK_PLUS);
            }
            read_expression();
            if (negative) add_line(opcode::NEG);
            add_line(opcode::MVBOX, index);
        } else if (tok.type != TOK_END_OF_FILE) {
            throw unexpected_token(tok, TOK_IDENTIFIER);
        } else {
            break;
        }
    }

    add_line(opcode::RDBOX, pdf_rect(box));

    m_lexer.set_script(box.script);
    while (read_statement());
}

bool parser::read_statement() {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case TOK_BRACE_BEGIN:
        m_lexer.advance(tok_first);
        while (true) {
            auto tok = m_lexer.peek();
            if (tok.type == TOK_BRACE_END) {
                m_lexer.advance(tok);
                break;
            } else if (tok.type != TOK_END_OF_FILE) {
                read_statement();
            } else {
                throw unexpected_token(tok, TOK_BRACE_END);
            }
        }
        break;
    case TOK_FUNCTION:
        read_keyword();
        break;
    case TOK_END_OF_FILE:
        return false;
    default: {
        int flags = read_variable(false);
        
        auto tok = m_lexer.peek();
        switch (tok.type) {
        case TOK_ASSIGN:
            m_lexer.advance(tok);
            read_expression();
            break;
        default:
            add_line(opcode::PUSHVIEW);
            break;
        }

        if (flags & VAR_SUM_ALL) {
            add_line(opcode::MOVCONTENT);
            add_line(opcode::SUBVIEW);
            add_line(opcode::PUSHNULL);
            std::string label_begin = fmt::format("__sum_{}", m_code.size());
            std::string label_end = fmt::format("__endsum_{}", m_code.size());
            add_label(label_begin);
            add_line(opcode::NEXTRESULT);
            add_line(opcode::JTE, jump_address{label_end, 0});
            add_line(opcode::PUSHVIEW);
            add_line(opcode::PARSENUM);
            add_line(opcode::ADD);
            add_line(opcode::JMP, jump_address{label_begin, 0});
            add_label(label_end);
            add_line(opcode::POPCONTENT);
        } else if (flags & VAR_PARSENUM) {
            add_line(opcode::PARSENUM);
        }
        
        if (flags & VAR_RESET) {
            add_line(opcode::RESETVAR);
        } else if (flags & VAR_INCREASE) {
            add_line(opcode::INC);
        } else if (flags & VAR_DECREASE) {
            add_line(opcode::DEC);
        } else {
            add_line(opcode::SETVAR);
        }
    }
    }
    return true;
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
        case TOK_ASTERISK:      return opcode::MUL;
        case TOK_SLASH:         return opcode::DIV;
        case TOK_PLUS:          return opcode::ADD;
        case TOK_MINUS:         return opcode::SUB;
        case TOK_LESS:          return opcode::LT;
        case TOK_LESS_EQ:       return opcode::LEQ;
        case TOK_GREATER:       return opcode::GT;
        case TOK_GREATER_EQ:    return opcode::GEQ;
        case TOK_EQUALS:        return opcode::EQ;
        case TOK_NOT_EQUALS:    return opcode::NEQ;
        case TOK_AND:           return opcode::AND;
        case TOK_OR:            return opcode::OR;
        default:
            throw layout_error("Operatore non valido");
        }
    };

    sub_expression();

    std::vector<token_type> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        if (op_prec(tok_op.type) > 0) {
            m_lexer.advance(tok_op);
            if (!op_stack.empty() && op_prec(op_stack.back()) >= op_prec(tok_op.type)) {
                add_line(op_opcode(op_stack.back()));
                op_stack.pop_back();
            }
            op_stack.push_back(tok_op.type);
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
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case TOK_PAREN_BEGIN:
        m_lexer.advance(tok_first);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        break;
    case TOK_FUNCTION:
        read_function();
        break;
    case TOK_NOT:
        m_lexer.advance(tok_first);
        sub_expression();
        add_line(opcode::NOT);
        break;
    case TOK_MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            m_lexer.advance(tok_num);
            add_line(opcode::PUSHNUM, -fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line(opcode::NEG);
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        m_lexer.advance(tok_first);
        add_line(opcode::PUSHNUM, fixed_point(std::string(tok_first.value)));
        break;
    case TOK_SLASH: 
        add_line(opcode::PUSHSTR, m_lexer.require(TOK_REGEXP).parse_string());
        break;
    case TOK_STRING:
        m_lexer.advance(tok_first);
        add_line(opcode::PUSHSTR, tok_first.parse_string());
        break;
    case TOK_CONTENT:
        m_lexer.advance(tok_first);
        add_line(opcode::PUSHVIEW);
        break;
    default: {
        int flags = read_variable(true);
        if (flags & VAR_MOVE) {
            add_line(opcode::MOVEVAR);
        } else {
            add_line(opcode::PUSHVAR);
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
    small_int index = 0;
    small_int index_last = -1;

    bool in_loop = true;
    token tok_modifier;
    while (in_loop) {
        tok_modifier = m_lexer.next();
        switch (tok_modifier.type) {
        case TOK_GLOBAL:
            isglobal = true;
            break;
        case TOK_PERCENT:
            if (read_only) throw unexpected_token(tok_modifier, TOK_IDENTIFIER);
            flags |= VAR_PARSENUM;
            break;
        case TOK_CARET:
            if (read_only) throw unexpected_token(tok_modifier, TOK_IDENTIFIER);
            flags |= VAR_SUM_ALL;
            break;
        case TOK_AMPERSAND:
            if (!read_only) throw unexpected_token(tok_modifier, TOK_IDENTIFIER);
            flags |= VAR_MOVE;
            break;
        case TOK_IDENTIFIER:
            in_loop = false;
            break;
        default:
            throw unexpected_token(tok_modifier, TOK_IDENTIFIER);
        }
    }
    
    // dopo l'ultima call a next tok_modifier punta al nome della variabile
    variable_name name{std::string(tok_modifier.value), isglobal};

    if (m_lexer.check_next(TOK_BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case TOK_BRACKET_END: // variable[] -- aggiunge alla fine
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            index = -1;
            break;
        case TOK_COLON: // variable[:] -- seleziona range intero
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            m_lexer.advance(tok);
            rangeall = true;
            break;
        case TOK_INTEGER: // variable[int
            m_lexer.advance(tok);
            index = cstoi(tok.value);
            if (m_lexer.check_next(TOK_COLON) && !read_only) { // variable[int:
                if (m_lexer.check_next(TOK_INTEGER)) { // variable[a:b] -- seleziona range
                    index_last = cstoi(tok.value);
                } else { // variable[int:expr] -- seleziona range
                    add_line(opcode::PUSHNUM, fixed_point(index));
                    read_expression();
                    getindex = true;
                    getindexlast = true;
                }
            }
            break;
        default: // variable[expr
            read_expression();
            getindex = true;
            if (m_lexer.check_next(TOK_COLON) && !read_only) { // variable[expr:expr]
                read_expression();
                getindexlast = true;
            }
        }
        m_lexer.require(TOK_BRACKET_END);
    }

    if (!read_only) {
        token tok = m_lexer.peek();
        switch(tok.type) {
        case TOK_PLUS:
            flags |= VAR_INCREASE;
            m_lexer.advance(tok);
            break;
        case TOK_MINUS:
            flags |= VAR_DECREASE;
            m_lexer.advance(tok);
            break;
        case TOK_COLON:
            flags |= VAR_RESET;
            m_lexer.advance(tok);
            break;
        default:
            break;
        }
    }

    if (rangeall) {
        add_line(opcode::SELRANGEALL, name);
    } else if (getindex) {
        if (getindexlast) {
            add_line(opcode::SELRANGETOP, name);
        } else {
            add_line(opcode::SELVARTOP, name);
        }
    } else if (index_last >= 0) {
        add_line(opcode::SELVAR, variable_idx{name, index, index_last});
    } else {
        add_line(opcode::SELVAR, variable_idx{name, index, 1});
    }

    return flags;
}

void parser::read_function() {
    auto tok_fun_name = m_lexer.require(TOK_FUNCTION);
    std::string fun_name(tok_fun_name.value.substr(1));

    auto var_function = [&](opcode cmd) {
        m_lexer.require(TOK_PAREN_BEGIN);
        bool isglobal = m_lexer.check_next(TOK_GLOBAL);
        auto tok_var = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        add_line(cmd, variable_name{std::string(tok_var.value), isglobal});
    };

    auto void_function = [&](opcode cmd, auto && ... args) {
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line(cmd, std::forward<decltype(args)>(args) ...);
    };

    switch (hash(fun_name)) {
    case hash("isset"):     var_function(opcode::ISSET); break;
    case hash("size"):      var_function(opcode::GETSIZE); break;
    case hash("ate"):       void_function(opcode::ATE); break;
    case hash("null"):      void_function(opcode::PUSHNULL); break;
    case hash("boxwidth"):  void_function(opcode::PUSHNUM, fixed_point(current_box->w)); break;
    case hash("boxheight"): void_function(opcode::PUSHNUM, fixed_point(current_box->h)); break;
    default: {
        small_int num_args = 0;
        m_lexer.require(TOK_PAREN_BEGIN);
        while (true) {
            auto tok = m_lexer.peek();
            if (tok.type == TOK_PAREN_END) {
                m_lexer.advance(tok);
                break;
            } else if (tok.type != TOK_END_OF_FILE) {
                ++num_args;
                read_expression();
                auto tok_comma = m_lexer.peek();
                switch (tok_comma.type) {
                case TOK_COMMA:
                    m_lexer.advance(tok_comma);
                    break;
                case TOK_PAREN_END:
                    break;
                default:
                    unexpected_token(tok_comma, TOK_PAREN_END);
                }
            } else {
                throw unexpected_token(tok, TOK_PAREN_END);
            }
        }

        auto call_op = [&](int should_be, auto && ... args) {
            if (num_args != should_be) {
                throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", fun_name, should_be), tok_fun_name);
            }
            add_line(std::forward<decltype(args)>(args) ...);
        };
        switch (hash(fun_name)) {
        case hash("num"): call_op(1, opcode::PARSENUM); break;
        case hash("int"): call_op(1, opcode::PARSEINT); break;
        default:
            if (auto *fun = find_function(fun_name)) {
                if (num_args < fun->minargs || num_args > fun->maxargs) {
                    if (fun->maxargs == std::numeric_limits<size_t>::max()) {
                        throw parsing_error(fmt::format("La funzione {0} richiede almeno {1} argomenti", fun->name, fun->minargs), tok_fun_name);
                    } else if (fun->minargs == fun->maxargs) {
                        throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", fun->name, fun->minargs), tok_fun_name);
                    } else {
                        throw parsing_error(fmt::format("La funzione {0} richiede {1}-{2} argomenti", fun->name, fun->minargs, fun->maxargs), tok_fun_name);
                    }
                }
                add_line(opcode::CALL, command_call{fun_name, num_args});
            } else {
                throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
            }
        }
    }
    }
}