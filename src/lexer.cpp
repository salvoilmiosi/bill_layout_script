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
    constexpr auto keyword_tokens = []<token_type ... Es>(enums::enum_sequence<Es...>) {
        return util::static_map<std::string_view, token_type>(
            {{enums::enum_data_v<Es>.value, Es} ... }
        );
    }(enums::filter_enum_sequence<is_keyword, enums::make_enum_sequence<token_type>>());

    constexpr auto symbols_table = []{
        std::array<std::array<token_type, 256>, 256> ret;
        for (auto [str, value] : []<token_type ... Es>(enums::enum_sequence<Es...>) {
            return std::array{std::make_pair(enums::enum_data_v<Es>.value, Es) ... };
        }(enums::filter_enum_sequence<is_symbol, enums::make_enum_sequence<token_type>>())) {
            assert(str.size() <= 2);
            if (str.size() == 1) {
                ret[str[0]][0] = value;
            } else {
                ret[str[0]][str[1]] = value;
            }
        }
        return ret;
    }();

    token tok;

    skipSpaces();
    
    auto start = m_current;

    char c = nextChar();
    if (c == '\0') {
        tok.type = token_type::END_OF_FILE;
    } else if (c == '"') {
        if (readString()) {
            tok.type = token_type::STRING;
        } else {
            tok.type = token_type::INVALID;
        }
    } else if (c >= '0' && c <= '9') {
        if (readNumber()) {
            if (std::find(start, m_current, '.') == m_current) {
                tok.type = token_type::INTEGER;
            } else {
                tok.type = token_type::NUMBER;
            }
        } else {
            tok.type = token_type::INVALID;
        }
    } else if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        if (readIdentifier()) {
            if (auto it = keyword_tokens.find(std::string_view{start, m_current}); it != keyword_tokens.end()) {
                tok.type = it->second;
            } else {
                tok.type = token_type::IDENTIFIER;
            }
        } else {
            tok.type = token_type::INVALID;
        }
    } else if (tok.type = symbols_table[c][*m_current]; tok.type != token_type::INVALID) {
        nextChar();
    } else {
        tok.type = symbols_table[c][0];
    }

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