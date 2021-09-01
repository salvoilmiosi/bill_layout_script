#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <functional>

#include "enums.h"
#include "utils.h"
#include "bytecode.h"

namespace bls {

    enum class token_kind {
        standard,
        keyword,
        symbol
    };

    struct token_info {
        token_kind kind = token_kind::standard;
        std::string_view value;
        std::string_view op_fun_name;
        int op_precedence = 0;
    };

    DEFINE_ENUM_DATA_IN_NS(bls, token_type, token_info,
        (INVALID)
        (END_OF_FILE)
        (IDENTIFIER)
        (STRING)
        (REGEXP)
        (NUMBER)
        (INTEGER)

        (KW_IF,         token_kind::keyword, "if")
        (KW_ELSE,       token_kind::keyword, "else")
        (KW_WHILE,      token_kind::keyword, "while")
        (KW_FOR,        token_kind::keyword, "for")
        (KW_GOTO,       token_kind::keyword, "goto")
        (KW_FUNCTION,   token_kind::keyword, "function")
        (KW_FOREACH,    token_kind::keyword, "foreach")
        (KW_WITH,       token_kind::keyword, "with")
        (KW_IMPORT,     token_kind::keyword, "import")
        (KW_BREAK,      token_kind::keyword, "break")
        (KW_CONTINUE,   token_kind::keyword, "continue")
        (KW_RETURN,     token_kind::keyword, "return")
        (KW_CLEAR,      token_kind::keyword, "clear")
        (KW_GLOBAL,     token_kind::keyword, "global")
        (KW_TIE,        token_kind::keyword, "tie")
        (KW_TRUE,       token_kind::keyword, "true")
        (KW_FALSE,      token_kind::keyword, "false")
        (KW_NULL,       token_kind::keyword, "null")

        (DOLLAR,        token_kind::symbol, "$")
        (SEMICOLON,     token_kind::symbol, ";")
        (PAREN_BEGIN,   token_kind::symbol, "(")
        (PAREN_END,     token_kind::symbol, ")")
        (COMMA,         token_kind::symbol, ",")
        (BRACKET_BEGIN, token_kind::symbol, "[")
        (BRACKET_END,   token_kind::symbol, "]")
        (BRACE_BEGIN,   token_kind::symbol, "{")
        (BRACE_END,     token_kind::symbol, "}")
        (ASSIGN,        token_kind::symbol, "=")
        (FORCE_ASSIGN,  token_kind::symbol, ":=")
        (ADD_ASSIGN,    token_kind::symbol, "+=")
        (SUB_ASSIGN,    token_kind::symbol, "-=")
        (ADD_ONE,       token_kind::symbol, "++")
        (SUB_ONE,       token_kind::symbol, "--")
        (CONTENT,       token_kind::symbol, "@")
        (COLON,         token_kind::symbol, ":")
        (NOT,           token_kind::symbol, "!")
        (ASTERISK,      token_kind::symbol, "*",    "mul", 6)
        (SLASH,         token_kind::symbol, "/",    "div", 6)
        (PLUS,          token_kind::symbol, "+",    "add", 5)
        (MINUS,         token_kind::symbol, "-",    "sub", 5)
        (LESS,          token_kind::symbol, "<",    "lt",  4)
        (LESS_EQ,       token_kind::symbol, "<=",   "leq", 4)
        (GREATER,       token_kind::symbol, ">",    "gt",  4)
        (GREATER_EQ,    token_kind::symbol, ">=",   "geq", 4)
        (EQUALS,        token_kind::symbol, "==",   "eq",  3)
        (NOT_EQUALS,    token_kind::symbol, "!=",   "neq", 3)
        (AND,           token_kind::symbol, "&&",   "and", 2)
        (OR,            token_kind::symbol, "||",   "or",  1)
    )

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