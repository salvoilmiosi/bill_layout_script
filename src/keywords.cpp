#include "parser.h"

#include "utils.h"

static inline std::string make_label(std::string_view name) {
    static int n = 0;
    return fmt::format("__{0}_{1}", name, n++);
}

void parser::read_keyword() {
    auto tok_name = m_lexer.require(TOK_FUNCTION);
    auto fun_name = tok_name.value.substr(1);

    switch(hash(fun_name)) {
    case hash("if"):
    case hash("ifnot"): {
        std::string endelse_label = make_label("endelse");
        std::string endif_label;
        bool condition_positive = fun_name == "if";
        bool in_loop = true;
        bool has_endelse = false;
        while (in_loop) {
            bool has_endif = false;
            endif_label = make_label("endif");
            m_lexer.require(TOK_PAREN_BEGIN);
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<opcode::UNEVAL_JUMP>(condition_positive ? opcode::JZ : opcode::JNZ, endif_label);
            read_statement();
            auto tok_if = m_lexer.peek();
            if (tok_if.type == TOK_FUNCTION) {
                fun_name = tok_if.value.substr(1);
                switch (hash(fun_name)) {
                case hash("else"):
                    m_lexer.advance(tok_if);
                    has_endelse = true;
                    has_endif = true;
                    add_line<opcode::UNEVAL_JUMP>(opcode::JMP, endelse_label);
                    add_label(endif_label);
                    read_statement();
                    in_loop = false;
                    break;
                case hash("elif"):
                case hash("elifnot"):
                    m_lexer.advance(tok_if);
                    condition_positive = fun_name == "elif";
                    has_endelse = true;
                    add_line<opcode::UNEVAL_JUMP>(opcode::JMP, endelse_label);
                    break;
                default:
                    in_loop = false;
                }
            } else {
                in_loop = false;
            }
            if (!has_endif) add_label(endif_label);
        }
        if (has_endelse) add_label(endelse_label);
        break;
    }
    case hash("while"): {
        std::string while_label = make_label("while");
        std::string endwhile_label = make_label("endwhile");
        m_loop_labels.push(loop_label_pair{while_label, endwhile_label});
        m_lexer.require(TOK_PAREN_BEGIN);
        add_label(while_label);
        read_expression();
        add_line<opcode::UNEVAL_JUMP>(opcode::JZ, endwhile_label);
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, while_label);
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
        add_line<opcode::UNEVAL_JUMP>(opcode::JZ, endfor_label);
        m_lexer.require(TOK_COMMA);
        size_t increase_stmt_begin = m_code.size();
        read_statement();
        size_t increase_stmt_end = m_code.size();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        vector_move_to_end(m_code, increase_stmt_begin, increase_stmt_end);
        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, for_label);
        add_label(endfor_label);
        m_loop_labels.pop();
        break;
    }
    case hash("goto"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, fmt::format("__label_{}", tok.value));
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("function"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto name = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        
        std::string fun_label = fmt::format("__function_{}", name.value);
        std::string endfun_label = fmt::format("__endfunction_{}", name.value);

        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, endfun_label);
        add_label(fun_label);
        read_statement();
        add_line<opcode::RET>();
        add_label(endfun_label);
        break;
    }
    case hash("call"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok = m_lexer.require(TOK_IDENTIFIER);
        add_line<opcode::UNEVAL_JUMP>(opcode::JSR, fmt::format("__function_{}", tok.value));
        m_lexer.require(TOK_PAREN_END);
        break;
    }
    case hash("foreach"): {
        std::string lines_label = make_label("lines");
        std::string endlines_label = make_label("endlines");
        m_loop_labels.push(loop_label_pair{lines_label, endlines_label});
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<opcode::MOVCONTENT>();
            pushed_content = true;
        }
        add_line<opcode::SUBVIEW>();
        add_label(lines_label);
        add_line<opcode::NEXTRESULT>();
        add_line<opcode::UNEVAL_JUMP>(opcode::JTE, endlines_label);
        read_statement();
        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, lines_label);
        add_label(endlines_label);
        if (pushed_content) {
            add_line<opcode::POPCONTENT>();
        } else {
            add_line<opcode::RESETVIEW>();
        }
        m_loop_labels.pop();
        break;
    }
    case hash("with"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::MOVCONTENT>();
        read_statement();
        add_line<opcode::POPCONTENT>();
        break;
    }
    case hash("between"): {
        add_line<opcode::NEWVIEW>();
        m_lexer.require(TOK_PAREN_BEGIN);
        add_line<opcode::PUSHVIEW>();
        read_expression();
        add_line<opcode::CALL>("indexof", small_int(2));
        add_line<opcode::SETBEGIN>();
        m_lexer.require(TOK_COMMA);
        add_line<opcode::PUSHVIEW>();
        read_expression();
        add_line<opcode::CALL>("indexof", small_int(2));
        add_line<opcode::SETEND>();
        m_lexer.require(TOK_PAREN_END);
        read_statement();
        add_line<opcode::RESETVIEW>();
        break;
    }
    case hash("step"): {
        bool pushed_content = false;
        if (m_lexer.check_next(TOK_PAREN_BEGIN)) {
            read_expression();
            m_lexer.require(TOK_PAREN_END);
            add_line<opcode::MOVCONTENT>();
            pushed_content = true;
        }

        add_line<opcode::SUBVIEW>();
        
        m_lexer.require(TOK_BRACE_BEGIN);
        while (!m_lexer.check_next(TOK_BRACE_END)) {
            add_line<opcode::NEXTRESULT>();
            read_statement();
        }

        if (pushed_content) {
            add_line<opcode::POPCONTENT>();
        } else {
            add_line<opcode::RESETVIEW>();
        }
        break;
    }
    case hash("setbegin"):
    case hash("setend"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        m_code.emplace_back(fun_name == "setbegin" ? opcode::SETBEGIN : opcode::SETEND);
        break;
    case hash("newview"):
        add_line<opcode::NEWVIEW>();
        read_statement();
        add_line<opcode::RESETVIEW>();
        break;
    case hash("clear"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        bool isglobal = m_lexer.check_next(TOK_GLOBAL);
        auto tok_var = m_lexer.require(TOK_IDENTIFIER);
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::SELVAR>(std::string(tok_var.value), small_int(0), small_int(0), uint8_t(SEL_GLOBAL & (-isglobal)));
        add_line<opcode::CLEAR>();
        break;
    }
    case hash("nexttable"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::NEXTTABLE>();
        break;
    case hash("error"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::THROWERR>();
        break;
    case hash("warning"):
        m_lexer.require(TOK_PAREN_BEGIN);
        read_expression();
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::ADDWARNING>();
        break;
    case hash("skip"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        break;
    case hash("return"):
        m_lexer.require(TOK_PAREN_BEGIN);
        m_lexer.require(TOK_PAREN_END);
        add_line<opcode::RET>();
        break;
    case hash("import"):
    case hash("setlayout"): {
        m_lexer.require(TOK_PAREN_BEGIN);
        auto tok_layout_name = m_lexer.require(TOK_STRING);
        m_lexer.require(TOK_PAREN_END);
        auto imported_file = std::filesystem::canonical(m_layout->m_filename.parent_path() / (tok_layout_name.parse_string() + ".bls"));
        uint8_t flags = 0;
        if (m_flags & PARSER_RECURSIVE_IMPORTS) {
            flags |= IMPORT_IGNORE;
        }
        if (fun_name == "setlayout") {
            flags |= IMPORT_SETLAYOUT;
        }
        add_line<opcode::IMPORT>(imported_file, flags);
        if (m_flags & PARSER_RECURSIVE_IMPORTS) {
            parser imported;
            imported.m_flags = m_flags;
            imported.read_layout(bill_layout_script::from_file(imported_file));
            auto code_len = m_code.size();
            std::copy(std::move_iterator(imported.m_code.begin()), std::move_iterator(imported.m_code.end()), std::back_inserter(m_code));
            for (auto &[line, comment] : imported.m_comments) {
                m_comments.emplace(code_len + line, std::move(comment));
            }
            for (auto &[label, line] : imported.m_labels) {
                m_labels.emplace(label, code_len + line);
            }
        }
        if (flags & IMPORT_SETLAYOUT) {
            add_line<opcode::HLT>();
        } else if (intl::valid_language(m_layout->language_code)) {
            add_line<opcode::SETLANG>(m_layout->language_code);
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
        add_line<opcode::UNEVAL_JUMP>(opcode::JMP, fun_name == "break" ? m_loop_labels.top().break_label : m_loop_labels.top().continue_label);
        break;
    default:
        throw parsing_error("Parola chiave sconosciuta", tok_name);
    }
}