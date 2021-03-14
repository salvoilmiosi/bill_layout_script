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
            add_line<OP_SETLANG>(layout.language_code);
        }
        for (auto &box : layout) {
            read_box(box);
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    if (!(m_flags & PARSER_NO_EVAL_JUMPS)) {
        for (auto line = m_code.begin(); line != m_code.end(); ++line) {
            if (line->command() == OP_UNEVAL_JUMP) {
                auto &addr = line->get_args<OP_UNEVAL_JUMP>();
                if (auto it = m_labels.find(addr.label); it != m_labels.end()) {
                    *line = command_args(addr.cmd, jump_address(it->second - (line - m_code.begin())));
                } else {
                    throw layout_error(fmt::format("Etichetta sconosciuta: {}", addr.label));
                }
            }
        }
    }
}

void parser::add_label(const std::string &label) {
    if (!m_labels.try_emplace(label, m_code.size()).second) {
        throw layout_error(fmt::format("Etichetta goto duplicata: {}", label));
    }
}

void parser::add_comment(const std::string &line) {
    m_comments.emplace(m_code.size(), line);
}

static const std::map<std::string, spacer_index, std::less<>> spacer_index_map = {
    {"p",       SPACER_PAGE},
    {"page",    SPACER_PAGE},
    {"x",       SPACER_X},
    {"y",       SPACER_Y},
    {"w",       SPACER_WIDTH},
    {"width",   SPACER_WIDTH},
    {"h",       SPACER_HEIGHT},
    {"height",  SPACER_HEIGHT},
    {"t",       SPACER_TOP},
    {"top",     SPACER_TOP},
    {"r",       SPACER_RIGHT},
    {"right",   SPACER_RIGHT},
    {"b",       SPACER_BOTTOM},
    {"bottom",  SPACER_BOTTOM},
    {"l",       SPACER_LEFT},
    {"left",    SPACER_LEFT},
    {"rotate",  SPACER_ROTATE_CW},
    {"rot_cw",  SPACER_ROTATE_CW},
    {"rot_ccw", SPACER_ROTATE_CCW},
};

void parser::read_box(const layout_box &box) {
    if (m_flags & PARSER_ADD_COMMENTS && !box.name.empty()) {
        add_comment("### " + box.name);
    }
    current_box = &box;

    if (m_flags & PARSER_ADD_COMMENTS) {
        m_lexer.set_comment_callback(nullptr);
    }
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == TOK_IDENTIFIER) {
        add_label(fmt::format("__label_{}", tok_label.value));
        m_lexer.require(TOK_END_OF_FILE);
    } else if (tok_label.type != TOK_END_OF_FILE) {
        throw unexpected_token(tok_label, TOK_IDENTIFIER);
    }

    if (m_flags & PARSER_ADD_COMMENTS) {
        m_lexer.set_comment_callback([this](const std::string &line){
            add_comment(line);
        });
    }
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == TOK_IDENTIFIER) {
            if (auto it = spacer_index_map.find(tok.value); it != spacer_index_map.end()) {
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
                if (negative) add_line<OP_CALL>("neg", 1);
                add_line<OP_MVBOX>(it->second);
            } else {
                throw unexpected_token(tok);
            }
        } else if (tok.type != TOK_END_OF_FILE) {
            throw unexpected_token(tok, TOK_IDENTIFIER);
        } else {
            break;
        }
    }

    add_line<OP_RDBOX>(pdf_rect(box));

    m_lexer.set_script(box.script);
    while (read_statement(false));
}

