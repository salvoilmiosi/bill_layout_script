#include "parser.h"

#include <iostream>
#include <tuple>

#include "utils.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "functions.h"

using namespace bls;

void parser::read_layout(const std::filesystem::path &path, const layout_box_list &layout) {
    m_path = path;
    m_layout = &layout;
    
    try {
        m_code.add_line<opcode::ADDLAYOUT>(std::filesystem::canonical(path).string());
        if (layout.setlayout) {
            m_code.add_line<opcode::SETLAYOUT>();
        }
        if (layout.language.empty()) {
            m_code.add_line<opcode::SETLANG>();
        } else {
            m_code.add_line<opcode::SETLANG>(layout.language + ".UTF-8");
        }
        for (auto &box : layout) {
            read_box(box);
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("{}: {}\n{}",
            current_box->name, error.what(),
            m_lexer.token_location_info(error.location())));
    }

    for (auto it = m_code.begin(); it != m_code.end(); ++it) {
        it->visit([&](auto &addr) {
            if constexpr (std::is_base_of_v<jump_address, std::remove_reference_t<decltype(addr)>>) {
                if (!addr.address) {
                    if (auto label_it = m_code.find_label(addr.label); label_it != m_code.end()) {
                        addr.address = label_it - it;
                    } else {
                        throw layout_error(fmt::format("Etichetta sconosciuta: {}", addr.label));
                    }
                }
            }
        });
    }
}

void parser::read_box(const layout_box &box) {
    if (box.flags.check(box_flags::DISABLED)) {
        return;
    }
    
    if (m_flags.check(parser_flags::ADD_COMMENTS) && !box.name.empty()) {
        m_code.add_line<opcode::COMMENT>("### " + box.name);
    }
    current_box = &box;

    if (m_flags.check(parser_flags::ADD_COMMENTS)) {
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
    m_content_level = 0;

    if (m_flags.check(parser_flags::ADD_COMMENTS)) {
        m_lexer.set_comment_callback([this](const std::string &line){
            m_code.add_line<opcode::COMMENT>(line);
        });
    }
    m_lexer.set_script(box.spacers);

    while(true) {
        auto tok = m_lexer.next();
        if (tok.type == token_type::IDENTIFIER) {
            if (auto idx = enums::find_enum_index<spacer_index>(tok.value); idx != enums::invalid_enum_value<spacer_index>) {
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
                    m_code.add_line<opcode::MVNBOX>(idx);
                } else {
                    m_code.add_line<opcode::MVBOX>(idx);
                }
            } else {
                throw parsing_error(fmt::format("Flag spacer non valido: {}", tok.value), tok);
            }
        } else if (tok.type != token_type::END_OF_FILE) {
            throw unexpected_token(tok, token_type::IDENTIFIER);
        } else {
            break;
        }
    }

    if (!box.flags.check(box_flags::NOREAD) && !box.flags.check(box_flags::SPACER)) {
        ++m_content_level;
        m_code.add_line<opcode::RDBOX>(box.mode, box.flags);
    }

    m_lexer.set_script(box.script);
    while (!m_lexer.check_next(token_type::END_OF_FILE)) {
        read_statement();
    }

    if (!box.flags.check(box_flags::NOREAD) && !box.flags.check(box_flags::SPACER)) {
        m_code.add_line<opcode::CNTPOP>();
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
    case token_type::KW_STEP:       parse_step_stmt(); break;
    case token_type::KW_IMPORT:     parse_import_stmt(); break;
    case token_type::KW_BREAK:      parse_break_stmt(); break;
    case token_type::KW_CONTINUE:   parse_continue_stmt(); break;
    case token_type::KW_RETURN:     parse_return_stmt(); break;
    case token_type::KW_CLEAR:      parse_clear_stmt(); break;
    case token_type::KW_SET:        parse_set_stmt(); break;
    default:
        assignment_stmt();
        m_lexer.require(token_type::SEMICOLON);
    }
}

