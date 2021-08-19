#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <functional>

#include "enums.h"
#include "stack.h"
#include "utils.h"
#include "bytecode.h"

namespace bls {

    DEFINE_ENUM_DATA_IN_NS(bls, token_type, const char *,
        (INVALID,       "Token Invalido")
        (END_OF_FILE,   "Fine File")
        (IDENTIFIER,    "Identificatore")
        (KW_IF,         "if")
        (KW_ELSE,       "else")
        (KW_WHILE,      "while")
        (KW_FOR,        "for")
        (KW_GOTO,       "goto")
        (KW_FUNCTION,   "function")
        (KW_FOREACH,    "foreach")
        (KW_WITH,       "with")
        (KW_IMPORT,     "import")
        (KW_BREAK,      "break")
        (KW_CONTINUE,   "continue")
        (KW_RETURN,     "return")
        (KW_CLEAR,      "clear")
        (KW_GLOBAL,     "global")
        (STRING,        "Stringa")
        (REGEXP,        "Espressione Regolare")
        (NUMBER,        "Numero")
        (INTEGER,       "Numero Intero")
        (DOLLAR,        "$")
        (SEMICOLON,     ";")
        (PAREN_BEGIN,   "(")
        (PAREN_END,     ")")
        (COMMA,         ",")
        (BRACKET_BEGIN, "[")
        (BRACKET_END,   "]")
        (BRACE_BEGIN,   "{")
        (BRACE_END,     "}")
        (ASSIGN,        "=")
        (FORCE_ASSIGN,  ":=")
        (ADD_ASSIGN,    "+=")
        (SUB_ASSIGN,    "-=")
        (ADD_ONE,       "++")
        (SUB_ONE,       "--")
        (CONTENT,       "@")
        (COLON,         ":")
        (ASTERISK,      "*")
        (SLASH,         "/")
        (PLUS,          "+")
        (MINUS,         "-")
        (AND,           "&&")
        (OR,            "||")
        (NOT,           "!")
        (EQUALS,        "==")
        (NOT_EQUALS,    "!=")
        (GREATER,       ">")
        (LESS,          "<")
        (GREATER_EQ,    ">=")
        (LESS_EQ,       "<=")
    )

    static const util::string_map<token_type> keyword_tokens {
        {"if",          token_type::KW_IF},
        {"else",        token_type::KW_ELSE},
        {"while",       token_type::KW_WHILE},
        {"for",         token_type::KW_FOR},
        {"goto",        token_type::KW_GOTO},
        {"function",    token_type::KW_FUNCTION},
        {"foreach",     token_type::KW_FOREACH},
        {"with",        token_type::KW_WITH},
        {"import",      token_type::KW_IMPORT},
        {"break",       token_type::KW_BREAK},
        {"continue",    token_type::KW_CONTINUE},
        {"return",      token_type::KW_RETURN},
        {"clear",       token_type::KW_CLEAR},
        {"global",      token_type::KW_GLOBAL}
    };

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
                ? intl::format("UNEXPECTED_TOKEN", enums::get_data(tok.type))
                : intl::format("UNEXPECTED_TOKEN_REQUIRED", enums::get_data(tok.type), enums::get_data(expected)), tok),
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

        std::string token_location_info(const token &tok);

    private:
        std::function<void(comment_line)> comment_callback;
        simple_stack<comment_line> comment_lines;

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