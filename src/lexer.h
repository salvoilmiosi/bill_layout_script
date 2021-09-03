#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <functional>

#include "enums.h"
#include "utils.h"
#include "bytecode.h"

namespace bls {

    struct keyword {
        std::string_view value;
    };

    struct symbol {
        std::string_view value;
    };

    struct operator_symbol : symbol {
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

        (KW_IF,         keyword{ "if" })
        (KW_ELSE,       keyword{ "else" })
        (KW_WHILE,      keyword{ "while" })
        (KW_FOR,        keyword{ "for" })
        (KW_GOTO,       keyword{ "goto" })
        (KW_FUNCTION,   keyword{ "function" })
        (KW_FOREACH,    keyword{ "foreach" })
        (KW_WITH,       keyword{ "with" })
        (KW_IMPORT,     keyword{ "import" })
        (KW_BREAK,      keyword{ "break" })
        (KW_CONTINUE,   keyword{ "continue" })
        (KW_RETURN,     keyword{ "return" })
        (KW_CLEAR,      keyword{ "clear" })
        (KW_GLOBAL,     keyword{ "global" })
        (KW_TIE,        keyword{ "tie" })
        (KW_TRUE,       keyword{ "true" })
        (KW_FALSE,      keyword{ "false" })
        (KW_NULL,       keyword{ "null" })

        (DOLLAR,        symbol{ "$" })
        (SEMICOLON,     symbol{ ";" })
        (PAREN_BEGIN,   symbol{ "(" })
        (PAREN_END,     symbol{ ")" })
        (COMMA,         symbol{ "," })
        (BRACKET_BEGIN, symbol{ "[" })
        (BRACKET_END,   symbol{ "]" })
        (BRACE_BEGIN,   symbol{ "{" })
        (BRACE_END,     symbol{ "}" })
        (ASSIGN,        symbol{ "=" })
        (FORCE_ASSIGN,  symbol{ ":=" })
        (ADD_ASSIGN,    symbol{ "+=" })
        (SUB_ASSIGN,    symbol{ "-=" })
        (ADD_ONE,       symbol{ "++" })
        (SUB_ONE,       symbol{ "--" })
        (CONTENT,       symbol{ "@" })
        (COLON,         symbol{ ":" })
        (NOT,           symbol{ "!" })
        
        (ASTERISK,      operator_symbol{ "*",    "mul", 6 })
        (SLASH,         operator_symbol{ "/",    "div", 6 })
        (PLUS,          operator_symbol{ "+",    "add", 5 })
        (MINUS,         operator_symbol{ "-",    "sub", 5 })
        (LESS,          operator_symbol{ "<",    "lt",  4 })
        (LESS_EQ,       operator_symbol{ "<=",   "leq", 4 })
        (GREATER,       operator_symbol{ ">",    "gt",  4 })
        (GREATER_EQ,    operator_symbol{ ">=",   "geq", 4 })
        (EQUALS,        operator_symbol{ "==",   "eq",  3 })
        (NOT_EQUALS,    operator_symbol{ "!=",   "neq", 3 })
        (AND,           operator_symbol{ "&&",   "and", 2 })
        (OR,            operator_symbol{ "||",   "or",  1 })
    )
    
    template<token_type E, typename T> struct is_token_of_kind : std::false_type {};
    template<token_type E, typename T> requires enums::has_data<E>
    struct is_token_of_kind<E, T> : std::is_convertible<enums::enum_data_t<E>, T> {};

    template<token_type E> using is_keyword = is_token_of_kind<E, keyword>;
    template<token_type E> using is_symbol = is_token_of_kind<E, symbol>;
    template<token_type E> using is_operator = is_token_of_kind<E, operator_symbol>;

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
    };

}

#endif