void parser::assignment_stmt() {
    variable_prefixes prefixes;

    auto tok = m_lexer.peek();
    switch(tok.type) {
    case token_type::IDENTIFIER:
        m_lexer.advance(tok);
        if (m_lexer.peek().type == token_type::PAREN_BEGIN) {
            read_function(tok, true);
            return;
        } else {
            m_code.add_line<opcode::SELVAR>(tok.value);
            read_variable_indices(false);
        }
        break;
    default:
        prefixes = read_variable(false);
    }

    command_args assign_cmd;

    tok = m_lexer.next();
    switch (tok.type) {
    case token_type::ADD_ASSIGN:
        assign_cmd = make_command<opcode::INCVAR>();
        read_expression();
        break;
    case token_type::SUB_ASSIGN:
        assign_cmd = make_command<opcode::DECVAR>();
        read_expression();
        break;
    case token_type::ADD_ONE:
        assign_cmd = make_command<opcode::INCVAR>();
        m_code.add_line<opcode::PUSHINT>(1);
        break;
    case token_type::SUB_ONE:
        assign_cmd = make_command<opcode::DECVAR>();
        m_code.add_line<opcode::PUSHINT>(1);
        break;
    case token_type::ASSIGN:
        assign_cmd = make_command<opcode::SETVAR>();
        read_expression();
        break;
    default:
        throw unexpected_token(tok, token_type::ASSIGN);
    }

    if (prefixes.call.command() != opcode::NOP) {
        m_code.push_back(prefixes.call);
    }

    m_code.push_back(assign_cmd);
}

void parser::read_expression() {
    struct operator_precedence {
        command_args command;
        int precedence;

        operator_precedence(std::string_view fun_name, int precedence)
            : command(make_command<opcode::CALL>(fun_name, 2))
            , precedence(precedence) {}
    };
    static const std::map<token_type, operator_precedence> operators = {
        {token_type::ASTERISK,      {"mul", 6}},
        {token_type::SLASH,         {"div", 6}},
        {token_type::PLUS,          {"add", 5}},
        {token_type::MINUS,         {"sub", 5}},
        {token_type::LESS,          {"lt",  4}},
        {token_type::LESS_EQ,       {"leq", 4}},
        {token_type::GREATER,       {"gt",  4}},
        {token_type::GREATER_EQ,    {"geq", 4}},
        {token_type::EQUALS,        {"eq",  3}},
        {token_type::NOT_EQUALS,    {"neq", 3}},
        {token_type::AND,           {"and", 2}},
        {token_type::OR,            {"or",  1}},
    };

    sub_expression();

    simple_stack<decltype(operators)::const_iterator> op_stack;
    
    while (true) {
        auto tok_op = m_lexer.peek();
        auto it = operators.find(tok_op.type);
        if (it == std::end(operators)) break;
        
        m_lexer.advance(tok_op);
        if (!op_stack.empty() && op_stack.back()->second.precedence >= it->second.precedence) {
            m_code.push_back(op_stack.back()->second.command);
            op_stack.pop_back();
        }
        op_stack.push_back(it);
        sub_expression();
    }

    while (!op_stack.empty()) {
        m_code.push_back(op_stack.back()->second.command);
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
            m_code.add_line<opcode::PUSHINT>(util::string_to<big_int>(tok_num.value));
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
            m_code.add_line<opcode::PUSHINT>(-util::string_to<big_int>(tok_num.value));
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
        m_code.add_line<opcode::PUSHINT>(util::string_to<big_int>(tok_first.value));
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
        if (m_content_level == 0) {
            throw parsing_error("Stack contenuti vuoto", tok_first);
        }
        m_code.add_line<opcode::PUSHVIEW>();
        break;
    case token_type::IDENTIFIER:
        m_lexer.advance(tok_first);
        if (m_lexer.peek().type == token_type::PAREN_BEGIN) {
            read_function(tok_first, false);
        } else if (auto it = std::ranges::find(m_fun_args, tok_first.value); it != m_fun_args.end()) {
            m_code.add_line<opcode::PUSHARG>(it - m_fun_args.begin());
        } else {
            m_code.add_line<opcode::SELVAR>(tok_first.value);
            read_variable_indices(true);
            m_code.add_line<opcode::PUSHVAR>();
        }
        break;
    default:
        read_variable(true);
        m_code.add_line<opcode::PUSHVAR>();
    }
}

static parsing_error read_only_error(const token &tok) {
    return parsing_error("Contesto di sola lettura", tok);
};

variable_prefixes parser::read_variable(bool read_only) {
    auto prefixes = read_variable_name(read_only);
    read_variable_indices(read_only);
    return prefixes;
}