bool parser::read_statement(bool throw_on_eof) {
    auto tok_first = m_lexer.peek();
    switch (tok_first.type) {
    case TOK_BRACE_BEGIN:
        m_lexer.advance(tok_first);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            read_statement();
        }
        break;
    case TOK_FUNCTION:
        read_keyword();
        break;
    case TOK_END_OF_FILE:
        if (throw_on_eof) {
            throw unexpected_token(tok_first);
        }
        return false;
    default: {
        size_t selvar_begin = m_code.size();
        int prefixes = read_variable(false);
        size_t selvar_end = m_code.size();
        
        auto tok = m_lexer.peek();
        flags_t flags = 0;
        
        switch (tok.type) {
        case TOK_ADD_ASSIGN:
        case TOK_SUB_ASSIGN:
            flags |= tok.type == TOK_ADD_ASSIGN ? SET_INCREASE : SET_DECREASE;
            [[fallthrough]];
        case TOK_ASSIGN:
            m_lexer.advance(tok);
            read_expression();
            break;
        default:
            add_line<OP_PUSHVIEW>();
            break;
        }

        if (prefixes & VP_AGGREGATE)  add_line<OP_CALL>("aggregate", 1);
        if (prefixes & VP_PARSENUM)   add_line<OP_CALL>("num", 1);
        if (prefixes & VP_OVERWRITE)  flags |= SET_OVERWRITE;
        if (prefixes & VP_FORCE)      flags |= SET_FORCE;

        move_to_end(m_code, selvar_begin, selvar_end);
        add_line<OP_SETVAR>(flags);
    }
    }
    return true;
}

void parser::read_expression() {
    std::map<token_type, std::tuple<int, command_args>> operators = {
        {TOK_ASTERISK,      {6, make_command<OP_CALL>("mul", 2)}},
        {TOK_SLASH,         {6, make_command<OP_CALL>("div", 2)}},
        {TOK_PLUS,          {5, make_command<OP_CALL>("add", 2)}},
        {TOK_MINUS,         {5, make_command<OP_CALL>("sub", 2)}},
        {TOK_LESS,          {4, make_command<OP_CALL>("lt", 2)}},
        {TOK_LESS_EQ,       {4, make_command<OP_CALL>("leq", 2)}},
        {TOK_GREATER,       {4, make_command<OP_CALL>("gt", 2)}},
        {TOK_GREATER_EQ,    {4, make_command<OP_CALL>("geq", 2)}},
        {TOK_EQUALS,        {3, make_command<OP_CALL>("eq", 2)}},
        {TOK_NOT_EQUALS,    {3, make_command<OP_CALL>("neq", 2)}},
        {TOK_AND,           {2, make_command<OP_CALL>("and", 2)}},
        {TOK_OR,            {1, make_command<OP_CALL>("or", 2)}},
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
        add_line<OP_CALL>("not", 1);
        break;
    case TOK_MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case TOK_INTEGER:
        case TOK_NUMBER:
            m_lexer.advance(tok_num);
            add_line<OP_PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            add_line<OP_CALL>("neg", 1);
        }
        break;
    }
    case TOK_INTEGER:
    case TOK_NUMBER:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case TOK_SLASH: 
        add_line<OP_PUSHSTR>(m_lexer.require(TOK_REGEXP).parse_string());
        break;
    case TOK_STRING:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHSTR>(tok_first.parse_string());
        break;
    case TOK_CONTENT:
        m_lexer.advance(tok_first);
        add_line<OP_PUSHVIEW>();
        break;
    default: {
        int prefixes = read_variable(true);
        if (prefixes & VP_REF) {
            add_line<OP_PUSHREF>();
        } else {
            add_line<OP_PUSHVAR>();
        }
    }
    }
}

void parser::read_variable_name() {
    bool isglobal = m_lexer.check_next(TOK_GLOBAL);
    auto tok_var = m_lexer.require(TOK_IDENTIFIER);
    add_line<OP_SELVAR>(std::string(tok_var.value), small_int(0), small_int(0), flags_t(SEL_GLOBAL & (-isglobal)));
}

