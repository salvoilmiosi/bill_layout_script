#include "parser.h"

#include "utils.h"

using namespace bls;

jump_label parser::make_label(std::string_view label) {
    return std::format("__{}_{}_{}", m_parser_id, m_code.size(), label);
};

void parser::parse_if_stmt() {
    m_lexer.require(token_type::KW_IF);

    auto endif_label = make_label("endif");
    auto else_label = make_label("else");

    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    if (auto &last = m_code.last_not_comment(); last.command() == opcode::CALL && last.get_args<opcode::CALL>().fun->first == "not") {
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

    auto while_label = make_label("while");
    auto endwhile_label = make_label("endwhile");
    m_loop_labels.push(loop_label_pair{while_label, endwhile_label});
    m_lexer.require(token_type::PAREN_BEGIN);
    m_code.add_label(while_label);
    read_expression();
    m_code.add_line<opcode::JZ>(endwhile_label);
    m_lexer.require(token_type::PAREN_END);
    read_statement();
    m_code.add_line<opcode::JMP>(while_label);
    m_code.add_label(endwhile_label);
    m_loop_labels.pop_back();
};

void parser::parse_for_stmt() {
    m_lexer.require(token_type::KW_FOR);

    auto for_label = make_label("for");
    auto endfor_label = make_label("endfor");
    m_loop_labels.push(loop_label_pair{for_label, endfor_label});
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
    auto increase_stmt_begin = m_code.size();
    if (!m_lexer.check_next(token_type::PAREN_END)) {
        assignment_stmt();
        m_lexer.require(token_type::PAREN_END);
    }
    auto increase_stmt_end = m_code.size();
    read_statement();
    m_code.move_not_comments(increase_stmt_begin, increase_stmt_end);
    m_code.add_line<opcode::JMP>(for_label);
    m_code.add_label(endfor_label);
    m_loop_labels.pop_back();
};

void parser::parse_goto_stmt() {
    m_lexer.require(token_type::KW_GOTO);

    auto tok = m_lexer.require(token_type::IDENTIFIER);
    for (int i=0; i<m_content_level; ++i) {
        m_code.add_line<opcode::CNTPOP>();
    }
    m_code.add_line<opcode::JMP>(std::format("__{}_box_{}", m_parser_id, tok.value));
    m_lexer.require(token_type::SEMICOLON);
};

void parser::parse_function_stmt() {
    m_lexer.require(token_type::KW_FUNCTION);

    ++m_function_level;
    bool has_content = false;
    auto name = m_lexer.require(token_type::IDENTIFIER);
    if (function_lookup::valid(function_lookup::find(name.value)) || m_functions.contains(name.value)) {
        throw parsing_error(intl::format("DUPLICATE_FUNCTION_NAME", name.value), name);
    }
    m_lexer.require(token_type::PAREN_BEGIN);
    small_int num_args = 0;
    while (!m_lexer.check_next(token_type::PAREN_END)) {
        auto tok = m_lexer.next();
        switch (tok.type) {
        case token_type::CONTENT:
            if (num_args == 0 && !has_content) {
                has_content = true;
                ++m_content_level;
            } else {
                throw unexpected_token(tok, token_type::IDENTIFIER);
            }
            break;
        case token_type::IDENTIFIER:
            if (std::ranges::find(m_fun_args, tok.value) != m_fun_args.end()) {
                throw parsing_error(intl::format("DUPLICATE_FUNCTION_ARG_NAME", tok.value), tok);
            }
            m_fun_args.emplace_back(tok.value);
            ++num_args;
            break;
        default:
            throw unexpected_token(tok, token_type::IDENTIFIER);
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
    
    std::string fun_label = std::format("__function_{}", name.value);
    std::string endfun_label = std::format("__endfunction_{}", name.value);

    m_code.add_line<opcode::JMP>(endfun_label);

    m_code.add_label(fun_label);
    m_functions.emplace(std::string(name.value), function_info{num_args, has_content});

    read_statement();
    m_code.add_line<opcode::RET>();
    m_code.add_label(endfun_label);
    m_fun_args.resize(m_fun_args.size() - num_args);
    if (has_content) {
        --m_content_level;
    }
    --m_function_level;
};

void parser::parse_foreach_stmt() {
    m_lexer.require(token_type::KW_FOREACH);

    auto begin_label = make_label("foreach");
    auto continue_label = make_label("foreach_continue");
    auto end_label = make_label("endforeach");
    m_loop_labels.push(loop_label_pair{continue_label, end_label});

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
    m_loop_labels.pop_back();
};

void parser::parse_with_stmt() {
    m_lexer.require(token_type::KW_WITH);
    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    ++m_content_level;
    m_code.add_line<opcode::CNTADDSTRING>();
    read_statement();
    --m_content_level;
    m_code.add_line<opcode::CNTPOP>();
};

void parser::parse_step_stmt() {
    m_lexer.require(token_type::KW_STEP);
    m_lexer.require(token_type::PAREN_BEGIN);
    read_expression();
    m_lexer.require(token_type::PAREN_END);
    ++m_content_level;
    m_code.add_line<opcode::CNTADDLIST>();

    m_lexer.require(token_type::BRACE_BEGIN);
    
    while (true) {
        read_statement();
        if (! m_lexer.check_next(token_type::BRACE_END)) {
            m_code.add_line<opcode::NEXTRESULT>();
        } else {
            break;
        }
    }

    --m_content_level;
    m_code.add_line<opcode::CNTPOP>();
};

void parser::parse_import_stmt() {
    m_lexer.require(token_type::KW_IMPORT);
    auto tok_layout_name = m_lexer.require(token_type::STRING);
    m_lexer.require(token_type::SEMICOLON);
    auto imported_file = m_path.parent_path() / (tok_layout_name.parse_string() + ".bls");
    m_code.add_line<opcode::IMPORT>(imported_file.string());
};

void parser::parse_break_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_BREAK);
    m_lexer.require(token_type::SEMICOLON);
    if (m_loop_labels.empty()) {
        throw parsing_error(intl::format("NOT_IN_A_LOOP"), tok_fun_name);
    }
    m_code.add_line<opcode::JMP>(m_loop_labels.top().break_label);
};

void parser::parse_continue_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_CONTINUE);
    m_lexer.require(token_type::SEMICOLON);
    if (m_loop_labels.empty()) {
        throw parsing_error(intl::format("NOT_IN_A_LOOP"), tok_fun_name);
    }
    m_code.add_line<opcode::JMP>(m_loop_labels.top().continue_label);
};

void parser::parse_return_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_RETURN);
    if (m_function_level == 0) {
        throw parsing_error(intl::format("NOT_IN_A_FUNCTION"), tok_fun_name);
    }
    if (m_lexer.check_next(token_type::SEMICOLON)) {
        m_code.add_line<opcode::RET>();
    } else {
        read_expression();
        m_lexer.require(token_type::SEMICOLON);
        m_code.add_line<opcode::RETVAL>();
    }
};

void parser::parse_clear_stmt() {
    m_lexer.require(token_type::KW_CLEAR);
    read_variable_name(true);
    m_lexer.require(token_type::SEMICOLON);
    m_code.add_line<opcode::CLEAR>();
};

void parser::parse_set_stmt() {
    auto tok_fun_name = m_lexer.require(token_type::KW_SET);
    if (m_content_level == 0) {
        throw parsing_error(intl::format("EMPTY_CONTENT_STACK"), tok_fun_name);
    }
    auto prefixes = read_variable(false);
    m_lexer.require(token_type::SEMICOLON);
    m_code.add_line<opcode::PUSHVIEW>();
    if (prefixes.call.command() != opcode::NOP) {
        m_code.push_back(prefixes.call);
    }
    m_code.add_line<opcode::SETVAR>();
}