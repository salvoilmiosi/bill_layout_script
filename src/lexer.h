#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <functional>

#include "enums.h"
#include "utils.h"
#include "bytecode.h"

namespace bls {

    struct keyword_kind {
        std::string_view value;
    };

    struct symbol_kind {
        std::string_view value;
    };

    struct operator_kind : symbol_kind {
        std::string_view fun_name;
        int precedence;
    };

    DEFINE_ENUM_DATA_IN_NS(bls, token_type,
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
        (COLON,         symbol_kind     { ":" })
        (NOT,           symbol_kind     { "!" })
        
        (ASTERISK,      operator_kind   { "*",  "mul", 6 })
        (SLASH,         operator_kind   { "/",  "div", 6 })
        (PLUS,          operator_kind   { "+",  "add", 5 })
        (MINUS,         operator_kind   { "-",  "sub", 5 })
        (LESS,          operator_kind   { "<",  "lt",  4 })
        (LESS_EQ,       operator_kind   { "<=", "leq", 4 })
        (GREATER,       operator_kind   { ">",  "gt",  4 })
        (GREATER_EQ,    operator_kind   { ">=", "geq", 4 })
        (EQUALS,        operator_kind   { "==", "eq",  3 })
        (NOT_EQUALS,    operator_kind   { "!=", "neq", 3 })
        (AND,           operator_kind   { "&&", "and", 2 })
        (OR,            operator_kind   { "||", "or",  1 })
    )
    
    template<token_type E, typename T> struct is_token_of_kind : std::false_type {};
    template<token_type E, typename T> requires enums::has_data<E>
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

        template<typename T>
        void set_comment_callback(T &&fun) {
            comment_callback = std::forward<T>(fun);
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
        std::function<void(std::string)> comment_callback;
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