#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

using namespace bls;

static parsing_error make_parsing_error(const layout_box &box, const token_error &error, const lexer &lexer) {
    return parsing_error(std::format("{0}: {1}\n{2}",
        box.name, lexer.token_location_info(error.location), error.what()));
}

command_list parser::read_layout(const std::filesystem::path &path, const layout_box_list &layout) {
    m_path = std::filesystem::canonical(path);
    m_code.add_line<opcode::SETPATH>(m_path.string());

    if (layout.find_layout_flag) {
        m_code.add_line<opcode::FOUNDLAYOUT>();
    }
    
    if (!layout.language.empty()) {
        m_lang = layout.language + ".UTF-8";
    }
    m_code.add_line<opcode::SETLANG>(m_lang);

    for (const layout_box &box : layout) {
        try {
            if (auto tok = read_goto_label(box)) {
                if (m_goto_labels.contains(tok.value)) {
                    throw token_error(intl::translate("DUPLICATE_GOTO_LABEL", tok.value), tok);
                } else {
                    m_goto_labels.emplace(std::string(tok.value), m_code.make_label());
                }
            }
        } catch (const token_error &error) {
            throw make_parsing_error(box, error, m_lexer);
        }
    }

    for (const layout_box &box : layout) {
        try {
            read_box(box);
        } catch (const token_error &error) {
            throw make_parsing_error(box, error, m_lexer);
        }
    }
    
    m_code.add_line<opcode::RET>();

    return std::move(m_code);
}

token parser::read_goto_label(const layout_box &box) {
    m_lexer.set_comment_callback(nullptr);
    m_lexer.set_script(box.goto_label);
    auto tok_label = m_lexer.next();
    if (tok_label.type == token_type::IDENTIFIER) {
        m_lexer.require(token_type::END_OF_FILE);
        return tok_label;
    } else if (tok_label.type != token_type::END_OF_FILE) {
        throw unexpected_token(tok_label, token_type::IDENTIFIER);
    }
    return {};
}

static const auto spacer_idx_map = [] {
    util::string_map<spacer_index> ret;
    for (auto value : enums::enum_values_v<spacer_index>) {
        for (auto str : enums::get_data(value)) {
            ret.emplace(str, value);
        }
    }
    return ret;
}();

