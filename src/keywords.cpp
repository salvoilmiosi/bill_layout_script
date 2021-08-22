#include "parser.h"

#include "utils.h"

using namespace bls;

void parser::parse_if_stmt() {
    m_lexer.require(token_type::KW_IF);

    auto endif_label = m_code.make_label();
    auto else_label = m_code.make_label();

    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    if (auto &last = m_code.last_not_comment(); last.command() == opcode::CALL && last.get_args<opcode::CALL>()->first == "not") {
        last = make_command<opcode::JNZ>(else_label);
    } else {
        m_code.add_line<opcode::JZ>(else_label);
    }
    read_statement();
    if (m_lexer.check_next(token_type::KW_ELSE)) {
        m_code.add_line<opcode::JMP>(endif_label);
        m_code.add_label(else_label);
        read_statement();
        m_code.add_label(endif_label);
    } else {
        m_code.add_label(else_label);
    }
};

void parser::parse_while_stmt() {
    m_lexer.require(token_type::KW_WHILE);

    auto while_label = m_code.make_label();
    auto endwhile_label = m_code.make_label();
    m_loop_stack.emplace(while_label, endwhile_label, m_content_level);
    m_lexer.require(token_type::PAREN_BEGIN);
    m_code.add_label(while_label);
    read_expression();
    m_code.add_line<opcode::JZ>(endwhile_label);
    m_lexer.require(token_type::PAREN_END);
    read_statement();
    m_code.add_line<opcode::JMP>(while_label);
    m_code.add_label(endwhile_label);
    m_loop_stack.pop_back();
};

void parser::parse_for_stmt() {
    m_lexer.require(token_type::KW_FOR);

    auto for_label = m_code.make_label();
    auto endfor_label = m_code.make_label();
    m_loop_stack.emplace(for_label, endfor_label, m_content_level);
    m_lexer.require(token_type::PAREN_BEGIN);
    if (!m_lexer.check_next(token_type::SEMICOLON)) {
        assignment_stmt();
        m_lexer.require(token_type::SEMICOLON);
    }
    m_code.add_label(for_label);
    if (!m_lexer.check_next(token_type::SEMICOLON)) {
        read_expression();
        m_code.add_line<opcode::JZ>(endfor_label);
        m_lexer.require(token_type::SEMICOLON);
    }
    auto increase_stmt_begin = std::prev(m_code.end());
    if (!m_lexer.check_next(token_type::PAREN_END)) {
        assignment_stmt();
        m_lexer.require(token_type::PAREN_END);
    }
    auto increase_stmt_end = std::prev(m_code.end());
    read_statement();
    m_code.splice(m_code.end(), m_code, std::next(increase_stmt_begin), std::next(increase_stmt_end));
    m_code.add_line<opcode::JMP>(for_label);
    m_code.add_label(endfor_label);
    m_loop_stack.pop_back();
};

void parser::parse_goto_stmt() {
    m_lexer.require(token_type::KW_GOTO);

    auto tok = m_lexer.require(token_type::IDENTIFIER);
    for (int i=0; i<m_content_level; ++i) {
        m_code.add_line<opcode::CNTPOP>();
    }
    if (auto it = m_goto_labels.find(tok.value); it != m_goto_labels.end()) {
        m_code.add_line<opcode::JMP>(it->second);
    } else {
        throw token_error(intl::format("UNKNOWN_LABEL", tok.value), tok);
    }
    m_lexer.require(token_type::SEMICOLON);
};

void parser::parse_function_stmt() {
    m_lexer.require(token_type::KW_FUNCTION);

    auto name = m_lexer.require(token_type::IDENTIFIER);
    if (function_lookup::valid(function_lookup::find(name.value)) || m_functions.contains(name.value)) {
        throw token_error(intl::format("DUPLICATE_FUNCTION_NAME", name.value), name);
    }
    m_lexer.require(token_type::PAREN_BEGIN);
    
    auto fun_label = m_code.make_label();
    auto endfun_label = m_code.make_label();

    m_code.add_line<opcode::JMP>(endfun_label);
    m_code.add_label(fun_label);

    auto old_content_level = m_content_level;
    m_content_level = 0;

    std::set<std::string_view> args;
    while (!m_lexer.check_next(token_type::PAREN_END)) {
        auto tok = m_lexer.next();
        switch (tok.type) {
        case token_type::CONTENT:
            if (args.empty() && m_content_level == 0) {
                ++m_content_level;
            } else {
                throw unexpected_token(tok, token_type::IDENTIFIER);
            }
            break;
        case token_type::DOLLAR: {
            auto tok = m_lexer.require(token_type::IDENTIFIER);
            if (args.insert(tok.value).second) {
                m_code.add_line<opcode::SELLOCAL>(tok.value);
            } else {
                throw token_error(intl::format("DUPLICATE_FUNCTION_ARG_NAME", tok.value), tok);
            }
            break;
        }
        default:
            throw unexpected_token(tok);
        }
        tok = m_lexer.peek();
        switch (tok.type) {
        case token_type::COMMA:
            m_lexer.advance(tok);
            break;
        case token_type::PAREN_END:
            break;
        default:
            throw unexpected_token(tok, token_type::PAREN_END);
        }
    }

    m_functions.emplace(std::string(name.value), function_info{fun_label, args.size() + m_content_level});
    for (auto _ : args) {
        m_code.add_line<opcode::FWDVAR>();
    }
    if (m_content_level > 0) {
        m_code.add_line<opcode::CNTADD>();
    }

    read_statement();

    if (m_content_level > 0) {
        m_code.add_line<opcode::CNTPOP>();
    }
    m_code.add_line<opcode::RET>();
    m_code.add_label(endfun_label);
    m_content_level = old_content_level;
};

