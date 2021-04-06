#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const std::filesystem::path &path, const box_vector &layout) {
    m_path = path;
    m_layout = &layout;
    
    try {
        if (intl::valid_language(layout.language_code)) {
            add_line<opcode::SETLANG>(layout.language_code);
        }
        for (auto &box : layout) {
            read_box(box);
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }
}

size_t parser::find_label(std::string_view label) {
    return std::ranges::find_if(m_code, [&](const command_args &line) {
        return line.command() == opcode::LABEL
            && label == *line.get_args<opcode::LABEL>();
    }) - m_code.begin();
};

void parser::add_label(std::string label) {
    if (find_label(label) != m_code.size()) {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    } else {
        add_line<opcode::LABEL>(std::move(label));
    }
}

spacer_index find_spacer_index(std::string_view name) {
    for (size_t i=0; i<EnumSize<spacer_index>; ++i) {
        if (EnumData<const char *>(static_cast<spacer_index>(i)) == name) {
            return static_cast<spacer_index>(i);
        }
    }
    return static_cast<spacer_index>(-1);
}

void parser::read_box(const layout_box &box) {
    if (box.flags & box_flags::DISABLED) {
        return;
    }
    
    if (m_flags & parser_flags::ADD_COMMENTS && !box.name.empty()) {
        add_line<opcode::COMMENT>("### " + box.name);
    }
    current_box = &box;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback(nullptr);
    }
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == token_type::IDENTIFIER) {
        add_label(fmt::format("__box_{}_{}", tok_label.value, hash(m_path.string())));
        m_lexer.require(token_type::END_OF_FILE);
    } else if (tok_label.type != token_type::END_OF_FILE) {
        throw unexpected_token(tok_label, token_type::IDENTIFIER);
    }

    if (!(box.flags & box_flags::NOREAD) || box.flags & box_flags::SPACER) {
        add_line<opcode::NEWBOX>();
        if (box.type == box_type::RECTANGLE) {
            add_line<opcode::PUSHDOUBLE>(box.x);
            add_line<opcode::MVBOX>(spacer_index::X);
            add_line<opcode::PUSHDOUBLE>(box.y);
            add_line<opcode::MVBOX>(spacer_index::Y);
            add_line<opcode::PUSHDOUBLE>(box.w);
            add_line<opcode::MVBOX>(spacer_index::WIDTH);
            add_line<opcode::PUSHDOUBLE>(box.h);
            add_line<opcode::MVBOX>(spacer_index::HEIGHT);
        }
        if (box.type != box_type::WHOLEFILE) {
            add_line<opcode::PUSHINT>(box.page);
            add_line<opcode::MVBOX>(spacer_index::PAGE);
        }
    }
    m_content_level = 0;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback([this](const std::string &line){
            add_line<opcode::COMMENT>(line);
        });
    }
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == token_type::IDENTIFIER) {
            if (auto it = find_spacer_index(tok.value); it != static_cast<spacer_index>(-1)) {
                bool negative = false;
                auto tok_sign = m_lexer.next();
                switch (tok_sign.type) {
                case token_type::PLUS:
                    break;
                case token_type::MINUS:
                    negative = true;
                    break;
                default:
                    throw unexpected_token(tok_sign, token_type::PLUS);
                }
                read_expression();
                if (negative) add_line<opcode::CALL>("neg", 1);
                add_line<opcode::MVBOX>(it);
            } else {
                throw unexpected_token(tok);
            }
        } else if (tok.type != token_type::END_OF_FILE) {
            throw unexpected_token(tok, token_type::IDENTIFIER);
        } else {
            break;
        }
    }

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        ++m_content_level;
        add_line<opcode::RDBOX>(box.mode, box.type, box.flags);
    }

    m_lexer.set_script(box.script);
    while (read_statement(false));

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        add_line<opcode::POPCONTENT>();
    }
}

