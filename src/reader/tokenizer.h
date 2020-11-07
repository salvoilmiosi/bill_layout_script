#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include <string>

enum token_type {
    TOK_END_OF_FILE = 0,
    TOK_ERROR,
    TOK_COMMENT,            // # comment
    TOK_IDENTIFIER,         // [a-bA-B_][a-bA-B0-9_]+
    TOK_STRING,             // "xyz"
    TOK_NUMBER,             // 123
    TOK_FUNCTION,           // $
    TOK_SYS_FUNCTION,       // &
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
    TOK_APPEND,             // +
    TOK_CLEAR,              // :
    TOK_CONTENT,            // @
};

struct token {
    token_type type;
    std::string_view value;
};

class tokenizer {
public:
    tokenizer(const std::string_view &script);

    bool next(bool peek = false);
    void advance();
    void gotoTok(const token &tok) {
        _current = tok.value.begin();
    }

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
    bool readNumber();
    bool readComment();
};

#endif