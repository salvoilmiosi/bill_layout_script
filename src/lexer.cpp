#include "lexer.h"
#include "unicode.h"

#include <cassert>

using namespace bls;

std::string token::parse_string() {
    assert (type == token_type::STRING || type == token_type::REGEXP);
    try {
        return unicode::parseQuotedString(value, type == token_type::REGEXP);
    } catch (const std::invalid_argument &error) {
        throw token_error(error.what(), *this);
    }
}

void lexer::set_script(std::string_view str) {
    m_begin = m_line_end = m_current = str.data();
    m_end = m_begin + str.size();

    m_line_count = 0;
    onLineStart();
}

char lexer::nextChar() {
    if (m_current == m_end)
        return '\0';
    return *m_current++;
}

void lexer::skipSpaces() {
    bool comment = false;
    while (m_current != m_end) {
        switch (*m_current) {
        case '\r':
        case '\n':
            comment = false;
            onLineStart();
            [[fallthrough]];
        case ' ':
        case '\t':
            ++m_current;
            break;
        case '#': // Skip comments
            comment = true;
            ++m_current;
            break;
        default:
            if (comment) {
                ++m_current;
            } else {
                return;
            }
        }
    }
}

std::string lexer::token_location_info(const token &tok) const {
    const char *tok_line_begin = m_begin;
    const char *tok_line_end = tok.value.data();
    size_t tok_line_count = 1;
    for (const char *c = m_begin; c != tok_line_end; ++c) {
        if (*c == '\n') {
            tok_line_begin = c + 1;
            ++tok_line_count;
        }
    }
    for (; tok_line_end != m_end && *tok_line_end != '\n'; ++tok_line_end);
    return std::format("Ln {0}\n{1}\n{2}{4:~<{3}}",
        tok_line_count, std::string_view{tok_line_begin, tok_line_end},
        std::string(tok.value.data() - tok_line_begin, '-'), std::max(size_t(1), tok.value.size()), '^'
    );
}

void lexer::onLineStart() {
    ++m_line_count;
    
    auto m_line_begin = m_line_end;
    for (; m_line_end != m_end && *m_line_end != '\n'; ++m_line_end);
    for (; m_line_begin != m_line_end && isspace(*m_line_begin); ++m_line_begin);
    std::string_view line{m_line_begin, m_line_end};
    if (!line.empty() && comment_callback) {
        comment_lines.push_back(std::format("{}: {}", m_line_count, line));
    }
    ++m_line_end;
}

