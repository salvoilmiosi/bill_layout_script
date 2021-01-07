#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include <string>
#include <vector>

enum token_type {
    TOK_END_OF_FILE = 0,
    TOK_ERROR,
    TOK_IDENTIFIER,         // [a-bA-B_][a-bA-B0-9_]*
    TOK_STRING,             // "xyz"
    TOK_REGEXP,             // /xyz/
    TOK_NUMBER,             // 123.4
    TOK_INTEGER,            // 123
    TOK_FUNCTION,           // $funzione
    TOK_AMPERSAND,          // &
    TOK_PAREN_BEGIN,        // (
    TOK_PAREN_END,          // )
    TOK_COMMA,              // ,
    TOK_BRACKET_BEGIN,      // [
    TOK_BRACKET_END,        // ]
    TOK_BRACE_BEGIN,        // {
    TOK_BRACE_END,          // }
    TOK_ASSIGN,             // =
    TOK_CONTENT,            // @
    TOK_COLON,              // :
    TOK_GLOBAL,             // ::
    TOK_PERCENT,            // %
    TOK_ASTERISK,           // *
    TOK_SLASH,              // /
    TOK_PLUS,               // +
    TOK_MINUS,              // -
    TOK_AND,                // &&
    TOK_OR,                 // ||
    TOK_NOT,                // !
    TOK_EQUALS,             // ==
    TOK_NOT_EQUALS,         // !=
    TOK_GREATER,            // >
    TOK_LESS,               // <
    TOK_GREATER_EQ,         // >=
    TOK_LESS_EQ,            // <=
};

constexpr const char *TOKEN_NAMES[] = {
    "eof",
    "errore",
    "identificatore",
    "stringa",
    "regexp",
    "numero",
    "numero intero",
    "$funzione",
    "'&'",
    "'('",
    "')'",
    "','",
    "'['",
    "']'",
    "'{'",
    "'}'",
    "'='",
    "'@'",
    "':'",
    "'%'",
    "'*'",
    "'/'",
    "'+'",
    "'-'",
    "'&&'",
    "'||'",
    "'!'",
    "'=='",
    "'!='",
    "'>'",
    "'<'",
    "'>='",
    "'<='"
};

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line) : message(message), line(line) {}
};

struct token {
    token_type type;
    std::string_view value;

    operator bool () {
        return type != TOK_ERROR;
    }
};

class tokenizer {
public:
    tokenizer(class parser &parent) : parent(parent) {}
    
    void setScript(std::string_view str);

    const token &next(bool do_advance = true);
    const token &peek() {
        return next(false);
    }
    
    token require(token_type type);

    token check_next(token_type type);

    void advance();

    parsing_error unexpected_token(token_type type);
    parsing_error unexpected_token();

    bool ate() {
        return m_current == script.end();
    }

    const token &current() const {
        return tok;
    }

    std::string getLocation(const token &tok);

private:
    class parser &parent;
    size_t last_debug_line;
    std::vector<std::string> debug_lines;

    std::string_view script;

    std::string_view::iterator m_current;
    token tok;

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