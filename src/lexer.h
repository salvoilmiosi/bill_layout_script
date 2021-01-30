#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <vector>
#include <stdexcept>
#include <fmt/format.h>

#include "bytecode.h"

#define TOKENS { \
    T(TOK_ERROR,            "errore"),          /* errore */ \
    T(TOK_END_OF_FILE,      "EOF"),             /* fine file */ \
    T(TOK_IDENTIFIER,       "identificatore"),  /* [a-bA-B_][a-bA-B0-9_]* */ \
    T(TOK_STRING,           "\"stringa\""), \
    T(TOK_REGEXP,           "/regexp/"), \
    T(TOK_NUMBER,           "numero"),          /* 123.4 */ \
    T(TOK_INTEGER,          "intero"),          /* 123 */ \
    T(TOK_FUNCTION,         "$funzione"), \
    T(TOK_AMPERSAND,        "&"), \
    T(TOK_PAREN_BEGIN,      "("), \
    T(TOK_PAREN_END,        ")"), \
    T(TOK_COMMA,            ","), \
    T(TOK_BRACKET_BEGIN,    "["), \
    T(TOK_BRACKET_END,      "]"), \
    T(TOK_BRACE_BEGIN,      "{"), \
    T(TOK_BRACE_END,        "}"), \
    T(TOK_ASSIGN,           "="), \
    T(TOK_CONTENT,          "@"), \
    T(TOK_COLON,            ":"), \
    T(TOK_GLOBAL,           "::"), \
    T(TOK_PERCENT,          "%"), \
    T(TOK_ASTERISK,         "*"), \
    T(TOK_SLASH,            "/"), \
    T(TOK_PLUS,             "+"), \
    T(TOK_MINUS,            "-"), \
    T(TOK_AND,              "&&"), \
    T(TOK_OR,               "||"), \
    T(TOK_NOT,              "!"), \
    T(TOK_EQUALS,           "=="), \
    T(TOK_NOT_EQUALS,       "!="), \
    T(TOK_GREATER,          ">"), \
    T(TOK_LESS,             "<"), \
    T(TOK_GREATER_EQ,       ">="), \
    T(TOK_LESS_EQ,          "<="), \
}

#define T(x, y) x
enum token_type TOKENS;
#undef T

#define T(x, y) y
static const char *token_names[] = TOKENS;
#undef T

struct token {
    token_type type;
    std::string_view value;

    operator bool () {
        return type != TOK_ERROR;
    }
};

class parsing_error : public std::runtime_error {
protected:
    token m_location;

public:
    parsing_error(const std::string &message, token location) : std::runtime_error(message), m_location(location) {}

    token location() const noexcept {
        return m_location;
    }
};

inline std::string_view token_string(token tok) {
    if (tok.type == TOK_END_OF_FILE) {
        return "EOF";
    } else {
        return tok.value;
    }
}

class unexpected_token : public parsing_error {
protected:
    token_type m_expected;

public:
    unexpected_token(token tok, token_type expected = TOK_ERROR)
        : parsing_error(expected == TOK_ERROR
            ? fmt::format("Imprevisto '{}'", token_string(tok))
            : fmt::format("Imprevisto '{}', richiesto '{}'", token_string(tok), token_names[expected]), tok),
        m_expected(expected) {}

    token_type expected() {
        return m_expected;
    }
};

class lexer {
public:
    void set_script(std::string_view str);

    void set_bytecode(bytecode *code) {
        m_code = code;
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
    bytecode *m_code = nullptr;

    size_t last_debug_line;
    std::vector<std::string> debug_lines;

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