token lexer::next(bool do_advance) {
    token tok;

    skipSpaces();
    
    auto start = m_current;

    char c = nextChar();
    bool ok = true;

    switch (c) {
    case '\0':
        tok.type = token_type::END_OF_FILE;
        break;
    case '"':
        tok.type = token_type::STRING;
        ok = readString();
        break;
    case '/':
        tok.type = token_type::SLASH;
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
        if (std::find(start, m_current, '.') == m_current) {
            tok.type = token_type::INTEGER;
        } else {
            tok.type = token_type::NUMBER;
        }
        break;
    case '$':
        tok.type = token_type::DOLLAR;
        break;
    case ',':
        tok.type = token_type::COMMA;
        break;
    case '(':
        tok.type = token_type::PAREN_BEGIN;
        break;
    case ')':
        tok.type = token_type::PAREN_END;
        break;
    case '[':
        tok.type = token_type::BRACKET_BEGIN;
        break;
    case ']':
        tok.type = token_type::BRACKET_END;
        break;
    case '{':
        tok.type = token_type::BRACE_BEGIN;
        break;
    case '}':
        tok.type = token_type::BRACE_END;
        break;
    case '=':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::EQUALS;
        } else {
            tok.type = token_type::ASSIGN;
        }
        break;
    case '*':
        tok.type = token_type::ASTERISK;
        break;
    case ';':
        tok.type = token_type::SEMICOLON;
        break;
    case '&':
        if (*m_current == '&') {
            nextChar();
            tok.type = token_type::AND;
        } else {
            ok = false;
        }
        break;
    case '|':
        if (*m_current == '|') {
            nextChar();
            tok.type = token_type::OR;
        } else {
            ok = false;
        }
        break;
    case '!':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::NOT_EQUALS;
        } else {
            tok.type = token_type::NOT;
        }
        break;
    case '@':
        tok.type = token_type::CONTENT;
        break;
    case ':':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::FORCE_ASSIGN;
        } else {
            tok.type = token_type::COLON;
        }
        break;
    case '+':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::ADD_ASSIGN;
        } else if (*m_current == '+') {
            nextChar();
            tok.type = token_type::ADD_ONE;
        } else {
            tok.type = token_type::PLUS;
        }
        break;
    case '-':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::SUB_ASSIGN;
        } else if (*m_current == '-') {
            nextChar();
            tok.type = token_type::SUB_ONE;
        } else {
            tok.type = token_type::MINUS;
        }
        break;
    case '>':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::GREATER_EQ;
        } else {
            tok.type = token_type::GREATER;
        }
        break;
    case '<':
        if (*m_current == '=') {
            nextChar();
            tok.type = token_type::LESS_EQ;
        } else {
            tok.type = token_type::LESS;
        }
        break;
    case 'a': case 'A':
    case 'b': case 'B':
    case 'c': case 'C':
    case 'd': case 'D':
    case 'e': case 'E':
    case 'f': case 'F':
    case 'g': case 'G':
    case 'h': case 'H':
    case 'i': case 'I':
    case 'j': case 'J':
    case 'k': case 'K':
    case 'l': case 'L':
    case 'm': case 'M':
    case 'n': case 'N':
    case 'o': case 'O':
    case 'p': case 'P':
    case 'q': case 'Q':
    case 'r': case 'R':
    case 's': case 'S':
    case 't': case 'T':
    case 'u': case 'U':
    case 'v': case 'V':
    case 'w': case 'W':
    case 'x': case 'X':
    case 'y': case 'Y':
    case 'z': case 'Z':
    case '_':
        ok = readIdentifier();
        if (auto it = keyword_tokens.find(std::string_view{start, m_current}); it != keyword_tokens.end()) {
            tok.type = it->second;
        } else {
            tok.type = token_type::IDENTIFIER;
        }
        break;
    default:
        ok = false;
        break;
    }

    if (!ok) tok.type = token_type::INVALID;
    tok.value = std::string_view(start, m_current);

    if (do_advance) {
        advance(tok);
    } else {
        m_current = start;
    }
    return tok;
}

token lexer::require(token_type type) {
    token tok;
    if (type == token_type::REGEXP) {
        auto begin = m_current;
        require(token_type::SLASH);
        if (readRegexp()) {
            tok.value = std::string_view(begin, m_current);
            tok.type = token_type::REGEXP;
        } else {
            tok.type = token_type::INVALID;
        }
    } else {
        tok = next();
    }
    if (tok.type != type) {
        throw unexpected_token(tok, type);
    }
    return tok;
}

token lexer::check_next(token_type type) {
    token tok = peek();
    if (tok.type != type) {
        tok.type = token_type::INVALID;
    } else {
        advance(tok);
    }
    return tok;
}

void lexer::advance(token tok) {
    if (comment_callback) {
        for (auto &line : comment_lines) {
            comment_callback(std::move(line));
        }
    }
    comment_lines.clear();

    m_current = tok.value.data() + tok.value.size();
}

bool lexer::readIdentifier() {
    auto p = m_current;
    char c = *p;
    while ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') {
        c = (m_current = p) < m_end ? *p++ : '\0';
    }
    return true;
}

bool lexer::readString() {
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

bool lexer::readRegexp() {
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

bool lexer::readNumber() {
    auto p = m_current;
    char c = '0';
    while (c >= '0' && c <= '9') {
        c = (m_current = p) < m_end ? *p++ : '\0';
    }
    if (c == '.') {
        c = (m_current = p) < m_end ? *p++ : '\0';
        while (c >= '0' && c <= '9')
            c = (m_current = p) < m_end ? *p++ : '\0';
    }
    return true;
}