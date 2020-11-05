#include "tokenizer.h"

#include <fmt/format.h>
#include "../shared/utils.h"

tokenizer::tokenizer(const std::string_view &_script) : script(_script) {
    current = script.begin();
}

char tokenizer::nextChar() {
    if (current == script.end())
        return '\0';
    return *current++;
}

void tokenizer::skipSpaces() {
    while (current != script.end()) {
        switch (*current) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            ++current;
            break;
        default:
            return;
        }
    }
}

bool tokenizer::nextToken(token &out, bool advance) {
    skipSpaces();
    
    auto start = current;

    char c = nextChar();
    bool ok = true;

    switch (c) {
    case '\0':
        out.type = TOK_END_OF_FILE;
        break;
    case '$':
        out.type = TOK_FUNCTION;
        break;
    case '"':
        out.type = TOK_STRING;
        ok = readString();
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
    case '-':
        out.type = TOK_NUMBER;
        ok = readNumber();
        break;
    case ',':
        out.type = TOK_COMMA;
        break;
    case '(':
        out.type = TOK_BRACE_BEGIN;
        break;
    case ')':
        out.type = TOK_BRACE_END;
        break;
    case '[':
        out.type = TOK_BRACKET_BEGIN;
        break;
    case ']':
        out.type = TOK_BRACKET_END;
        break;
    case '=':
        out.type = TOK_EQUALS;
        break;
    case '%':
        out.type = TOK_PERCENT;
        break;
    case '!':
        out.type = TOK_DEBUG;
        break;
    case '*':
        out.type = TOK_GLOBAL;
        break;
    case '#':
        out.type = TOK_COMMENT;
        ok = readComment();
        break;
    case '@':
        out.type = TOK_CONTENT;
        break;
    case '+':
        out.type = TOK_APPEND;
        break;
    case ':':
        out.type = TOK_CLEAR;
        break;
    case ';':
        out.type = TOK_END_EXPR;
        break;
    default:
        out.type = TOK_IDENTIFIER;
        ok = readIdentifier();
        break;
    }

    if (!ok) out.type = TOK_ERROR;
    out.value = std::string_view(start, current - start);

    if (!advance) {
        current = start;
    }

    return ok;
}

void tokenizer::advance(const token &tok) {
    current += tok.value.size();
}

std::string tokenizer::getLocation(const token &tok) {
    size_t numline = 1;
    size_t loc = 0;
    bool found = false;
    std::string line;
    for (const char *c = script.begin(); c < script.end(); ++c) {
        line += *c;
        if (c == tok.value) {
            found = true;
        }
        if (!found) {
            ++loc;
            if (*c == '\n') {
                ++numline;
                loc = 0;
                line.clear();
            }
        } else if (*c == '\n') {
            break;
        }
    }
    return fmt::format("{0} at Ln {1}, Col {2}", line, numline, loc);
}

bool tokenizer::readIdentifier() {
    const char *p = current;
    char c = *p;
    if (c >= '0' && c <= '9') {
        return false;
    }
    while ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') {
        c = (current = p) < script.end() ? *p++ : '\0';
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

bool tokenizer::readNumber() {
    const char *p = current;
    char c = *p;
    while ((c >= '0' && c <= '9') || c == '.') {
        c = (current = p) < script.end() ? *p++ : '\0';
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