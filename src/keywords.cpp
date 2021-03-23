#include "parser.h"

#include "utils.h"

static inline std::string make_label(std::string_view name) {
    static int n = 0;
    return fmt::format("__{0}_{1}", name, n++);
}

void parser::read_keyword() {
    auto tok_name = m_lexer.require(token_type::FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    enum function_type {
        FUN_EXPRESSION,
        FUN_VARIABLE,
        FUN_VOID,
    };
    
    static const std::map<std::string, std::tuple<function_type, command_args>, std::less<>> simple_functions = {
        {"error",       {FUN_EXPRESSION, make_command<opcode::THROWERROR>()}},
        {"warning",     {FUN_EXPRESSION, make_command<opcode::WARNING>()}},
        {"setbegin",    {FUN_EXPRESSION, make_command<opcode::SETBEGIN>()}},
        {"setend",      {FUN_EXPRESSION, make_command<opcode::SETEND>()}},
        {"clear",       {FUN_VARIABLE,   make_command<opcode::CLEAR>()}},
        {"nexttable",   {FUN_VOID,       make_command<opcode::NEXTTABLE>()}},
        {"skip",        {FUN_VOID,       make_command<opcode::NOP>()}},
        {"halt",        {FUN_VOID,       make_command<opcode::HLT>()}}
    };

    if (auto it = simple_functions.find(fun_name); it != simple_functions.end()) {
        m_lexer.require(token_type::PAREN_BEGIN);
        switch (std::get<function_type>(it->second)) {
        case FUN_EXPRESSION:
            read_expression();
            break;
        case FUN_VARIABLE:
            read_variable_name();
            break;
        case FUN_VOID:
            break;
        }
        m_lexer.require(token_type::PAREN_END);
        m_code.push_back(std::get<command_args>(it->second));
    } else switch (hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"): {
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
                add_jump<opcode::JZ>(else_label);
            } else {
                add_jump<opcode::JNZ>(else_label);
            }
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == token_type::FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance(tok_if);
                    add_endif = true;
                    has_else = true;
                    add_jump<opcode::JMP>(endif_label);
                    add_label(else_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    add_endif = true;
                    add_jump<opcode::JMP>(endif_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_else) add_label(else_label);
        }
        if (add_endif) add_label(endif_label);
        break;
    }
    case hash("while"): {
        std::string while_label = make_label("while");
        std::string endwhile_label = make_label("endwhile");
        m_loop_labels.push(loop_label_pair{while_label, endwhile_label});
        m_lexer.require(token_type::PAREN_BEGIN);
        add_label(while_label);
        read_expression();
        add_jump<opcode::JZ>(endwhile_label);
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        add_jump<opcode::JMP>(while_label);
        add_label(endwhile_label);
        m_loop_labels.pop();
        break;
    }
    case hash("for"): {
        std::string for_label = make_label("for");
        std::string endfor_label = make_label("endfor");
        m_loop_labels.push(loop_label_pair{for_label, endfor_label});
        m_lexer.require(token_type::PAREN_BEGIN);
        read_statement();
        m_lexer.require(token_type::COMMA);
        add_label(for_label);
        read_expression();
        add_jump<opcode::JZ>(endfor_label);
        m_lexer.require(token_type::COMMA);
        size_t increase_stmt_begin = m_code.size();
        read_statement();
        size_t increase_stmt_end = m_code.size();
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        move_to_end(m_code, increase_stmt_begin, increase_stmt_end);
        add_jump<opcode::JMP>(for_label);
        add_label(endfor_label);
        m_loop_labels.pop();
        break;
    }
    case hash("goto"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok = m_lexer.require(token_type::IDENTIFIER);
        add_jump<opcode::JMP>(fmt::format("__label_{}", tok.value));
        m_lexer.require(token_type::PAREN_END);
        break;
    }
    case hash("function"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto name = m_lexer.require(token_type::IDENTIFIER);
        m_lexer.require(token_type::PAREN_END);
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

        add_jump<opcode::JMP>(endfun_label);
        add_label(fun_label);
        read_statement();
        add_line<opcode::RET>();
        add_label(endfun_label);
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
        add_jump<opcode::JSR>(fmt::format("__function_{}", tok.value), numargs);
        break;
    }
    case hash("foreach"): {
        std::string begin_label = make_label("foreach");
        std::string continue_label = make_label("foreach_continue");
        std::string end_label = make_label("endforeach");
        m_loop_labels.push(loop_label_pair{continue_label, end_label});
        bool pushed_content = false;
        if (m_lexer.check_next(token_type::PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            ++m_content_level;
            add_line<opcode::ADDCONTENT>();
            pushed_content = true;
        }
        add_line<opcode::SPLITVIEW>();
        add_label(begin_label);
        read_statement();
        add_label(continue_label);
        add_line<opcode::NEXTRESULT>();
        add_jump<opcode::JNTE>(begin_label);
        add_label(end_label);
        if (pushed_content) {
            --m_content_level;
            add_line<opcode::POPCONTENT>();
        } else {
            add_line<opcode::RESETVIEW>();
        }
        m_loop_labels.pop();
        break;
    }
    case hash("with"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);
        ++m_content_level;
        add_line<opcode::ADDCONTENT>();
        read_statement();
        --m_content_level;
        add_line<opcode::POPCONTENT>();
        break;
    }
    case hash("between"): {
        add_line<opcode::NEWVIEW>();
        m_lexer.require(token_type::PAREN_BEGIN);
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_name);
        }
        add_line<opcode::PUSHVIEW>();
        auto begin_str = m_lexer.require(token_type::STRING).parse_string();
        int begin_len = begin_str.size();
        add_line<opcode::PUSHSTR>(std::move(begin_str));
        add_line<opcode::CALL>("indexof", 2);
        add_line<opcode::SETBEGIN>();
        if (m_lexer.check_next(token_type::COMMA)) {
            add_line<opcode::PUSHVIEW>();
            add_line<opcode::PUSHSTR>(m_lexer.require(token_type::STRING).parse_string());   
            add_line<opcode::PUSHNUM>(begin_len);
            add_line<opcode::CALL>("indexof", 3);
            add_line<opcode::SETEND>();
        }
        m_lexer.require(token_type::PAREN_END);
        read_statement();
        add_line<opcode::RESETVIEW>();
        break;
    }
    case hash("step"): {
        bool pushed_content = false;
        if (m_lexer.check_next(token_type::PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            ++m_content_level;
            add_line<opcode::ADDCONTENT>();
            pushed_content = true;
        }

        m_lexer.require(token_type::BRACE_BEGIN);
        add_line<opcode::SPLITVIEW>();
        
        while (true) {
            read_statement();
            if (! m_lexer.check_next(token_type::BRACE_END)) {
                add_line<opcode::NEXTRESULT>();
            } else {
                break;
            }
        }

        if (pushed_content) {
            --m_content_level;
            add_line<opcode::POPCONTENT>();
        } else {
            add_line<opcode::RESETVIEW>();
        }
        break;
    }
    case hash("newview"):
        add_line<opcode::NEWVIEW>();
        read_statement();
        add_line<opcode::RESETVIEW>();
        break;
    case hash("import"):
    case hash("setlayout"): {
        m_lexer.require(token_type::PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(token_type::STRING);
        m_lexer.require(token_type::PAREN_END);
        auto imported_file = m_path / (tok_layout_name.parse_string() + ".bls");
        bitset<import_flags> flags;
        if (m_flags & parser_flags::RECURSIVE_IMPORTS) {
            flags |= import_flags::NOIMPORT;
        }
        if (fun_name == "setlayout") {
            flags |= import_flags::SETLAYOUT;
        }
        add_line<opcode::IMPORT>(imported_file, flags);
        if (m_flags & parser_flags::RECURSIVE_IMPORTS) {
            parser imported;
            imported.m_flags = m_flags;
            imported.read_layout(imported_file.parent_path(), box_vector::from_file(imported_file));
            auto code_len = m_code.size();
            std::ranges::move(imported.m_code, std::back_inserter(m_code));
            for (auto &[line, comment] : imported.m_comments) {
                m_comments.emplace(code_len + line, std::move(comment));
            }
            for (auto &[label, line] : imported.m_labels) {
                m_labels.emplace(label, code_len + line);
            }
        }
        if (flags & import_flags::SETLAYOUT) {
            add_line<opcode::HLT>();
        } else if (intl::valid_language(m_layout->language_code)) {
            add_line<opcode::SETLANG>(m_layout->language_code);
        }
        break;
    }
    case hash("break"):
    case hash("continue"):
        m_lexer.require(token_type::PAREN_BEGIN);
        m_lexer.require(token_type::PAREN_END);
        if (m_loop_labels.empty()) {
            throw parsing_error("Non in un loop", tok_name);
        }
        add_jump<opcode::JMP>(fun_name == "break" ? m_loop_labels.top().break_label : m_loop_labels.top().continue_label);
        break;
    case hash("return"):
        m_lexer.require(token_type::PAREN_BEGIN);
        if (m_lexer.check_next(token_type::PAREN_END)) {
            add_line<opcode::RET>();
        } else {
            read_expression();
            m_lexer.require(token_type::PAREN_END);
            add_line<opcode::RETVAL>();
        }
        break;
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}