#ifndef __LEXER_H__
#define __LEXER_H__

#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <fmt/format.h>

#define TOKENS \
T(INVALID,      "Token Invalido") \
T(END_OF_FILE,  "Fine File") \
T(IDENTIFIER,   "Identificatore") \
T(STRING,       "Stringa") \
T(REGEXP,       "Espressione Regolare") \
T(NUMBER,       "Numero") \
T(INTEGER,      "Numero Intero") \
T(FUNCTION,     "Funzione") \
T(AMPERSAND,    "&") \
T(PAREN_BEGIN,  "(") \
T(PAREN_END,    ")") \
T(COMMA,        ",") \
T(BRACKET_BEGIN,"[") \
T(BRACKET_END,  "]") \
T(BRACE_BEGIN,  "{") \
T(BRACE_END,    "}") \
T(ASSIGN,       "=") \
T(ADD_ASSIGN,   "+=") \
T(SUB_ASSIGN,   "-=") \
T(CONTENT,      "@") \
T(TILDE,        "~") \
T(COLON,        ":") \
T(GLOBAL,       "::") \
T(SINGLE_QUOTE, "'") \
T(PERCENT,      "%") \
T(CARET,        "^") \
T(ASTERISK,     "*") \
T(SLASH,        "/") \
T(PLUS,         "+") \
T(MINUS,        "-") \
T(AND,          "&&") \
T(OR,           "||") \
T(NOT,          "!") \
T(EQUALS,       "==") \
T(NOT_EQUALS,   "!=") \
T(GREATER,      ">") \
T(LESS,         "<") \
T(GREATER_EQ,   ">=") \
T(LESS_EQ,      "<=")

#define T(x, y) TOK_##x,
enum token_type { TOKENS };
#undef T
#define T(x, y) y,
static const char *token_names[] = { TOKENS };
#undef T

struct token {
    token_type type;
    std::string_view value;

    operator bool () {
        return type != TOK_INVALID;
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
    unexpected_token(token tok, token_type expected = TOK_INVALID)
        : parsing_error(expected == TOK_INVALID
            ? fmt::format("Imprevisto '{}'", token_names[tok.type])
            : fmt::format("Imprevisto '{}', richiesto '{}'", token_names[tok.type], token_names[expected]), tok),
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