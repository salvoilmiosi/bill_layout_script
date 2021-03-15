#include "parser.h"

#include "utils.h"

static inline std::string make_label(std::string_view name) {
    static int n = 0;
    return fmt::format("__{0}_{1}", name, n++);
}

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    enum function_type {
        FUN_EXPRESSION,
        FUN_VARIABLE,
        FUN_VOID
    };
    
    static const std::map<std::string, std::tuple<function_type, command_args>, std::less<>> simple_functions = {
        {"error",       {FUN_EXPRESSION, make_command<OP_THROWERROR>()}},
        {"warning",     {FUN_EXPRESSION, make_command<OP_WARNING>()}},
        {"setbegin",    {FUN_EXPRESSION, make_command<OP_SETBEGIN>()}},
        {"setend",      {FUN_EXPRESSION, make_command<OP_SETEND>()}},
        {"clear",       {FUN_VARIABLE,   make_command<OP_CLEAR>()}},
        {"nexttable",   {FUN_VOID,       make_command<OP_NEXTTABLE>()}},
        {"skip",        {FUN_VOID,       make_command<OP_NOP>()}},
        {"return",      {FUN_VOID,       make_command<OP_RET>()}},
        {"halt",        {FUN_VOID,       make_command<OP_HLT>()}},
    };

    if (auto it = simple_functions.find(fun_name); it != simple_functions.end()) {
        m_lexer.require(TOK_PAREN_BEGIN);
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
        m_lexer.require(TOK_PAREN_END);
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
            m_lexer.require(TOK_PAREN_BEGIN);
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<OP_UNEVAL_JUMP>(condition_positive ? OP_JZ : OP_JNZ, else_label);
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == TOK_FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance(tok_if);
                    add_endif = true;
                    has_else = true;
                    add_line<OP_UNEVAL_JUMP>(OP_JMP, endif_label);
                    add_label(else_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    add_endif = true;
                    add_line<OP_UNEVAL_JUMP>(OP_JMP, endif_label);
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
        m_lexer.require(TOK_PAREN_BEGIN);
        add_label(while_label);
        read_expression();
        add_line<OP_UNEVAL_JUMP>(OP_JZ, endwhile_label);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line<OP_UNEVAL_JUMP>(OP_JMP, while_label);
        add_label(endwhile_label);
        m_loop_labels.pop();
        break;
    }
    case hash("for"): {
        std::string for_label = make_label("for");
        std::string endfor_label = make_label("endfor");
        m_loop_labels.push(loop_label_pair{for_label, endfor_label});
        m_lexer.require(TOK_PAREN_BEGIN);
        read_statement();
        m_lexer.require(TOK_COMMA);
        add_label(for_label);
        read_expression();
        add_line<OP_UNEVAL_JUMP>(OP_JZ, endfor_label);
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = m_code.size();
        read_statement();
        size_t increase_stmt_end = m_code.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        move_to_end(m_code, increase_stmt_begin, increase_stmt_end);
        add_line<OP_UNEVAL_JUMP>(OP_JMP, for_label);
        add_label(endfor_label);
        m_loop_labels.pop();
        break;
    }
    case hash("goto"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line<OP_UNEVAL_JUMP>(OP_JMP, fmt::format("__label_{}", tok.value));
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("function"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto name = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

        add_line<OP_UNEVAL_JUMP>(OP_JMP, endfun_label);
        add_label(fun_label);
        read_statement();
        add_line<OP_RET>();
        add_label(endfun_label);
        break;
    }
    case hash("call"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line<OP_UNEVAL_JUMP>(OP_JSR, fmt::format("__function_{}", tok.value));
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("foreach"): {
        std::string begin_label = make_label("foreach");
        std::string continue_label = make_label("foreach_continue");
        std::string end_label = make_label("endforeach");
        m_loop_labels.push(loop_label_pair{continue_label, end_label});
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<OP_ADDCONTENT>();
            pushed_content = true;
        }
        add_line<OP_SPLITVIEW>();
        add_label(begin_label);
        read_statement();
        add_label(continue_label);
        add_line<OP_NEXTRESULT>();
        add_line<OP_UNEVAL_JUMP>(OP_JNTE, begin_label);
        add_label(end_label);
        if (pushed_content) {
            add_line<OP_POPCONTENT>();
        } else {
            add_line<OP_RESETVIEW>();
        }
        m_loop_labels.pop();
        break;
    }
    case hash("with"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line<OP_ADDCONTENT>();
        read_statement();
        add_line<OP_POPCONTENT>();
        break;
    }
    case hash("between"): {
        add_line<OP_NEWVIEW>();
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line<OP_PUSHVIEW>();
        auto begin_str = m_lexer.require(TOK_STRING).parse_string();
        int begin_len = begin_str.size();
        add_line<OP_PUSHSTR>(std::move(begin_str));
        add_line<OP_CALL>("indexof", 2);
        add_line<OP_SETBEGIN>();
        if (m_lexer.check_next(TOK_COMMA)) {
            add_line<OP_PUSHVIEW>();
            add_line<OP_PUSHSTR>(m_lexer.require(TOK_STRING).parse_string());   
            add_line<OP_PUSHNUM>(begin_len);
            add_line<OP_CALL>("indexof", 3);
            add_line<OP_SETEND>();
        }
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line<OP_RESETVIEW>();
        break;
    }
    case hash("step"): {
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<OP_ADDCONTENT>();
            pushed_content = true;
        }

        add_line<OP_SPLITVIEW>();
        
        m_lexer.require(TOK_BRACE_BEGIN);
        while (true) {
            read_statement();
            if (! m_lexer.check_next(TOK_BRACE_END)) {
                add_line<OP_NEXTRESULT>();
            } else {
                break;
            }
        }

        if (pushed_content) {
            add_line<OP_POPCONTENT>();
        } else {
            add_line<OP_RESETVIEW>();
        }
        break;
    }
    case hash("newview"):
        add_line<OP_NEWVIEW>();
        read_statement();
        add_line<OP_RESETVIEW>();
        break;
    case hash("import"):
    case hash("setlayout"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(TOK_STRING);
        m_lexer.require(TOK_PAREN_END);
        auto imported_file = m_path / (tok_layout_name.parse_string() + ".bls");
        flags_t flags = 0;
        if (m_flags & PARSER_RECURSIVE_IMPORTS) {
            flags |= IMPORT_IGNORE;
        }
        if (fun_name == "setlayout") {
            flags |= IMPORT_SETLAYOUT;
        }
        add_line<OP_IMPORT>(imported_file, flags);
        if (m_flags & PARSER_RECURSIVE_IMPORTS) {
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
        if (flags & IMPORT_SETLAYOUT) {
            add_line<OP_HLT>();
        } else if (intl::valid_language(m_layout->language_code)) {
            add_line<OP_SETLANG>(m_layout->language_code);
        }
        break;
    }
    case hash("break"):
    case hash("continue"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        if (m_loop_labels.empty()) {
            throw parsing_error("Non in un loop", tok_name);
        }
        add_line<OP_UNEVAL_JUMP>(OP_JMP, fun_name == "break" ? m_loop_labels.top().break_label : m_loop_labels.top().continue_label);
        break;
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}