bool parser::read_statement(bool throw_on_eof) {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case token_type::BRACE_BEGIN:
        m_lexer.advance(tok_first);
        while (!m_lexer.check_next(token_type::BRACE_END)) {
            read_statement();
        }
        break;
    case token_type::FUNCTION:
        read_keyword();
        break;
    case token_type::END_OF_FILE:
        if (throw_on_eof) {
            throw unexpected_token(tok_first);
        }
        return false;
    default: {
        size_t selvar_begin = m_code.size();
        auto prefixes = read_variable(false);
        size_t selvar_end = m_code.size();
        
        auto tok = m_lexer.peek();
        bitset<setvar_flags> flags;
        bool negative = false;
        
        switch (tok.type) {
        case token_type::SUB_ASSIGN:
            prefixes |= variable_prefixes::NEGATE;
            [[fallthrough]];
        case token_type::ADD_ASSIGN:
            flags |= setvar_flags::INCREASE;
            [[fallthrough]];
        case token_type::ASSIGN:
            m_lexer.advance(tok);
            read_expression();
            break;
        default:
            if (m_content_level == 0) {
                throw parsing_error("Stack contenuti vuoto", tok);
            }
            add_line<opcode::PUSHVIEW>();
            break;
        }

        if (prefixes & variable_prefixes::CAPITALIZE) {
            add_line<opcode::CALL>("capitalize", 1);
        }
        if (prefixes & variable_prefixes::AGGREGATE) {
            add_line<opcode::CALL>("aggregate", 1);
        } else if (prefixes & variable_prefixes::PARSENUM) {
            add_line<opcode::CALL>("num", 1);
        }
        if (prefixes & variable_prefixes::NEGATE) {
            add_line<opcode::CALL>("neg", 1);
        }
        if (prefixes & variable_prefixes::OVERWRITE)  flags |= setvar_flags::OVERWRITE;
        if (prefixes & variable_prefixes::FORCE)      flags |= setvar_flags::FORCE;

        std::rotate(m_code.begin() + selvar_begin, m_code.begin() + selvar_end, m_code.end());
        add_line<opcode::SETVAR>(flags);
    }
    }
    return true;
}

void parser::read_expression() {
    std::map<token_type, std::tuple<int, command_args>> operators = {
        {token_type::ASTERISK,      {6, make_command<opcode::CALL>("mul", 2)}},
        {token_type::SLASH,         {6, make_command<opcode::CALL>("div", 2)}},
        {token_type::PLUS,          {5, make_command<opcode::CALL>("add", 2)}},
        {token_type::MINUS,         {5, make_command<opcode::CALL>("sub", 2)}},
        {token_type::LESS,          {4, make_command<opcode::CALL>("lt", 2)}},
        {token_type::LESS_EQ,       {4, make_command<opcode::CALL>("leq", 2)}},
        {token_type::GREATER,       {4, make_command<opcode::CALL>("gt", 2)}},
        {token_type::GREATER_EQ,    {4, make_command<opcode::CALL>("geq", 2)}},
        {token_type::EQUALS,        {3, make_command<opcode::CALL>("eq", 2)}},
        {token_type::NOT_EQUALS,    {3, make_command<opcode::CALL>("neq", 2)}},
        {token_type::AND,           {2, make_command<opcode::CALL>("and", 2)}},
        {token_type::OR,            {1, make_command<opcode::CALL>("or", 2)}},
    };

    sub_expression();

    std::vector<decltype(operators)::iterator> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        auto it = operators.find(tok_op.type);
        if (it == std::end(operators)) break;
        
        m_lexer.advance(tok_op);
        if (!op_stack.empty() && std::get<int>(op_stack.back()->second) >= std::get<int>(it->second)) {
            m_code.push_back(std::get<command_args>(op_stack.back()->second));
            op_stack.pop_back();
        }
        op_stack.push_back(it);
        sub_expression();
    }

    while (!op_stack.empty()) {
        m_code.push_back(std::get<command_args>(op_stack.back()->second));
        op_stack.pop_back();
    }
}