variable_prefixes parser::read_variable_name(bool read_only) {
    variable_prefixes prefixes;
    bool isglobal = false;

    token current_token;

    auto set_global = [&] {
        if (isglobal) throw parsing_error("Prefisso global duplicato", current_token);
        isglobal = true;
    };

    auto set_function_call = [&](std::string_view fun_name) {
        if (read_only) throw read_only_error(current_token);
        if (prefixes.call.command() == opcode::NOP) {
            prefixes.call = make_command<opcode::CALL>(fun_name, 1);
        } else {
            throw parsing_error("Ammesso solo un prefisso di funzione", current_token);
        }
    };

    while (true) {
        current_token = m_lexer.next();
        switch (current_token.type) {
        case token_type::KW_GLOBAL:     set_global(); break;
        case token_type::PERCENT:       set_function_call("num"); break;
        case token_type::CARET:         set_function_call("aggregate"); break;
        case token_type::SINGLE_QUOTE:  set_function_call("totitle"); break;
        case token_type::BRACKET_BEGIN:
            read_expression();
            m_lexer.require(token_type::BRACKET_END);
            if (isglobal) {
                m_code.add_line<opcode::SELGLOBALDYN>();
            } else {
                m_code.add_line<opcode::SELVARDYN>();
            }
            return prefixes;
        case token_type::IDENTIFIER:
            if (isglobal) {
                m_code.add_line<opcode::SELGLOBAL>(current_token.value);
            } else {
                m_code.add_line<opcode::SELVAR>(current_token.value);
            }
            return prefixes;
        default:
            throw unexpected_token(current_token, token_type::IDENTIFIER);
        }
    }
}

void parser::read_variable_indices(bool read_only) {
    bool in_loop = true;
    while (in_loop && m_lexer.check_next(token_type::BRACKET_BEGIN)) { // variable[
        token tok = m_lexer.peek();
        switch (tok.type) {
        case token_type::COLON: {
            if (read_only) throw read_only_error(tok);
            m_lexer.advance(tok);
            tok = m_lexer.peek();
            switch (tok.type) {
            case token_type::BRACKET_END:
                m_code.add_line<opcode::SELEACH>(); // variable[:]
                in_loop = false;
                break;
            case token_type::INTEGER: // variable[:N] -- append N times
                m_lexer.advance(tok);
                m_code.add_line<opcode::SELAPPEND>();
                m_code.add_line<opcode::SELSIZE>(util::string_to<int>(tok.value));
                in_loop = false;
                break;
            default:
                m_code.add_line<opcode::SELAPPEND>();
                read_expression();
                m_code.add_line<opcode::SELSIZEDYN>();
            }
            break;
        }
        case token_type::BRACKET_END:
            if (read_only) throw read_only_error(tok);
            m_code.add_line<opcode::SELAPPEND>();
            break;
        default:
            if (tok = m_lexer.check_next(token_type::INTEGER)) { // variable[N]
                m_code.add_line<opcode::SELINDEX>(util::string_to<int>(tok.value));
            } else {
                read_expression();
                m_code.add_line<opcode::SELINDEXDYN>();
            }
            if (tok = m_lexer.check_next(token_type::COLON)) { // variable[N:M] -- M times after index N
                if (read_only) throw read_only_error(tok);
                if (tok = m_lexer.check_next(token_type::INTEGER)) {
                    m_code.add_line<opcode::SELSIZE>(util::string_to<int>(tok.value));
                } else {
                    read_expression();
                    m_code.add_line<opcode::SELSIZEDYN>();
                }
                in_loop = false;
                break;
            }
        }
        m_lexer.require(token_type::BRACKET_END);
    }
}

void parser::read_function(token tok_fun_name, bool top_level) {
    assert(tok_fun_name.type == token_type::IDENTIFIER);
    m_lexer.require(token_type::PAREN_BEGIN);

    auto fun_name = tok_fun_name.value;
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
    
    if (auto it = function_lookup::find(fun_name); function_lookup::valid(it)) {
        const auto &fun = it->second;
        if (num_args < fun.minargs || num_args > fun.maxargs) {
            throw invalid_numargs(std::string(fun_name), fun.minargs, fun.maxargs, tok_fun_name);
        }
        if (fun.returns_value) {
            if (top_level) throw parsing_error(fmt::format("Impossibile chiamare {} dal top level", fun_name), tok_fun_name);
            m_code.add_line<opcode::CALL>(it, num_args);
        } else {
            if (!top_level) throw parsing_error(fmt::format("Impossibile chiamare {} fuori dal top level", fun_name), tok_fun_name);
            m_code.add_line<opcode::SYSCALL>(it, num_args);
        }
    } else if (auto it = m_functions.find(fun_name); it != m_functions.end()) {
        const auto &fun = it->second;
        if (fun.numargs != num_args) {
            throw invalid_numargs(std::string(fun_name), fun.numargs, fun.numargs, tok_fun_name);
        }
        if (fun.has_contents && m_content_level == 0) {
            throw parsing_error(fmt::format("Impossibile chiamare {}, stack contenuti vuoto", fun_name), tok_fun_name);
        }
        jump_label label = fmt::format("__function_{}", fun_name);
        if (top_level) {
            m_code.add_line<opcode::JSR>(label, num_args);
        } else {
            m_code.add_line<opcode::JSRVAL>(label, num_args);
        }
    } else {
        throw parsing_error(fmt::format("Funzione sconosciuta: {}", fun_name), tok_fun_name);
    }
}