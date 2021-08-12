#include "unicode.h"
#include "utils.h"

typedef std::string_view::iterator location;

// borrowed from jsoncpp source code

static bool decodeUnicodeEscapeSequence(location& current, location end, unsigned int& ret_unicode) {
    int unicode = 0;
    for (int index = 0; index < 4; ++index) {
        char c = *current++;
        unicode *= 16;
        if (c >= '0' && c <= '9')
            unicode += c - '0';
        else if (c >= 'a' && c <= 'f')
            unicode += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            unicode += c - 'A' + 10;
        else
            return false;
    }
    ret_unicode = static_cast<unsigned int>(unicode);
    return true;
}

static bool decodeUnicodeCodePoint(location& current, location end, unsigned int& unicode) {
    if (!decodeUnicodeEscapeSequence(current, end, unicode))
        return false;
    if (unicode >= 0xD800 && unicode <= 0xDBFF) {
        // surrogate pairs
        if (end - current < 6)
            return false;
        if (*(current++) == '\\' && *(current++) == 'u') {
            unsigned int surrogatePair;
            if (decodeUnicodeEscapeSequence(current, end, surrogatePair)) {
                unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

/// Converts a unicode code-point to UTF-8.
static inline std::string codePointToUTF8(unsigned int cp) {
    std::string result;

    // based on description from http://en.wikipedia.org/wiki/UTF-8

    if (cp <= 0x7f) {
        result.resize(1);
        result[0] = static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        result.resize(2);
        result[1] = static_cast<char>(0x80 | (0x3f & cp));
        result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
    } else if (cp <= 0xFFFF) {
        result.resize(3);
        result[2] = static_cast<char>(0x80 | (0x3f & cp));
        result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
        result[0] = static_cast<char>(0xE0 | (0xf & (cp >> 12)));
    } else if (cp <= 0x10FFFF) {
        result.resize(4);
        result[3] = static_cast<char>(0x80 | (0x3f & cp));
        result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
        result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
        result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
    }

    return result;
}

std::string unicode::parseQuotedString(std::string_view str, bool is_regex) {
    std::string decoded;
    location current = str.begin() + 1;
    location end = str.end() - 1;
    while (current != end) {
        char c = *current++;
        if ((is_regex && c == '/') || (!is_regex && c == '"')) {
            break;
        }
        if (c == '\\') {
            if (current == end) {
                throw std::runtime_error(intl::format("INVALID_STRING_CONSTANT"));
            }
            char escape = *current++;
            switch (escape) {
            case '"':
                decoded += '"';
                break;
            case '/':
                decoded += '/';
                break;
            case '\\':
                decoded += '\\';
                break;
            case 'f':
                decoded += '\f';
                break;
            case 'n':
                decoded += '\n';
                break;
            case 'r':
                decoded += '\r';
                break;
            case 't':
                decoded += '\t';
                break;
            case 'u': {
                unsigned int unicode;
                if (!decodeUnicodeCodePoint(current, end, unicode)) {
                    throw std::runtime_error(intl::format("INVALID_STRING_CONSTANT"));
                }
                decoded += codePointToUTF8(unicode);
                break;
            }
            default:
                if (!is_regex) {
                    throw std::runtime_error(intl::format("INVALID_STRING_CONSTANT"));
                } else {
                    decoded += '\\';
                    decoded += escape;
                }
            }
        } else if (is_regex && c == ' ') {
            decoded += "(?:\\s+)";
        } else {
            decoded += c;
        }
    }

    return decoded;
}

static unsigned int utf8ToCodepoint(const char*& s, const char* e) {
    const unsigned int REPLACEMENT_CHARACTER = 0xFFFD;

    unsigned int firstByte = static_cast<unsigned char>(*s);

    if (firstByte < 0x80)
        return firstByte;

    if (firstByte < 0xE0) {
        if (e - s < 2)
            return REPLACEMENT_CHARACTER;

        unsigned int calculated = ((firstByte & 0x1F) << 6) | (static_cast<unsigned int>(s[1]) & 0x3F);
        s += 1;
        // oversized encoded characters are invalid
        return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
    }

    if (firstByte < 0xF0) {
        if (e - s < 3)
            return REPLACEMENT_CHARACTER;

        unsigned int calculated = ((firstByte & 0x0F) << 12) | ((static_cast<unsigned int>(s[1]) & 0x3F) << 6) | (static_cast<unsigned int>(s[2]) & 0x3F);
        s += 2;
        // surrogates aren't valid codepoints itself
        // shouldn't be UTF-8 encoded
        if (calculated >= 0xD800 && calculated <= 0xDFFF)
            return REPLACEMENT_CHARACTER;
        // oversized encoded characters are invalid
        return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
    }

    if (firstByte < 0xF8) {
        if (e - s < 4)
            return REPLACEMENT_CHARACTER;

        unsigned int calculated = ((firstByte & 0x07) << 18) | ((static_cast<unsigned int>(s[1]) & 0x3F) << 12) | ((static_cast<unsigned int>(s[2]) & 0x3F) << 6) | (static_cast<unsigned int>(s[3]) & 0x3F);
        s += 3;
        // oversized encoded characters are invalid
        return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
    }

    return REPLACEMENT_CHARACTER;
}

std::string unicode::escapeString(std::string_view str) {
    auto to_hex = [](unsigned ch) {
        return fmt::format("\\u{:04x}", ch);
    };

    std::string result = "\"";
    const char* end = str.data() + str.size();
    for (const char* c = str.data(); c != end; ++c) {
        switch (*c) {
        case '\"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        // case '/':
        // Even though \/ is considered a legal escape in JSON, a bare
        // slash is also legal, so I see no reason to escape it.
        // (I hope I am not misunderstanding something.)
        // blep notes: actually escaping \/ may be useful in javascript to avoid </
        // sequence.
        // Should add a flag to allow this compatibility mode and prevent this
        // sequence from occurring.
        default: {
            unsigned codepoint = utf8ToCodepoint(c, end); // modifies `c`
            if (codepoint < 0x20) {
                result += to_hex(codepoint);
            } else if (codepoint < 0x80) {
                result += static_cast<char>(codepoint);
            } else if (codepoint < 0x10000) {
                // Basic Multilingual Plane
                result += to_hex(codepoint);
            } else {
                // Extended Unicode. Encode 20 bits as a surrogate pair.
                codepoint -= 0x10000;
                result += to_hex(0xd800 + ((codepoint >> 10) & 0x3ff));
                result += to_hex(0xdc00 + (codepoint & 0x3ff));
            }
        } break;
        }
    }
    result += "\"";
    return result;
}