void parser::sub_expression() {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case token_type::PAREN_BEGIN:
        m_lexer.advance(tok_first);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        break;
    case token_type::FUNCTION:
        read_function();
        break;
    case token_type::NOT:
        m_lexer.advance(tok_first);
        sub_expression();
        add_line<opcode::CALL>("not", 1);
        break;
    case token_type::PLUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            add_line<opcode::PUSHINT>(string_to<int64_t>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line<opcode::CALL>("num", 1);
        }
        break;
    }
    case token_type::MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            add_line<opcode::PUSHINT>(-string_to<int64_t>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            add_line<opcode::PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line<opcode::CALL>("neg", 1);
        }
        break;
    }
    case token_type::INTEGER:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHINT>(string_to<int64_t>(tok_first.value));
        break;
    case token_type::NUMBER:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case token_type::SLASH: 
        add_line<opcode::PUSHSTR>(m_lexer.require(token_type::REGEXP).parse_string());
        break;
    case token_type::STRING:
        m_lexer.advance(tok_first);
        add_line<opcode::PUSHSTR>(tok_first.parse_string());
        break;
    case token_type::CONTENT:
        m_lexer.advance(tok_first);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_first);
        }
        add_line<opcode::PUSHVIEW>();
        break;
    default: {
        auto prefixes = read_variable(true);
        if (prefixes & variable_prefixes::REF) {
            add_line<opcode::PUSHREF>();
        } else {
            add_line<opcode::PUSHVAR>();
        }
    }
    }
}

void parser::read_variable_name() {
    bool isglobal = m_lexer.check_next(token_type::GLOBAL);
    auto tok_var = m_lexer.require(token_type::IDENTIFIER);
    add_line<opcode::SELVAR>(std::string(tok_var.value), small_int(0), small_int(0), isglobal ? flags_t(selvar_flags::GLOBAL) : flags_t(0));
}

