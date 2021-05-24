#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

void parser::read_layout(const std::filesystem::path &path, const layout_box_list &layout) {
    m_path = path;
    m_layout = &layout;
    
    try {
        m_code.add_line<opcode::LAYOUTNAME>(std::filesystem::canonical(path).string());
        if (layout.setlayout) {
            m_code.add_line<opcode::SETLAYOUT>();
        }
        for (auto &box : layout) {
            read_box(box);
        }
        if (layout.setlayout) {
            m_code.add_line<opcode::HLT>();
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    for (auto it = m_code.begin(); it != m_code.end(); ++it) {
        it->visit([&](auto &label) {
            if constexpr (std::is_base_of_v<jump_address, std::remove_reference_t<decltype(label)>>) {
                if (auto *str = std::get_if<string_ptr>(&label)) {
                    if (auto label_it = m_code.find_label(*str); label_it != m_code.end()) {
                        static_cast<jump_address &>(label) = ptrdiff_t(label_it - it);
                    } else {
                        throw layout_error(fmt::format("Etichetta sconosciuta: {}", **str));
                    }
                }
            }
        });
    }
}

void parser::read_box(const layout_box &box) {
    if (box.flags & box_flags::DISABLED) {
        return;
    }
    
    if (m_flags & parser_flags::ADD_COMMENTS && !box.name.empty()) {
        m_code.add_line<opcode::COMMENT>("### " + box.name);
    }
    current_box = &box;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback(nullptr);
    }
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == token_type::IDENTIFIER) {
        m_code.add_label(fmt::format("__{}_box_{}", m_parser_id, tok_label.value));
        m_lexer.require(token_type::END_OF_FILE);
    } else if (tok_label.type != token_type::END_OF_FILE) {
        throw unexpected_token(tok_label, token_type::IDENTIFIER);
    }

    if (!(box.flags & box_flags::NOREAD) || box.flags & box_flags::SPACER) {
        m_code.add_line<opcode::NEWBOX>();
        if (box.type == box_type::RECTANGLE) {
            m_code.add_line<opcode::PUSHDOUBLE>(box.x);
            m_code.add_line<opcode::MVBOX>(spacer_index::X);
            m_code.add_line<opcode::PUSHDOUBLE>(box.y);
            m_code.add_line<opcode::MVBOX>(spacer_index::Y);
            m_code.add_line<opcode::PUSHDOUBLE>(box.w);
            m_code.add_line<opcode::MVBOX>(spacer_index::WIDTH);
            m_code.add_line<opcode::PUSHDOUBLE>(box.h);
            m_code.add_line<opcode::MVBOX>(spacer_index::HEIGHT);
        }
        if (box.type != box_type::WHOLEFILE) {
            m_code.add_line<opcode::PUSHINT>(box.page);
            m_code.add_line<opcode::MVBOX>(spacer_index::PAGE);
        }
    }
    m_content_level = 0;

    if (m_flags & parser_flags::ADD_COMMENTS) {
        m_lexer.set_comment_callback([this](const std::string &line){
            m_code.add_line<opcode::COMMENT>(line);
        });
    }
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == token_type::IDENTIFIER) {
            try {
                auto idx = find_enum_index<spacer_index>(tok.value);
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
                m_lexer.require(token_type::SEMICOLON);
                if (negative) m_code.add_line<opcode::CALL>("neg", 1);
                m_code.add_line<opcode::MVBOX>(idx);
            } catch (std::out_of_range) {
                throw parsing_error(fmt::format("Flag spacer non valido: {}", tok.value), tok);
            }
        } else if (tok.type != token_type::END_OF_FILE) {
            throw unexpected_token(tok, token_type::IDENTIFIER);
        } else {
            break;
        }
    }

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        ++m_content_level;
        m_code.add_line<opcode::RDBOX>(box.mode, box.type, box.flags);
    }

    m_lexer.set_script(box.script);
    while (!m_lexer.check_next(token_type::END_OF_FILE)) {
        read_statement();
    }

    if (!(box.flags & box_flags::NOREAD || box.flags & box_flags::SPACER)) {
        m_code.add_line<opcode::POPCONTENT>();
    }
}

void parser::read_statement() {
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
        throw unexpected_token(tok_first);
    case token_type::SEMICOLON:
        m_lexer.advance(tok_first);
        break;
    default:
        sub_statement();
        m_lexer.require(token_type::SEMICOLON);
    }
}