void parser::parse_foreach_stmt() {
    m_lexer.require(token_type::KW_FOREACH);

    auto begin_label = m_code.make_label();
    auto continue_label = m_code.make_label();
    auto end_label = m_code.make_label();
    m_loop_stack.emplace(continue_label, end_label, m_content_level);

    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    ++m_content_level;
    m_code.add_line<opcode::CNTADDLIST>();
    m_code.add_label(begin_label);
    m_code.add_line<opcode::JTE>(end_label);
    read_statement();
    m_code.add_label(continue_label);
    m_code.add_line<opcode::NEXTRESULT>();
    m_code.add_line<opcode::JMP>(begin_label);
    m_code.add_label(end_label);
    --m_content_level;
    m_code.add_line<opcode::CNTPOP>();
    m_loop_stack.pop_back();
};

void parser::parse_with_stmt() {
    m_lexer.require(token_type::KW_WITH);
    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    ++m_content_level;
    m_code.add_line<opcode::CNTADD>();
    read_statement();
    --m_content_level;
    m_code.add_line<opcode::CNTPOP>();
};

void parser::parse_import_stmt() {
    m_lexer.require(token_type::KW_IMPORT);
    auto tok_layout_name = m_lexer.require(token_type::STRING);
    m_lexer.require(token_type::SEMICOLON);
    auto imported_file = m_path.parent_path() / (tok_layout_name.parse_string() + ".bls");
    m_code.add_line<opcode::IMPORT>(imported_file.string());
    m_code.add_line<opcode::SETPATH>(m_path.string());
    m_code.add_line<opcode::SETLANG>(m_lang);
};

void parser::parse_break_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_BREAK);
    m_lexer.require(token_type::SEMICOLON);
    if (m_loop_stack.empty()) {
        throw token_error(intl::format("NOT_IN_A_LOOP"), tok_fun_name);
    }
    for (int i = m_loop_stack.top().entry_content_level; i < m_content_level; ++i) {
        m_code.add_line<opcode::CNTPOP>();
    }
    m_code.add_line<opcode::JMP>(m_loop_stack.top().break_node);
};

void parser::parse_continue_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_CONTINUE);
    m_lexer.require(token_type::SEMICOLON);
    if (m_loop_stack.empty()) {
        throw token_error(intl::format("NOT_IN_A_LOOP"), tok_fun_name);
    }
    for (int i = m_loop_stack.top().entry_content_level; i < m_content_level; ++i) {
        m_code.add_line<opcode::CNTPOP>();
    }
    m_code.add_line<opcode::JMP>(m_loop_stack.top().continue_node);
};

void parser::parse_return_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_RETURN);
    if (m_lexer.check_next(token_type::SEMICOLON)) {
        for (int i = 0; i < m_content_level; ++i) {
            m_code.add_line<opcode::CNTPOP>();
        }
        m_code.add_line<opcode::RET>();
    } else {
        read_expression();
        switch (m_code.last_not_comment().command()) {
        case opcode::PUSHVAR:
        case opcode::PUSHVIEW:
            m_code.add_line<opcode::CALL>("copy");
        }
        for (int i = 0; i < m_content_level; ++i) {
            m_code.add_line<opcode::CNTPOP>();
        }
        m_code.add_line<opcode::RETVAL>();
        m_lexer.require(token_type::SEMICOLON);
    }
};

void parser::parse_clear_stmt() {
    m_lexer.require(token_type::KW_CLEAR);
    read_variable_name();
    m_lexer.require(token_type::SEMICOLON);
    m_code.add_line<opcode::CLEAR>();
};

void parser::parse_tie_stmt() {
    m_lexer.require(token_type::KW_TIE);
    m_lexer.require(token_type::PAREN_BEGIN);
    auto var_begin = std::prev(m_code.end());
    size_t num_vars = 0;
    while (!m_lexer.check_next(token_type::PAREN_END)) {
        ++num_vars;
        auto var_end = std::prev(m_code.end());
        read_variable_name();
        read_variable_indices();
        m_code.splice(m_code.end(), m_code, std::next(var_begin), std::next(var_end));
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

    command_args op_cmd;
    token tok = m_lexer.next();
    switch(tok.type) {
    case token_type::ASSIGN:        op_cmd = make_command<opcode::SETVAR>(); break;
    case token_type::FORCE_ASSIGN:  op_cmd = make_command<opcode::FORCEVAR>(); break;
    case token_type::ADD_ASSIGN:    op_cmd = make_command<opcode::INCVAR>(); break;
    case token_type::SUB_ASSIGN:    op_cmd = make_command<opcode::DECVAR>(); break;
    default:
        throw unexpected_token(tok, token_type::ASSIGN);
    }
    read_expression();
    m_code.add_line<opcode::CNTADDLIST>();
    for(size_t i=0; i<num_vars; ++i) {
        m_code.add_line<opcode::PUSHVIEW>();
        m_code.push_back(op_cmd);
        if (i != num_vars - 1) {
            m_code.add_line<opcode::NEXTRESULT>();
        }
    }
    m_code.add_line<opcode::CNTPOP>();
}