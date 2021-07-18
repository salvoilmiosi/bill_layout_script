#include "parser.h"

#include "utils.h"

using namespace bls;

void parser::read_keyword() {
    auto make_label = [&](std::string_view label) {
        return fmt::format("__{}_{}_{}", m_parser_id, m_code.size(), label);
    };

    auto tok_name = m_lexer.require(token_type::FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    using namespace util::literals;

    switch (util::hash(fun_name)) {
    case "if"_h:
    case "ifnot"_h: {
        std::string endif_label = make_label("endif");
        std::string else_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool add_endif = false;
        while (in_loop) {
            bool has_else = false;
            else_label = make_label("else");
            m_lexer.require(token_type::PAREN_BEGIN);
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            if (condition_positive) {
                m_code.add_line<opcode::JZ>(else_label);
            } else {
                m_code.add_line<opcode::JNZ>(else_label);
            }
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == token_type::FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (util::hash(fun_name)) {
                case "else"_h:
                    m_lexer.advance(tok_if);
                    add_endif = true;
                    has_else = true;
                    m_code.add_line<opcode::JMP>(endif_label);
                    m_code.add_label(else_label);
                    read_statement();
                    in_loop = false;
                    break;
                case "elif"_h:
                case "elifnot"_h:
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    add_endif = true;
                    m_code.add_line<opcode::JMP>(endif_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_else) m_code.add_label(else_label);
        }
        if (add_endif) m_code.add_label(endif_label);
        break;
    }
    case "while"_h: {
        std::string while_label = make_label("while");
        std::string endwhile_label = make_label("endwhile");
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
        break;
    }
    case "for"_h: {
        std::string for_label = make_label("for");
        std::string endfor_label = make_label("endfor");
        m_loop_labels.push(loop_label_pair{for_label, endfor_label});
        m_lexer.require(token_type::PAREN_BEGIN);
        if (!m_lexer.check_next(token_type::SEMICOLON)) {
            sub_statement();
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
            sub_statement();
            m_lexer.require(token_type::PAREN_END);
        }
        auto increase_stmt_end = m_code.size();
        read_statement();
        m_code.move_not_comments(increase_stmt_begin, increase_stmt_end);
        m_code.add_line<opcode::JMP>(for_label);
        m_code.add_label(endfor_label);
        m_loop_labels.pop_back();
        break;
    }
    case "goto"_h: {
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        for (int i=0; i<m_content_level; ++i) {
            m_code.add_line<opcode::CNTPOP>();
        }
        m_code.add_line<opcode::JMP>(fmt::format("__{}_box_{}", m_parser_id, tok.value));
        m_lexer.require(token_type::SEMICOLON);
        break;
    }
    case "function"_h: {
        ++m_function_level;
        bool has_content = false;
        auto name = m_lexer.require(token_type::IDENTIFIER);
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
                    throw parsing_error(fmt::format("Argomento funzione duplicato: {}", tok.value), tok);
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
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

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
        break;
    }
    case "foreach"_h: {
        std::string begin_label = make_label("foreach");
        std::string continue_label = make_label("foreach_continue");
        std::string end_label = make_label("endforeach");
        m_loop_labels.push(loop_label_pair{continue_label, end_label});

        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        ++m_content_level;
        if (auto &last = m_code.last_not_comment(); last.command() == opcode::PUSHVAR) {
            last = make_command<opcode::PUSHREF>();
        }
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
        break;
    }
    case "with"_h: {
        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        ++m_content_level;
        if (auto &last = m_code.last_not_comment(); last.command() == opcode::PUSHVAR) {
            last = make_command<opcode::PUSHREF>();
        }
        m_code.add_line<opcode::CNTADDSTRING>();
        read_statement();
        --m_content_level;
        m_code.add_line<opcode::CNTPOP>();
        break;
    }
    case "lbetween"_h:
    case "rbetween"_h:
    case "lrbetween"_h:
    case "between"_h: {
        auto index_begin_fun = [&](bool is_regex = false) {
            if (fun_name == "lbetween" || fun_name == "lrbetween") {
                return is_regex ? "searchpos" : "indexof";
            } else {
                return is_regex ? "searchposend" : "indexofend";
            }
        };
        auto index_end_fun = [&](bool is_regex = false) {
            if (fun_name == "rbetween" || fun_name == "lrbetween") {
                return is_regex ? "searchposend" : "indexofend";
            } else {
                return is_regex ? "searchpos" : "indexof";
            }
        };
        m_code.add_line<opcode::PUSHVIEW>();
        m_code.add_line<opcode::CNTADDSTRING>();
        m_lexer.require(token_type::PAREN_BEGIN);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_name);
        }
        switch(auto begin_tok = m_lexer.peek(); begin_tok.type) {
        case token_type::SLASH:
            begin_tok = m_lexer.require(token_type::REGEXP);
            [[fallthrough]];
        case token_type::STRING: {
            m_lexer.advance(begin_tok);
            auto begin_str = begin_tok.parse_string();
            if (!begin_str.empty()) {
                m_code.add_line<opcode::PUSHVIEW>();
                m_code.add_line<opcode::PUSHSTR>(std::move(begin_str));
                m_code.add_line<opcode::CALL>(index_begin_fun(begin_tok.type == token_type::REGEXP), 2);
                m_code.add_line<opcode::SETBEGIN>();
            }
            break;
        }
        case token_type::FUNCTION:
            m_code.add_line<opcode::PUSHVIEW>();
            read_expression();
            m_code.add_line<opcode::CALL>(index_begin_fun(), 2);
            m_code.add_line<opcode::SETBEGIN>();
            break;
        default:
            throw unexpected_token(begin_tok, token_type::STRING);
        }
        if (m_lexer.check_next(token_type::COMMA)) {
            switch (auto end_tok = m_lexer.peek(); end_tok.type) {
            case token_type::SLASH:
                end_tok = m_lexer.require(token_type::REGEXP);
                [[fallthrough]];
            case token_type::STRING: {
                m_lexer.advance(end_tok);
                auto end_str = end_tok.parse_string();
                if (!end_str.empty()) {
                    m_code.add_line<opcode::PUSHVIEW>();
                    m_code.add_line<opcode::PUSHSTR>(std::move(end_str));
                    m_code.add_line<opcode::CALL>(index_end_fun(end_tok.type == token_type::REGEXP), 2);
                    m_code.add_line<opcode::SETEND>();
                }
                break;
            }
            case token_type::FUNCTION:
                m_code.add_line<opcode::PUSHVIEW>();
                read_expression();
                m_code.add_line<opcode::CALL>(index_end_fun(), 2);
                m_code.add_line<opcode::SETEND>();
                break;
            default:
                throw unexpected_token(end_tok, token_type::STRING);
            }
        }
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        m_code.add_line<opcode::CNTPOP>();
        break;
    }
    case "step"_h: {
        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        ++m_content_level;
        if (auto &last = m_code.last_not_comment(); last.command() == opcode::PUSHVAR) {
            last = make_command<opcode::PUSHREF>();
        }
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
        break;
    }
    case "import"_h: {
        auto tok_layout_name = m_lexer.require(token_type::STRING);
        m_lexer.require(token_type::SEMICOLON);
        auto imported_file = m_path.parent_path() / (tok_layout_name.parse_string() + ".bls");
        m_code.add_line<opcode::IMPORT>(imported_file.string());
        break;
    }
    case "break"_h:
    case "continue"_h:
        m_lexer.require(token_type::SEMICOLON);
        if (m_loop_labels.empty()) {
            throw parsing_error("Non in un loop", tok_name);
        }
        m_code.add_line<opcode::JMP>(fun_name == "break" ? m_loop_labels.top().break_label : m_loop_labels.top().continue_label);
        break;
    case "return"_h:
        if (m_function_level == 0) {
            throw parsing_error("Non in una funzione", tok_name);
        }
        if (m_lexer.check_next(token_type::SEMICOLON)) {
            m_code.add_line<opcode::RET>();
        } else {
            read_expression();
            m_lexer.require(token_type::SEMICOLON);
            auto &last = m_code.last_not_comment();
            if (last.command() == opcode::PUSHVAR) {
                last = make_command<opcode::RETVAR>();
            } else {
                m_code.add_line<opcode::RETVAL>();
            }
        }
        break;
    case "clear"_h:
        read_variable(false);
        m_lexer.require(token_type::SEMICOLON);
        m_code.add_line<opcode::CLEAR>();
        break;
    default: {
        static const std::map<std::string, std::tuple<int, command_args>, std::less<>> simple_functions = {
            {"error",       {2, make_command<opcode::THROWERROR>()}},
            {"note",        {1, make_command<opcode::ADDNOTE>()}},
            {"nexttable",   {0, make_command<opcode::NEXTTABLE>()}},
            {"firsttable",  {0, make_command<opcode::FIRSTTABLE>()}},
            {"halt",        {0, make_command<opcode::HLT>()}}
        };

        if (auto it = simple_functions.find(fun_name); it != simple_functions.end()) {
            int num_args = std::get<int>(it->second);
            if (num_args > 0) {
                m_lexer.require(token_type::PAREN_BEGIN);
                read_expression();
                for (--num_args; num_args > 0; --num_args) {
                    m_lexer.require(token_type::COMMA);
                    read_expression();
                }
                m_lexer.require(token_type::PAREN_END);
            }
            m_lexer.require(token_type::SEMICOLON);
            m_code.push_back(std::get<command_args>(it->second));
        } else if (auto it = m_functions.find(fun_name); it != m_functions.end()) {
            m_lexer.require(token_type::PAREN_BEGIN);
            small_int num_args = 0;
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
            m_lexer.require(token_type::SEMICOLON);
            
            if (it->second.numargs != num_args) {
                throw invalid_numargs(std::string(fun_name), it->second.numargs, it->second.numargs, tok_name);
            }
            if (it->second.has_contents && m_content_level == 0) {
                throw parsing_error(fmt::format("Impossibile chiamare {}, stack contenuti vuoto", fun_name), tok_name);
            }
            m_code.add_line<opcode::JSR>(fmt::format("__function_{}", fun_name), num_args);
        } else {
            throw parsing_error("Parola chiave sconosciuta", tok_name);
        }
    }
    }
}