bitset<variable_prefixes> parser::read_variable(bool read_only) {
    bitset<variable_prefixes> prefixes;

    variable_selector var_idx;

    token tok_prefix;

    auto add_flags_to = [&](auto &out, auto flags, bool condition = true) {
        if ((out & flags) || !condition) throw unexpected_token(tok_prefix, token_type::IDENTIFIER);
        out |= flags;
    };
    
    bool in_loop = true;
    while (in_loop) {
        tok_prefix = m_lexer.next();
        switch (tok_prefix.type) {
        case token_type::GLOBAL:    add_flags_to(var_idx.flags, selvar_flags::GLOBAL); break;
        case token_type::PERCENT:   add_flags_to(prefixes, variable_prefixes::PARSENUM, !read_only); break;
        case token_type::CARET:     add_flags_to(prefixes, variable_prefixes::AGGREGATE, !read_only); break;
        case token_type::SINGLE_QUOTE: add_flags_to(prefixes, variable_prefixes::CAPITALIZE, !read_only); break;
        case token_type::TILDE:     add_flags_to(prefixes, variable_prefixes::OVERWRITE, !read_only); break;
        case token_type::NOT:       add_flags_to(prefixes, variable_prefixes::FORCE, !read_only); break;
        case token_type::AMPERSAND: add_flags_to(prefixes, variable_prefixes::REF, read_only); break;
        case token_type::IDENTIFIER:
            var_idx.name = std::string(tok_prefix.value);
            in_loop = false;
            break;
        default:
            throw unexpected_token(tok_prefix, token_type::IDENTIFIER);
        }
    }

    if (m_lexer.check_next(token_type::BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case token_type::COLON: {
            if (read_only) throw unexpected_token(tok, token_type::INTEGER);
            m_lexer.advance(tok);
            tok = m_lexer.peek();
            switch (tok.type) {
            case token_type::BRACKET_END:
                var_idx.flags |= selvar_flags::EACH; // variable[:]
                break;
            case token_type::INTEGER: // variable[:N] -- append N times
                m_lexer.advance(tok);
                var_idx.flags |= selvar_flags::APPEND;
                var_idx.length = string_toint(tok.value);
                break;
            default:
                read_expression();
                var_idx.flags |= selvar_flags::APPEND;
                var_idx.flags |= selvar_flags::DYN_LEN;
            }
            break;
        }
        case token_type::BRACKET_END:
            if (read_only) throw unexpected_token(tok, token_type::INTEGER);
            var_idx.flags |= selvar_flags::APPEND; // variable[] -- append
            break;
        default:
            if (tok = m_lexer.check_next(token_type::INTEGER)) { // variable[N]
                var_idx.index = string_toint(tok.value);
            } else {
                read_expression();
                var_idx.flags |= selvar_flags::DYN_IDX;
            }
            if (tok = m_lexer.check_next(token_type::COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw unexpected_token(tok, token_type::BRACKET_END);
                if (tok = m_lexer.check_next(token_type::INTEGER)) {
                    var_idx.length = string_toint(tok.value);
                } else {
                    read_expression();
                    var_idx.flags |= selvar_flags::DYN_LEN;
                }
            }
        }
        m_lexer.require(token_type::BRACKET_END);
    }

    add_line<opcode::SELVAR>(var_idx);

    return prefixes;
}

void parser::read_function() {
    auto tok_fun_name = m_lexer.require(token_type::FUNCTION);
    auto fun_name = tok_fun_name.value.substr(1);
    
    switch (hash(fun_name)) {
    case hash("isset"):
    case hash("size"):
        m_lexer.require(token_type::PAREN_BEGIN);
        read_variable_name();
        m_lexer.require(token_type::PAREN_END);
        add_line<opcode::GETSIZE>();
        if (fun_name == "isset") {
            add_line<opcode::PUSHINT>(0);
            add_line<opcode::CALL>("neq", 2);
        }
        break;
    case hash("box"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        if (auto spacer = find_spacer_index(tok.value); spacer != static_cast<spacer_index>(-1)) {
            m_lexer.require(token_type::PAREN_END);
            add_line<opcode::GETBOX>(spacer);
        } else {
            throw unexpected_token(tok);
        }
        break;
    }
    case hash("arg"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto num = string_toint(m_lexer.require(token_type::INTEGER).value);
        m_lexer.require(token_type::PAREN_END);
        add_line<opcode::PUSHARG>(small_int(num));
        break;
    }
    case hash("call"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        small_int numargs = 0;
        while (!m_lexer.check_next(token_type::PAREN_END)) {
            m_lexer.require(token_type::COMMA);
            read_expression();
            ++numargs;
        }
        add_line<opcode::JSRVAL>(fmt::format("__function_{}", tok.value), numargs);
        break;
    }
    case hash("ate"):
    case hash("docpages"):
        m_lexer.require(token_type::PAREN_BEGIN);
        m_lexer.require(token_type::PAREN_END);
        if (fun_name == "docpages") {
            add_line<opcode::DOCPAGES>();
        } else {
            add_line<opcode::GETBOX>(spacer_index::PAGE);
            add_line<opcode::DOCPAGES>();
            add_line<opcode::CALL>("gt", 2);
        }
        break;
    default: {
        auto it = function_lookup.find(fun_name);
        if (it == function_lookup.end()) {
            throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
        }

        small_int num_args = 0;
        m_lexer.require(token_type::PAREN_BEGIN);
        while (!m_lexer.check_next(token_type::PAREN_END)) {
            ++num_args;
            read_expression();
            auto tok_comma = m_lexer.peek();
            switch (tok_comma.type) {
            case token_type::COMMA:
                m_lexer.advance(tok_comma);
                break;
            case token_type::PAREN_END:
                break;
            default:
                throw unexpected_token(tok_comma, token_type::PAREN_END);
            }
        }

        const auto &fun = it->second;
        if (num_args < fun.minargs || num_args > fun.maxargs) {
            throw invalid_numargs(std::string(fun_name), fun.minargs, fun.maxargs, tok_fun_name);
        }
        add_line<opcode::CALL>(it, num_args);
    }
    }
}