void parser::sub_statement() {
    auto selvar_begin = m_code.size();
    auto prefixes = read_variable(false);
    auto selvar_end = m_code.size();
    
    auto tok = m_lexer.peek();
    bitset<setvar_flags> flags;
    bool negative = false;
    
    switch (tok.type) {
    case token_type::SUB_ASSIGN:
        flags |= setvar_flags::DECREASE;
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
        m_code.add_line<opcode::PUSHVIEW>();
        break;
    }

    if (prefixes & variable_prefixes::CAPITALIZE) {
        m_code.add_line<opcode::CALL>("capitalize", 1);
    }
    if (prefixes & variable_prefixes::AGGREGATE) {
        m_code.add_line<opcode::CALL>("aggregate", 1);
    } else if (prefixes & variable_prefixes::PARSENUM) {
        m_code.add_line<opcode::CALL>("num", 1);
    }
    if (prefixes & variable_prefixes::OVERWRITE)  flags |= setvar_flags::OVERWRITE;
    if (prefixes & variable_prefixes::FORCE)      flags |= setvar_flags::FORCE;

    m_code.move_not_comments(selvar_begin, selvar_end);
    m_code.add_line<opcode::SETVAR>(flags);
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

    simple_stack<decltype(operators)::iterator> op_stack;
    
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
        m_code.add_line<opcode::CALL>("not", 1);
        break;
    case token_type::PLUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(string_to<big_int>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("num", 1);
        }
        break;
    }
    case token_type::MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(-string_to<big_int>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("neg", 1);
        }
        break;
    }
    case token_type::INTEGER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHINT>(string_to<big_int>(tok_first.value));
        break;
    case token_type::NUMBER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case token_type::SLASH: 
        m_code.add_line<opcode::PUSHSTR>(m_lexer.require(token_type::REGEXP).parse_string());
        break;
    case token_type::STRING:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHSTR>(tok_first.parse_string());
        break;
    case token_type::CONTENT:
        m_lexer.advance(tok_first);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_first);
        }
        m_code.add_line<opcode::PUSHVIEW>();
        break;
    default: {
        auto prefixes = read_variable(true);
        if (prefixes & variable_prefixes::REF) {
            m_code.add_line<opcode::PUSHREF>();
        } else {
            m_code.add_line<opcode::PUSHVAR>();
        }
    }
    }
}

void parser::read_variable_name() {
    variable_selector selvar;
    if (m_lexer.check_next(token_type::GLOBAL)) {
        selvar.flags |= selvar_flags::GLOBAL;
    }
    auto tok = m_lexer.next();
    switch (tok.type) {
    case token_type::IDENTIFIER:
        selvar.name = tok.value;
        break;
    case token_type::BRACKET_BEGIN:
        read_expression();
        m_lexer.require(token_type::BRACKET_END);
        selvar.flags |= selvar_flags::DYN_NAME;
        break;
    default:
        throw unexpected_token(tok, token_type::IDENTIFIER);
    }
    m_code.add_line<opcode::SELVAR>(std::move(selvar));
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
        case token_type::BRACKET_BEGIN:
            read_expression();
            m_lexer.require(token_type::BRACKET_END);
            var_idx.flags |= selvar_flags::DYN_NAME;
            in_loop = false;
            break;
        case token_type::IDENTIFIER:
            var_idx.name = tok_prefix.value;
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
                var_idx.length = string_to<int>(tok.value);
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
                var_idx.index = string_to<int>(tok.value);
            } else {
                read_expression();
                var_idx.flags |= selvar_flags::DYN_IDX;
            }
            if (tok = m_lexer.check_next(token_type::COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw unexpected_token(tok, token_type::BRACKET_END);
                if (tok = m_lexer.check_next(token_type::INTEGER)) {
                    var_idx.length = string_to<int>(tok.value);
                } else {
                    read_expression();
                    var_idx.flags |= selvar_flags::DYN_LEN;
                }
            }
        }
        m_lexer.require(token_type::BRACKET_END);
    }

    m_code.add_line<opcode::SELVAR>(var_idx);

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
        m_code.add_line<opcode::GETSIZE>();
        if (fun_name == "isset") {
            m_code.add_line<opcode::CALL>("bool", 1);
        }
        break;
    case hash("box"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        m_lexer.require(token_type::PAREN_END);
        try {
            m_code.add_line<opcode::GETBOX>(find_enum_index<spacer_index>(tok.value));
        } catch (std::out_of_range) {
            throw parsing_error(fmt::format("Flag spacer non valido: {}", tok.value), tok);
        }
        break;
    }
    case hash("doc"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        m_lexer.require(token_type::PAREN_END);
        try {
            m_code.add_line<opcode::GETDOC>(find_enum_index<doc_index>(tok.value));
        } catch (std::out_of_range) {
            throw parsing_error(fmt::format("Flag documento non valido: {}", tok.value), tok);
        }
        break;
    }
    case hash("arg"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        m_lexer.require(token_type::PAREN_END);
        if (auto it = std::ranges::find(m_fun_args, tok.value); it != m_fun_args.end()) {
            m_code.add_line<opcode::PUSHARG>(it - m_fun_args.begin());
        } else {
            throw parsing_error(fmt::format("Argomento funzione sconosciuto: {}", tok.value), tok);
        }
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
        if (auto it = m_functions.find(tok.value); it != m_functions.end()) {
            if (it->second.numargs != numargs) {
                throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", tok.value, it->second.numargs), tok);
            }
            if (it->second.has_contents && m_content_level == 0) {
                throw parsing_error(fmt::format("Impossibile chiamare {}, stack contenuti vuoto", tok.value), tok);
            }
            m_code.add_line<opcode::JSRVAL>(fmt::format("__function_{}", tok.value), numargs);
        } else {
            throw parsing_error(fmt::format("Funzione {} non dichiarata", tok.value), tok);
        }
        break;
    }
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
        m_code.add_line<opcode::CALL>(it, num_args);
    }
    }
}