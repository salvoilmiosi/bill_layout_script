#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <functional>
#include <fmt/format.h>

#include "enums.h"
#include "stack.h"

DEFINE_ENUM(token_type,
    (INVALID,      "Token Invalido")
    (END_OF_FILE,  "Fine File")
    (IDENTIFIER,   "Identificatore")
    (STRING,       "Stringa")
    (REGEXP,       "Espressione Regolare")
    (NUMBER,       "Numero")
    (INTEGER,      "Numero Intero")
    (FUNCTION,     "Funzione")
    (SEMICOLON,    ";")
    (AMPERSAND,    "&")
    (PAREN_BEGIN,  "(")
    (PAREN_END,    ")")
    (COMMA,        ",")
    (BRACKET_BEGIN,"[")
    (BRACKET_END,  "]")
    (BRACE_BEGIN,  "{")
    (BRACE_END,    "}")
    (ASSIGN,       "=")
    (ADD_ASSIGN,   "+=")
    (SUB_ASSIGN,   "-=")
    (CONTENT,      "@")
    (TILDE,        "~")
    (COLON,        ":")
    (GLOBAL,       "::")
    (SINGLE_QUOTE, "'")
    (PERCENT,      "%")
    (CARET,        "^")
    (ASTERISK,     "*")
    (SLASH,        "/")
    (PLUS,         "+")
    (MINUS,        "-")
    (AND,          "&&")
    (OR,           "||")
    (NOT,          "!")
    (EQUALS,       "==")
    (NOT_EQUALS,   "!=")
    (GREATER,      ">")
    (LESS,         "<")
    (GREATER_EQ,   ">=")
    (LESS_EQ,      "<=")
)

struct token {
    token_type type;
    std::string_view value;

    operator bool () {
        return type != token_type::INVALID;
    }

    std::string parse_string();
};

class parsing_error : public std::runtime_error {
protected:
    token m_location;

public:
    template<typename T>
    parsing_error(T &&message, token location) :
        std::runtime_error(std::forward<T>(message)), m_location(location) {}

    token location() const noexcept {
        return m_location;
    }
};

class unexpected_token : public parsing_error {
protected:
    token_type m_expected;

public:
    unexpected_token(token tok, token_type expected = token_type::INVALID)
        : parsing_error(expected == token_type::INVALID
            ? fmt::format("Imprevisto '{}'", EnumData<const char *>(tok.type))
            : fmt::format("Imprevisto '{}', richiesto '{}'", EnumData<const char *>(tok.type), EnumData<const char *>(expected)), tok),
        m_expected(expected) {}

    token_type expected() {
        return m_expected;
    }
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
    std::function<void(const std::string &)> comment_callback;

    size_t last_debug_line;
    simple_stack<std::string> debug_lines;

    std::string_view script;

    std::string_view::iterator m_current;

    char nextChar();
    void skipSpaces();
    
    void addDebugData();
    void flushDebugData();

    bool readIdentifier();
    bool readString();
    bool readRegexp();
    bool readNumber();
};

#endif