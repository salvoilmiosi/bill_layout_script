#include "parser.h"

#include <iostream>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"

void parser::read_layout(const bill_layout_script &layout) {
    try {
        if (!layout.language_code.empty()) {
            add_line(opcode::SETLANG, layout.language_code);
        }
        for (auto &box : layout.m_boxes) {
            read_box(*box);
        }
        add_line(opcode::RET);
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("In {0}: {1}\n{2}", current_box->name,
            error.what(), m_lexer.tokenLocationInfo(error.location())));
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
    auto it = m_labels.find(label);
    if (it == m_labels.end()) {
        m_labels.emplace(label, m_code.size());
    } else {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    }
}

void parser::read_box(const layout_box &box) {
    if (m_flags & FLAGS_DEBUG && !box.name.empty()) {
        add_line(opcode::COMMENT, fmt::format("## {}", box.name));
    }
    current_box = &box;
    if (!box.goto_label.empty()) add_label(box.goto_label);

    m_lexer.setScript(box.spacers);

    while(!m_lexer.ate()) {
        spacer_index index;
        switch (hash(m_lexer.require(TOK_IDENTIFIER).value)) {
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
            throw m_lexer.unexpected_token();
        }
        bool negative = false;
        switch (m_lexer.next().type) {
        case TOK_PLUS:
            break;
        case TOK_MINUS:
            negative = true;
            break;
        default:
            throw m_lexer.unexpected_token(TOK_PLUS);
        }
        read_expression();
        if (negative) add_line(opcode::NEG);
        add_line(opcode::MVBOX, index);
    }

    add_line(opcode::RDBOX, pdf_rect(box));

    m_lexer.setScript(box.script);
    while (read_statement());
}

