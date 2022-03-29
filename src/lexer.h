#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>

#include "bytecode.h"

namespace bls {

    struct keyword_kind {
        std::string_view value;
    };

    struct symbol_kind {
        std::string_view value;
    };

    struct operator_kind {
        std::string_view fun_name;
        int precedence;
    };

    struct keyword_op_kind : keyword_kind, operator_kind {};
    struct symbol_op_kind : symbol_kind, operator_kind {};

    DEFINE_ENUM_DATA(token_type,
        (INVALID)
        (END_OF_FILE)
        (IDENTIFIER)
        (STRING)
        (REGEXP)
        (NUMBER)
        (INTEGER)

        (KW_IF,         keyword_kind    { "if" })
        (KW_ELSE,       keyword_kind    { "else" })
        (KW_WHILE,      keyword_kind    { "while" })
        (KW_FOR,        keyword_kind    { "for" })
        (KW_GOTO,       keyword_kind    { "goto" })
        (KW_FUNCTION,   keyword_kind    { "function" })
        (KW_FOREACH,    keyword_kind    { "foreach" })
        (KW_WITH,       keyword_kind    { "with" })
        (KW_IMPORT,     keyword_kind    { "import" })
        (KW_BREAK,      keyword_kind    { "break" })
        (KW_CONTINUE,   keyword_kind    { "continue" })
        (KW_RETURN,     keyword_kind    { "return" })
        (KW_CLEAR,      keyword_kind    { "clear" })
        (KW_GLOBAL,     keyword_kind    { "global" })
        (KW_TIE,        keyword_kind    { "tie" })
        (KW_TRUE,       keyword_kind    { "true" })
        (KW_FALSE,      keyword_kind    { "false" })
        (KW_NULL,       keyword_kind    { "null" })

        (DOLLAR,        symbol_kind     { "$" })
        (SEMICOLON,     symbol_kind     { ";" })
        (PAREN_BEGIN,   symbol_kind     { "(" })
        (PAREN_END,     symbol_kind     { ")" })
        (COMMA,         symbol_kind     { "," })
        (BRACKET_BEGIN, symbol_kind     { "[" })
        (BRACKET_END,   symbol_kind     { "]" })
        (BRACE_BEGIN,   symbol_kind     { "{" })
        (BRACE_END,     symbol_kind     { "}" })
        (ASSIGN,        symbol_kind     { "=" })
        (FORCE_ASSIGN,  symbol_kind     { ":=" })
        (ADD_ASSIGN,    symbol_kind     { "+=" })
        (SUB_ASSIGN,    symbol_kind     { "-=" })
        (ADD_ONE,       symbol_kind     { "++" })
        (SUB_ONE,       symbol_kind     { "--" })
        (CONTENT,       symbol_kind     { "@" })
        (QUESTION_MARK, symbol_kind     { "?" })
        (COLON,         symbol_kind     { ":" })
        (NOT,           symbol_kind     { "!" })
        
        (ASTERISK,      symbol_op_kind  { "*",  "mul", 6 })
        (SLASH,         symbol_op_kind  { "/",  "div", 6 })
        (PLUS,          symbol_op_kind  { "+",  "add", 5 })
        (MINUS,         symbol_op_kind  { "-",  "sub", 5 })
        (LESS,          symbol_op_kind  { "<",  "lt",  4 })
        (LESS_EQ,       symbol_op_kind  { "<=", "leq", 4 })
        (GREATER,       symbol_op_kind  { ">",  "gt",  4 })
        (GREATER_EQ,    symbol_op_kind  { ">=", "geq", 4 })
        (EQUALS,        symbol_op_kind  { "==", "eq",  3 })
        (NOT_EQUALS,    symbol_op_kind  { "!=", "neq", 3 })
        (AND,           symbol_op_kind  { "&&", "and", 2 })
        (OR,            symbol_op_kind  { "||", "or",  1 })
    )
    
    template<token_type E, typename T> struct is_token_of_kind : std::false_type {};
    template<token_type E, typename T> requires enums::value_with_data<E>
    struct is_token_of_kind<E, T> : std::is_convertible<enums::enum_data_t<E>, T> {};

    template<token_type E> using is_keyword = is_token_of_kind<E, keyword_kind>;
    template<token_type E> using is_symbol = is_token_of_kind<E, symbol_kind>;
    template<token_type E> using is_operator = is_token_of_kind<E, operator_kind>;

    struct token {
        token_type type;
        std::string_view value;

        operator bool () {
            return type != token_type::INVALID;
        }

        std::string parse_string();
    };

    struct token_error : public std::runtime_error {
        const token location;

        template<typename T>
        token_error(T &&message, token location) :
            std::runtime_error(std::forward<T>(message)), location(location) {}
    };

    struct unexpected_token : public token_error {
        token_type expected;

        unexpected_token(token tok, token_type expected = token_type::INVALID)
            : token_error(expected == token_type::INVALID
                ? intl::translate("UNEXPECTED_TOKEN", enums::to_string(tok.type))
                : intl::translate("UNEXPECTED_TOKEN_REQUIRED", enums::to_string(tok.type), enums::to_string(expected)), tok),
            expected(expected) {}
    };

    class lexer {
    public:
        void set_script(std::string_view str);

        void set_code_ptr(class parser_code *code) {
            m_code = code;
        }

        token next(bool do_advance = true);
        token peek() {
            return next(false);
        }
        
        token require(token_type type);

        token check_next(token_type type);

        void advance(token tok);

        std::string token_location_info(const token &tok) const;

    private:
        class parser_code *m_code = nullptr;
        util::simple_stack<std::string> comment_lines;

        const char *m_begin;
        const char *m_current;
        const char *m_end;

        const char *m_line_end;
        int m_line_count;

        char nextChar();
        void skipSpaces();
        
        void onLineStart();

        bool readIdentifier();
        bool readString();
        bool readRegexp();
        bool readNumber();
        bool readLineComment();
        bool readBlockComment();
    };

}

#endif