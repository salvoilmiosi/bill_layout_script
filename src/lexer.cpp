#include "lexer.h"

void lexer::set_script(std::string_view str) {
    script = str;
    m_current = script.begin();

    last_debug_line = -1;
    addDebugData();
    flushDebugData();
}

char lexer::nextChar() {
    if (m_current == script.end())
        return '\0';
    return *m_current++;
}

void lexer::skipSpaces() {
    bool comment = false;
    while (m_current != script.end()) {
        switch (*m_current) {
        case '\r':
        case '\n':
            comment = false;
            addDebugData();
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

void lexer::addDebugData() {
    if (comment_callback) {
        size_t begin = script.find_first_not_of(" \t\r\n", m_current - script.begin());
        if (begin != std::string::npos && begin != last_debug_line) {
            last_debug_line = begin;
            size_t endline = script.find_first_of("\r\n", begin);
            auto line = script.substr(begin, endline == std::string_view::npos ? std::string_view::npos : endline - begin);
            debug_lines.push_back(std::string(line));
        }
    }
}

void lexer::flushDebugData() {
    if (comment_callback) {
        for (auto &line : debug_lines) {
            comment_callback(line);
        }
    }
    debug_lines.clear();
}

token lexer::next(bool do_advance) {
    token tok;

    skipSpaces();
    if (do_advance) {
        flushDebugData();
    }
    
    auto start = m_current;

    char c = nextChar();
    bool ok = true;

    switch (c) {
    case '\0':
        tok.type = token_type::END_OF_FILE;
        break;
    case '$':
        tok.type = token_type::FUNCTION;
        ok = readIdentifier();
        break;
    case '"':
        tok.type = token_type::STRING;
        ok = readString();
        break;
    case '/':
        tok.type = token_type::SLASH;
        break;
    case '0' ... '9':
        ok = readNumber();
        if (std::find(start, m_current, '.') == m_current) {
            tok.type = token_type::INTEGER;
        } else {
            tok.type = token_type::NUMBER;
        }
        break;
    case '.':
        tok.type = token_type::DOT;
        break;
    case '\'':
        tok.type = token_type::SINGLE_QUOTE;
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
    case '%':
        tok.type = token_type::PERCENT;
        break;
    case '^':
        tok.type = token_type::CARET;
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
            tok.type = token_type::AMPERSAND;
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
    case '~':
        tok.type = token_type::TILDE;
        break;
    case ':':
        tok.type = token_type::COLON;
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
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        tok.type = token_type::IDENTIFIER;
        ok = readIdentifier();
        break;
    default:
        ok = false;
        break;
    }

    if (!ok) tok.type = token_type::INVALID;
    tok.value = std::string_view(start, m_current - start);

    if (!do_advance) {
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
            tok.value = std::string_view(begin, m_current - begin);
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
    flushDebugData();
    m_current = tok.value.begin() + tok.value.size();
}

std::string lexer::token_location_info(const token &tok) {
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
    return fmt::format("{0}\n{3:->{2}}\nLn {1}, Col {2}", line, numline, loc, '^');
}

bool lexer::readIdentifier() {
    const char *p = m_current;
    char c = *p;
    while ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') {
        c = (m_current = p) < script.end() ? *p++ : '\0';
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
    const char *p = m_current;
    char c = '0';
    while (c >= '0' && c <= '9') {
        c = (m_current = p) < script.end() ? *p++ : '\0';
    }
    if (c == '.') {
        c = (m_current = p) < script.end() ? *p++ : '\0';
        while (c >= '0' && c <= '9')
            c = (m_current = p) < script.end() ? *p++ : '\0';
    }
    return true;
}