void parser::read_box(const layout_box &box) {
    if (box.flags.check(box_flags::DISABLED)) {
        return;
    }
    
    if (token tok = read_goto_label(box)) {
        m_code.add_label(m_goto_labels.at(std::string(tok.value)));
    }
    
    m_code.add_line<opcode::BOXNAME>(box.name);

    if (!box.flags.check(box_flags::NOREAD) || box.flags.check(box_flags::SPACER)) {
        m_code.add_line<opcode::NEWBOX>();
        if (!box.flags.check(box_flags::PAGE)) {
            m_code.add_line<opcode::PUSHDOUBLE>(box.x);
            m_code.add_line<opcode::MVBOX>(spacer_index::X);
            m_code.add_line<opcode::PUSHDOUBLE>(box.y);
            m_code.add_line<opcode::MVBOX>(spacer_index::Y);
            m_code.add_line<opcode::PUSHDOUBLE>(box.w);
            m_code.add_line<opcode::MVBOX>(spacer_index::WIDTH);
            m_code.add_line<opcode::PUSHDOUBLE>(box.h);
            m_code.add_line<opcode::MVBOX>(spacer_index::HEIGHT);
        }
        m_code.add_line<opcode::PUSHINT>(box.page);
        m_code.add_line<opcode::MVBOX>(spacer_index::PAGE);
    }

    m_lexer.set_comment_callback([this](std::string line){
        m_code.add_line<opcode::COMMENT>(std::move(line));
    });
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == token_type::IDENTIFIER) {
            auto it = spacer_idx_map.find(tok.value);
            if (it == spacer_idx_map.end()) {
                throw token_error(intl::translate("INVALID_SPACER_FLAG", tok.value), tok);
            }
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
            if (negative) {
                m_code.add_line<opcode::MVNBOX>(it->second);
            } else {
                m_code.add_line<opcode::MVBOX>(it->second);
            }
        } else if (tok.type != token_type::END_OF_FILE) {
            throw unexpected_token(tok, token_type::IDENTIFIER);
        } else {
            break;
        }
    }

    if (!box.flags.check(box_flags::NOREAD) && !box.flags.check(box_flags::SPACER)) {
        ++m_views_size;
        m_code.add_line<opcode::RDBOX>(box.mode, box.flags);
    }

    m_lexer.set_script(box.script);
    while (!m_lexer.check_next(token_type::END_OF_FILE)) {
        read_statement();
    }

    if (!box.flags.check(box_flags::NOREAD) && !box.flags.check(box_flags::SPACER)) {
        --m_views_size;
        m_code.add_line<opcode::VIEWPOP>();
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
    case token_type::END_OF_FILE:
        throw unexpected_token(tok_first);
    case token_type::SEMICOLON:
        m_lexer.advance(tok_first);
        break;
    case token_type::KW_IF:         parse_if_stmt(); break;
    case token_type::KW_WHILE:      parse_while_stmt(); break;
    case token_type::KW_FOR:        parse_for_stmt(); break;
    case token_type::KW_GOTO:       parse_goto_stmt(); break;
    case token_type::KW_FUNCTION:   parse_function_stmt(); break;
    case token_type::KW_FOREACH:    parse_foreach_stmt(); break;
    case token_type::KW_WITH:       parse_with_stmt(); break;
    case token_type::KW_IMPORT:     parse_import_stmt(); break;
    case token_type::KW_BREAK:      parse_break_stmt(); break;
    case token_type::KW_CONTINUE:   parse_continue_stmt(); break;
    case token_type::KW_RETURN:     parse_return_stmt(); break;
    case token_type::KW_CLEAR:      parse_clear_stmt(); break;
    case token_type::KW_TIE:        parse_tie_stmt(); break;
    default:
        assignment_stmt();
        m_lexer.require(token_type::SEMICOLON);
    }
}

void parser::assignment_stmt() {
    auto tok = m_lexer.peek();
    switch(tok.type) {
    case token_type::IDENTIFIER:
        m_lexer.advance(tok);
        if (m_lexer.peek().type == token_type::PAREN_BEGIN) {
            read_function(tok, true);
            return;
        } else {
            m_code.add_line<opcode::SELVAR>(tok.value);
            read_variable_indices();
        }
        break;
    default:
        read_variable_name();
        read_variable_indices();
    }

    tok = m_lexer.next();
    switch (tok.type) {
    case token_type::ADD_ASSIGN:
        read_expression();
        m_code.add_line<opcode::INCVAR>();
        break;
    case token_type::SUB_ASSIGN:
        read_expression();
        m_code.add_line<opcode::DECVAR>();
        break;
    case token_type::ADD_ONE:
        m_code.add_line<opcode::PUSHINT>(1);
        m_code.add_line<opcode::INCVAR>();
        break;
    case token_type::SUB_ONE:
        m_code.add_line<opcode::PUSHINT>(1);
        m_code.add_line<opcode::DECVAR>();
        break;
    case token_type::FORCE_ASSIGN:
        read_expression();
        m_code.add_line<opcode::FORCEVAR>();
        break;
    case token_type::ASSIGN:
        read_expression();
        m_code.add_line<opcode::SETVAR>();
        break;
    default:
        throw unexpected_token(tok, token_type::ASSIGN);
    }
}