bool parser::read_statement() {
    switch (m_lexer.peek().type) {
    case TOK_BRACE_BEGIN:
        m_lexer.advance();
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            read_statement();
        }
        break;
    case TOK_FUNCTION:
        read_keyword();
        break;
    case TOK_END_OF_FILE:
        return false;
    default:
    {
        int flags = read_variable(false);
        
        switch (m_lexer.peek().type) {
        case TOK_ASSIGN:
            m_lexer.advance();
            read_expression();
            break;
        default:
            add_line(opcode::PUSHVIEW);
            break;
        }

        if (flags & VAR_PARSENUM) {
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
        token_type op_type = m_lexer.peek().type;
        if (op_prec(op_type) > 0) {
            m_lexer.advance();
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
    switch (m_lexer.peek().type) {
    case TOK_PAREN_BEGIN:
        m_lexer.advance();
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        break;
    case TOK_FUNCTION:
        read_function();
        break;
    case TOK_NOT:
        m_lexer.advance();
        sub_expression();
        add_line(opcode::NOT);
        break;
    case TOK_MINUS:
    {
        m_lexer.advance();
        switch (m_lexer.peek().type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            m_lexer.advance();
            add_line(opcode::PUSHNUM, -fixed_point(std::string(m_lexer.current().value)));
            break;
        default:
            sub_expression();
            add_line(opcode::NEG);
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        m_lexer.advance();
        add_line(opcode::PUSHNUM, fixed_point(std::string(m_lexer.current().value)));
        break;
    case TOK_SLASH:
    {
        auto tok = m_lexer.require(TOK_REGEXP);
        std::string str;
        if (!parse_regexp_token(str, tok.value)) {
            throw parsing_error("Constante regexp non valida", tok);
        }
        add_line(opcode::PUSHSTR, str);
        break;
    }
    case TOK_STRING:
    {
        m_lexer.advance();
        std::string str;
        if (!parse_string_token(str, m_lexer.current().value)) {
            throw parsing_error("Constante stringa non valida", m_lexer.current());
        }
        add_line(opcode::PUSHSTR, str);
        break;
    }
    case TOK_CONTENT:
        m_lexer.advance();
        add_line(opcode::PUSHVIEW);
        break;
    default:
    {
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
    while (in_loop) {
        switch (m_lexer.next().type) {
        case TOK_GLOBAL:
            isglobal = true;
            break;
        case TOK_PERCENT:
            if (read_only) throw m_lexer.unexpected_token(TOK_IDENTIFIER);
            flags |= VAR_PARSENUM;
            break;
        case TOK_AMPERSAND:
            if (!read_only) throw m_lexer.unexpected_token(TOK_IDENTIFIER);
            flags |= VAR_MOVE;
            break;
        case TOK_IDENTIFIER:
            in_loop = false;
            break;
        default:
            throw m_lexer.unexpected_token(TOK_IDENTIFIER);
        }
    }
    
    std::string name(m_lexer.current().value);

    if (m_lexer.check_next(TOK_BRACKET_BEGIN)) { // variable[
        switch (m_lexer.peek().type) {
        case TOK_BRACKET_END: // variable[] -- aggiunge alla fine
            if (read_only) throw m_lexer.unexpected_token(TOK_INTEGER);
            index = -1;
            break;
        case TOK_COLON: // variable[:] -- seleziona range intero
            if (read_only) throw m_lexer.unexpected_token(TOK_INTEGER);
            m_lexer.advance();
            rangeall = true;
            break;
        case TOK_INTEGER: // variable[int
            m_lexer.advance();
            index = cstoi(m_lexer.current().value);
            if (m_lexer.check_next(TOK_COLON) && !read_only) { // variable[int:
                if (m_lexer.check_next(TOK_INTEGER)) { // variable[a:b] -- seleziona range
                    index_last = cstoi(m_lexer.current().value);
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
        switch(m_lexer.peek().type) {
        case TOK_PLUS:
            flags |= VAR_INCREASE;
            m_lexer.advance();
            break;
        case TOK_MINUS:
            flags |= VAR_DECREASE;
            m_lexer.advance();
            break;
        case TOK_COLON:
            flags |= VAR_RESET;
            m_lexer.advance();
            break;
        default:
            break;
        }
    }

    if (isglobal) {
        name += '*';
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

    auto var_function = [&](auto && ... args) {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_variable(true);
        m_lexer.require(TOK_PAREN_END);
        add_line(std::forward<decltype(args)>(args) ...);
    };

    auto void_function = [&](auto && ... args) {
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line(std::forward<decltype(args)>(args) ...);
    };

    switch (hash(fun_name)) {
    case hash("isset"):     var_function(opcode::ISSET); break;
    case hash("size"):      var_function(opcode::GETSIZE); break;
    case hash("ate"):       void_function(opcode::ATE); break;
    case hash("boxwidth"):  void_function(opcode::PUSHNUM, fixed_point(current_box->w)); break;
    case hash("boxheight"): void_function(opcode::PUSHNUM, fixed_point(current_box->h)); break;
    case hash("date"):
    case hash("month"):
        read_date_fun(fun_name);
        break;
    default:
    {
        small_int num_args = 0;
        m_lexer.require(TOK_PAREN_BEGIN);
        while (!m_lexer.check_next(TOK_PAREN_END)) {
            ++num_args;
            read_expression();
            if (!m_lexer.check_next(TOK_COMMA) && m_lexer.current().type != TOK_PAREN_END) {
                throw m_lexer.unexpected_token(TOK_PAREN_END);
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
            add_line(opcode::CALL, command_call{fun_name, num_args});
        }
    }
    }
}

void parser::read_date_fun(const std::string &fun_name) {
    m_lexer.require(TOK_PAREN_BEGIN);
    read_expression();
    switch (m_lexer.next().type) {
    case TOK_COMMA:
    {
        std::string fmt_string;
        if (!parse_string_token(fmt_string, m_lexer.require(TOK_STRING).value)) {
            throw parsing_error("Costante stringa non valida", m_lexer.current());
        }

        add_line(opcode::PUSHSTR, fmt_string);
        std::string regex = "(%D)";
        int idx = -1;
        switch (m_lexer.next().type) {
        case TOK_COMMA:
        {
            auto tok = m_lexer.require(TOK_REGEXP);
            regex.clear();
            if (!parse_regexp_token(regex, tok.value)) {
                throw parsing_error("Constante regexp non valida", tok);
            }
            switch (m_lexer.next().type) {
            case TOK_INTEGER:
                idx = cstoi(m_lexer.current().value);
                break;
            case TOK_PAREN_END:
                break;
            default:
                throw m_lexer.unexpected_token(TOK_PAREN_END);
            }
            break;
        }
        case TOK_PAREN_END:
            break;
        default:
            throw m_lexer.unexpected_token(TOK_PAREN_END);
        }
        
        std::string date_regex = "\\b" + fmt_string + "\\b";
        string_replace(date_regex, ".", "\\.");
        string_replace(date_regex, "%b", "\\w+");
        string_replace(date_regex, "%B", "\\w+");
        string_replace(date_regex, "%d", "\\d{2}");
        string_replace(date_regex, "%m", "\\d{2}");
        string_replace(date_regex, "%y", "\\d{2}");
        string_replace(date_regex, "%Y", "\\d{4}");

        string_replace(regex, "%D", date_regex);
        add_line(opcode::PUSHSTR, regex);
        if (idx >= 0) {
            add_line(opcode::PUSHNUM, fixed_point(idx));
            add_line(opcode::CALL, command_call{fun_name, 4});
        } else {
            add_line(opcode::CALL, command_call{fun_name, 3});
        }
        break;
    }
    case TOK_PAREN_END:
        add_line(opcode::CALL, command_call{fun_name, 1});
        break;
    default:
        throw m_lexer.unexpected_token(TOK_PAREN_END);
    }
}