#include "tokenizer.h"

#include <fmt/format.h>
#include "layout.h"

tokenizer::tokenizer(const std::string_view &_script) : script(_script) {
    _current = script.begin();
}

char tokenizer::nextChar() {
    if (_current == script.end())
        return '\0';
    return *_current++;
}

void tokenizer::skipSpaces() {
    while (_current != script.end()) {
        switch (*_current) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            ++_current;
            break;
        default:
            return;
        }
    }
}

bool tokenizer::next() {
    skipSpaces();
    
    auto start = _current;

    char c = nextChar();
    bool ok = true;

    switch (c) {
    case '\0':
        tok.type = TOK_END_OF_FILE;
        break;
    case '$':
        tok.type = TOK_FUNCTION;
        ok = readIdentifier();
        break;
    case '"':
        tok.type = TOK_STRING;
        ok = readString();
        break;
    case '/':
        tok.type = TOK_REGEXP;
        ok = readRegexp();
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        ok = readNumber();
        if (std::find(start, _current, '.') == _current) {
            tok.type = TOK_INTEGER;
        } else {
            tok.type = TOK_NUMBER;
        }
        break;
    case ',':
        tok.type = TOK_COMMA;
        break;
    case '(':
        tok.type = TOK_PAREN_BEGIN;
        break;
    case ')':
        tok.type = TOK_PAREN_END;
        break;
    case '[':
        tok.type = TOK_BRACKET_BEGIN;
        break;
    case ']':
        tok.type = TOK_BRACKET_END;
        break;
    case '{':
        tok.type = TOK_BRACE_BEGIN;
        break;
    case '}':
        tok.type = TOK_BRACE_END;
        break;
    case '=':
        tok.type = TOK_EQUALS;
        break;
    case '%':
        tok.type = TOK_PERCENT;
        break;
    case '!':
        tok.type = TOK_DEBUG;
        break;
    case '*':
        tok.type = TOK_GLOBAL;
        break;
    case '#':
        tok.type = TOK_COMMENT;
        ok = readComment();
        if (ok) {
            // skip comments
            return next();
        }
        break;
    case '@':
        tok.type = TOK_CONTENT;
        break;
    case ':':
        tok.type = TOK_COLON;
        break;
    case '+':
        tok.type = TOK_PLUS;
        break;
    case '-':
        tok.type = TOK_MINUS;
        break;
    default:
        tok.type = TOK_IDENTIFIER;
        ok = readIdentifier();
        break;
    }

    if (!ok) tok.type = TOK_ERROR;
    tok.value = std::string_view(start, _current - start);

    return ok;
}

token tokenizer::require(token_type type) {
    next();
    if (current().type != type) {
        throw unexpected_token(type);
    }
    return current();
}

bool tokenizer::peek() {
    auto pos = _current;
    bool ret = next();
    _current = pos;
    return ret;
}

void tokenizer::advance() {
    _current = tok.value.begin() + tok.value.size();
}

void tokenizer::gotoTok(const token &tok) {
    _current = tok.value.begin();
}

parsing_error tokenizer::unexpected_token(token_type type) {
    return parsing_error(fmt::format("Imprevisto '{0}', richiesto {1}", current().value, TOKEN_NAMES[type]), getLocation(current()));
}

parsing_error tokenizer::unexpected_token() {
    return parsing_error(fmt::format("Imprevisto '{0}'", current().value), getLocation(current()));
}

std::string tokenizer::getLocation(const token &tok) {
    size_t numline = 1;
    size_t loc = 1;
    bool found = false;
    std::string line;
    for (const char *c = script.begin(); c < script.end(); ++c) {
        if (*c != '\n') line += *c;
        if (c == tok.value.begin()) {
            found = true;
        }
        if (!found) {
            ++loc;
            if (*c == '\n') {
                ++numline;
                loc = 1;
                line.clear();
            }
        } else if (*c == '\n') {
            break;
        }
    }
    return fmt::format("{0}: Ln {1}, Col {2}:\n{3:->{2}}", line, numline, loc, '^');
}

bool tokenizer::readIdentifier() {
    const char *p = _current;
    char c = *p;
    while ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') {
        c = (_current = p) < script.end() ? *p++ : '\0';
    }
    return true;
}

bool tokenizer::readString() {
    while (true) {
        switch (nextChar()) {
        case '\\':
            nextChar();
            break;
        case '"':
            return true;
        case '\n':
        case '\0':
            return false;
        }
    }
    return false;
}

bool tokenizer::readRegexp() {
    while (true) {
        switch (nextChar()) {
        case '\\':
            nextChar();
            break;
        case '/':
            return true;
        case '\n':
        case '\0':
            return false;
        }
    }
    return false;
}

bool tokenizer::readNumber() {
    const char *p = _current;
    char c = *p;
    while ((c >= '0' && c <= '9') || c == '.') {
        c = (_current = p) < script.end() ? *p++ : '\0';
    }
    return true;
}

bool tokenizer::readComment() {
    while (true) {
        switch(nextChar()) {
        case '\n':
        case '\0':
            return true;
        }
    }
    return true;
}