void parser::read_expression() {
    if (m_lexer.check_next(token_type::KW_FOREACH)) {
        m_lexer.require(token_type::PAREN_BEGIN);
        read_expression();
        m_lexer.require(token_type::PAREN_END);

        auto label_loop_begin = m_code.make_label();
        auto label_loop_end = m_code.make_label();

        m_code.add_line<opcode::VIEWADDLIST>();
        m_code.add_line<opcode::PUSHNULL>();

        m_code.add_label(label_loop_begin);
        m_code.add_line<opcode::JVE>(label_loop_end);

        ++m_views_size;
        read_expression();
        --m_views_size;

        m_code.add_line<opcode::STKAPP>();
        m_code.add_line<opcode::VIEWNEXT>();
        m_code.add_line<opcode::JMP>(label_loop_begin);
        m_code.add_label(label_loop_end);

        m_code.add_line<opcode::STKSWP>();
        m_code.add_line<opcode::VIEWPOP>();
    } else {
        sub_expression();

        simple_stack<token_type> op_stack;
        
        while (true) {
            auto tok_op = m_lexer.peek();
            if (enums::get_data(tok_op.type).op_precedence == 0) break;
            
            m_lexer.advance(tok_op);
            if (!op_stack.empty() && enums::get_data(op_stack.back()).op_precedence >= enums::get_data(tok_op.type).op_precedence) {
                m_code.add_line<opcode::CALL>(enums::get_data(op_stack.back()).op_fun_name);
                op_stack.pop_back();
            }
            op_stack.push_back(tok_op.type);
            sub_expression();
        }

        while (!op_stack.empty()) {
            m_code.add_line<opcode::CALL>(enums::get_data(op_stack.back()).op_fun_name);
            op_stack.pop_back();
        }
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
    case token_type::NOT:
        m_lexer.advance(tok_first);
        sub_expression();
        m_code.add_line<opcode::CALL>("not");
        break;
    case token_type::PLUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(util::string_to<int64_t>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("num");
        }
        break;
    }
    case token_type::MINUS: {
        m_lexer.advance(tok_first);
        auto tok_num = m_lexer.peek();
        switch (tok_num.type) {
        case token_type::INTEGER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHINT>(-util::string_to<int64_t>(tok_num.value));
            break;
        case token_type::NUMBER:
            m_lexer.advance(tok_num);
            m_code.add_line<opcode::PUSHNUM>(-fixed_point(std::string(tok_num.value)));
            break;
        default:
            sub_expression();
            m_code.add_line<opcode::CALL>("neg");
        }
        break;
    }
    case token_type::KW_NULL:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHNULL>();
        break;
    case token_type::KW_TRUE:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHBOOL>(true);
        break;
    case token_type::KW_FALSE:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHBOOL>(false);
        break;
    case token_type::INTEGER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHINT>(util::string_to<int64_t>(tok_first.value));
        break;
    case token_type::NUMBER:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHNUM>(fixed_point(std::string(tok_first.value)));
        break;
    case token_type::SLASH: 
        m_code.add_line<opcode::PUSHREGEX>(m_lexer.require(token_type::REGEXP).parse_string());
        break;
    case token_type::STRING:
        m_lexer.advance(tok_first);
        m_code.add_line<opcode::PUSHSTR>(tok_first.parse_string());
        break;
    case token_type::CONTENT:
        m_lexer.advance(tok_first);
        if (m_views_size == 0) {
            throw token_error(intl::translate("EMPTY_VIEW_STACK"), tok_first);
        }
        m_code.add_line<opcode::PUSHVIEW>();
        break;
    case token_type::IDENTIFIER:
        m_lexer.advance(tok_first);
        if (m_lexer.peek().type == token_type::PAREN_BEGIN) {
            read_function(tok_first, false);
        } else {
            m_code.add_line<opcode::SELVAR>(tok_first.value);
            m_code.add_line<opcode::PUSHVAR>();
        }
        break;
    default:
        read_variable_name();
        m_code.add_line<opcode::PUSHVAR>();
    }
    
    while (m_lexer.check_next(token_type::BRACKET_BEGIN)) {
        if (token tok = m_lexer.check_next(token_type::INTEGER)) {
            m_code.add_line<opcode::SUBITEM>(util::string_to<size_t>(tok.value));
        } else {
            read_expression();
            m_code.add_line<opcode::SUBITEMDYN>();
        }
        m_lexer.require(token_type::BRACKET_END);
    }
}