int parser::read_variable(bool read_only) {
    int prefixes = 0;

    variable_selector var_idx;

    token tok_prefix;

    auto add_flags_to = [&](auto &out, auto flags, bool condition = true) {
        if ((out & flags) || !condition) throw unexpected_token(tok_prefix, TOK_IDENTIFIER);
        out |= flags;
    };
    
    bool in_loop = true;
    while (in_loop) {
        tok_prefix = m_lexer.next();
        switch (tok_prefix.type) {
        case TOK_GLOBAL:    add_flags_to(var_idx.flags, SEL_GLOBAL); break;
        case TOK_PERCENT:   add_flags_to(prefixes, VP_PARSENUM, !read_only); break;
        case TOK_CARET:     add_flags_to(prefixes, VP_AGGREGATE, !read_only); break;
        case TOK_TILDE:     add_flags_to(prefixes, VP_OVERWRITE, !read_only); break;
        case TOK_NOT:       add_flags_to(prefixes, VP_FORCE, !read_only); break;
        case TOK_AMPERSAND: add_flags_to(prefixes, VP_REF, read_only); break;
        case TOK_IDENTIFIER:
            var_idx.name = std::string(tok_prefix.value);
            in_loop = false;
            break;
        default:
            throw unexpected_token(tok_prefix, TOK_IDENTIFIER);
        }
    }

    if (m_lexer.check_next(TOK_BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case TOK_COLON: {
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            m_lexer.advance(tok);
            tok = m_lexer.peek();
            switch (tok.type) {
            case TOK_BRACKET_END:
                var_idx.flags |= SEL_EACH; // variable[:]
                break;
            case TOK_INTEGER: // variable[:N] -- append N times
                m_lexer.advance(tok);
                var_idx.flags |= SEL_APPEND;
                var_idx.length = string_toint(tok.value);
                break;
            default:
                read_expression();
                var_idx.flags |= SEL_APPEND | SEL_DYN_LEN;
            }
            break;
        }
        case TOK_BRACKET_END:
            if (read_only) throw unexpected_token(tok, TOK_INTEGER);
            var_idx.flags |= SEL_APPEND; // variable[] -- append
            break;
        default:
            if (tok = m_lexer.check_next(TOK_INTEGER)) { // variable[N]
                var_idx.index = string_toint(tok.value);
            } else {
                read_expression();
                var_idx.flags |= SEL_DYN_IDX;
            }
            if (tok = m_lexer.check_next(TOK_COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw unexpected_token(tok, TOK_BRACKET_END);
                if (tok = m_lexer.check_next(TOK_INTEGER)) {
                    var_idx.length = string_toint(tok.value);
                } else {
                    read_expression();
                    var_idx.flags |= SEL_DYN_LEN;
                }
            }
        }
        m_lexer.require(TOK_BRACKET_END);
    }

    add_line<OP_SELVAR>(var_idx);

    return prefixes;
}

void parser::read_function() {
    auto tok_fun_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_fun_name.value.substr(1);

    enum function_type {
        FUN_VARIABLE,
        FUN_GETBOX,
        FUN_VOID,
    };
    
    static const std::map<std::string, std::tuple<function_type, command_args>, std::less<>> simple_functions = {
        {"isset",       {FUN_VARIABLE,  make_command<OP_ISSET>()}},
        {"size",        {FUN_VARIABLE,  make_command<OP_GETSIZE>()}},
        {"ate",         {FUN_VOID,      make_command<OP_ATE>()}},
        {"docpages",    {FUN_VOID,      make_command<OP_DOCPAGES>()}},
        {"box",         {FUN_GETBOX,    {}}},
    };

    if (auto it = simple_functions.find(fun_name); it != simple_functions.end()) {
        switch (std::get<function_type>(it->second)) {
        case FUN_VARIABLE:
            m_lexer.require(TOK_PAREN_BEGIN);
            read_variable_name();
            m_lexer.require(TOK_PAREN_END);
            m_code.push_back(std::get<command_args>(it->second));
            break;
        case FUN_GETBOX: {
            m_lexer.require(TOK_PAREN_BEGIN);
            auto tok = m_lexer.require(TOK_IDENTIFIER);
            if (auto it = spacer_index_map.find(tok.value); it != spacer_index_map.end()) {
                m_lexer.require(TOK_PAREN_END);
                add_line<OP_GETBOX>(it->second);
            } else {
                throw unexpected_token(tok);
            }
            break;
        }
        case FUN_VOID:
            m_lexer.require(TOK_PAREN_BEGIN);
            m_lexer.require(TOK_PAREN_END);
            m_code.push_back(std::get<command_args>(it->second));
            break;
        }
    } else if (auto it = function_lookup.find(fun_name); it != function_lookup.end()) {
        small_int num_args = 0;
        m_lexer.require(TOK_PAREN_BEGIN);
        while (!m_lexer.check_next(TOK_PAREN_END)) {
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
                throw unexpected_token(tok_comma, TOK_PAREN_END);
            }
        }

        const auto &fun = it->second;
        if (num_args < fun.minargs || num_args > fun.maxargs) {
            throw invalid_numargs(std::string(fun_name), fun.minargs, fun.maxargs, tok_fun_name);
        }
        add_line<OP_CALL>(it, num_args);
    } else {
        throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
    }
}