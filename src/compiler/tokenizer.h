#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include <string>

enum token_type {
    TOK_END_OF_FILE = 0,
    TOK_ERROR,
    TOK_COMMENT,            // # commento
    TOK_IDENTIFIER,         // [a-bA-B_][a-bA-B0-9_]+
    TOK_STRING,             // "xyz"
    TOK_REGEXP,             // /xyz/
    TOK_NUMBER,             // 123
    TOK_FUNCTION,           // $
    TOK_PAREN_BEGIN,        // (
    TOK_PAREN_END,          // )
    TOK_COMMA,              // ,
    TOK_BRACKET_BEGIN,      // [
    TOK_BRACKET_END,        // ]
    TOK_BRACE_BEGIN,        // {
    TOK_BRACE_END,          // }
    TOK_EQUALS,             // =
    TOK_PERCENT,            // %
    TOK_GLOBAL,             // *
    TOK_DEBUG,              // !
    TOK_COLON,              // :
    TOK_CONTENT,            // @
    TOK_PLUS,               // +
    TOK_MINUS,              // -
};

constexpr const char *TOKEN_NAMES[] = {
    "eof",
    "errore",
    "commento",
    "identificatore",
    "stringa",
    "regexp",
    "numero",
    "'$'",
    "'('",
    "')'",
    "','",
    "'['",
    "']'",
    "'{'",
    "'}'",
    "'='",
    "'%'",
    "'*'",
    "'!'",
    "':'",
    "'@'",
    "'+'",
    "'-'",
};

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line) : message(message), line(line) {}
};

struct token {
    token_type type;
    std::string_view value;
};

class tokenizer {
public:
    tokenizer() {}
    tokenizer(const std::string_view &script);

    bool next();
    token require(token_type type);

    bool peek();
    void advance();

    parsing_error unexpected_token(token_type type);
    parsing_error unexpected_token();

    void gotoTok(const token &tok);

    bool ate() {
        return _current == script.end();
    }

    const token &current() const {
        return tok;
    }

    std::string getLocation(const token &tok);

private:
    std::string_view script;

    std::string_view::iterator _current;
    token tok;

    char nextChar();
    void skipSpaces();

    bool readIdentifier();
    bool readString();
    bool readRegexp();
    bool readNumber();
    bool readComment();
};

#endif