void parser::read_variable_name() {
    enum class variable_type {
        VALUES,
        GLOBAL,
        LOCAL
    } var_type = variable_type::VALUES;

    if (m_lexer.check_next(token_type::KW_GLOBAL)) {
        var_type = variable_type::GLOBAL;
    } else if (auto tok = m_lexer.check_next(token_type::DOLLAR)) {
        var_type = variable_type::LOCAL;
    }

    if (m_lexer.check_next(token_type::BRACKET_BEGIN)) {
        read_expression();
        m_lexer.require(token_type::BRACKET_END);
        switch (var_type) {
        case variable_type::VALUES: m_code.add_line<opcode::SELVARDYN>(); break;
        case variable_type::GLOBAL: m_code.add_line<opcode::SELGLOBALDYN>(); break;
        case variable_type::LOCAL: m_code.add_line<opcode::SELLOCALDYN>(); break;
        }
    } else if (auto tok = m_lexer.require(token_type::IDENTIFIER)) {
        switch (var_type) {
        case variable_type::VALUES: m_code.add_line<opcode::SELVAR>(tok.value); break;
        case variable_type::GLOBAL: m_code.add_line<opcode::SELGLOBAL>(tok.value); break;
        case variable_type::LOCAL: m_code.add_line<opcode::SELLOCAL>(tok.value); break;
        }
    }
}

void parser::read_variable_indices() {
    while (m_lexer.check_next(token_type::BRACKET_BEGIN)) {
        if (m_lexer.check_next(token_type::BRACKET_END)) { // variable[]
            m_code.add_line<opcode::SELAPPEND>();
        } else if (auto tok = m_lexer.check_next(token_type::INTEGER)) { // variable[int]
            m_code.add_line<opcode::SELINDEX>(util::string_to<size_t>(tok.value));
            m_lexer.require(token_type::BRACKET_END);
        } else { // variable[expr]
            read_expression();
            m_code.add_line<opcode::SELINDEXDYN>();
            m_lexer.require(token_type::BRACKET_END);
        }
    }
}

void parser::read_function(token tok_fun_name, bool top_level) {
    assert(tok_fun_name.type == token_type::IDENTIFIER);
    m_lexer.require(token_type::PAREN_BEGIN);
    
    auto fun_name = tok_fun_name.value;
    size_t num_args = 0;

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
    
    if (auto it = function_lookup::find(fun_name); function_lookup::valid(it)) {
        const auto &fun = it->second;
        if (num_args < fun.minargs || num_args > fun.maxargs) {
            throw invalid_numargs(std::string(fun_name), fun.minargs, fun.maxargs, tok_fun_name);
        }
        if (fun.minargs != fun.maxargs) {
            m_code.add_line<opcode::CALLARGS>(num_args);
        }
        if (fun.returns_value) {
            if (top_level) throw token_error(intl::translate("CANT_CALL_FROM_TOP_LEVEL", fun_name), tok_fun_name);
            m_code.add_line<opcode::CALL>(it);
        } else {
            if (!top_level) throw token_error(intl::translate("CANT_CALL_OUT_OF_TOP_LEVEL", fun_name), tok_fun_name);
            m_code.add_line<opcode::SYSCALL>(it);
        }
    } else if (auto it = m_functions.find(fun_name); it != m_functions.end()) {
        const auto &fun = it->second;
        if (fun.numargs != num_args) {
            throw invalid_numargs(std::string(fun_name), fun.numargs, fun.numargs, tok_fun_name);
        }
        if (top_level) {
            m_code.add_line<opcode::JSR>(fun.node);
        } else {
            m_code.add_line<opcode::JSRVAL>(fun.node);
        }
    } else {
        throw token_error(intl::translate("UNKNOWN_FUNCTION", fun_name), tok_fun_name);
    }
}