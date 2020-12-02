#include "parsestr.h"
#include "utils.h"

typedef std::string_view::iterator location;

// borrowed from jsoncpp source code

static bool decodeUnicodeEscapeSequence(location &current, location end, unsigned int &ret_unicode) {
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

static bool decodeUnicodeCodePoint(location &current, location end, unsigned int &unicode) {
  if (!decodeUnicodeEscapeSequence(current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    // surrogate pairs
    if (end - current < 6) return false;
    if (*(current++) == '\\' && *(current++) == 'u') {
      unsigned int surrogatePair;
      if (decodeUnicodeEscapeSequence(current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return false;
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

bool parse_string(std::string &decoded, std::string_view value) {
    location current = value.begin() + 1;
    location end = value.end() - 1;
    while (current != end) {
        char c = *current++;
        if (c == '"') break;
        if (c == '\\') {
            if (current == end) return false;
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
            case 'b':
                decoded += '\b';
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
            case 'u':
            {
                unsigned int unicode;
                if (!decodeUnicodeCodePoint(current, end, unicode))
                    return false;
                decoded += codePointToUTF8(unicode);
                break;
            }
            default:
                return false;
            }
        } else {
            decoded += c;
        }
    }
    return true;
}

bool parse_string_regexp(std::string &decoded, std::string_view value) {
    location current = value.begin() + 1;
    location end = value.end() - 1;
    while (current != end) {
        char c = *current++;
        if (c == '/') break;
        if (c == '\\') {
            if (current == end) return false;
            char escape = *current++;
            switch (escape) {
            case '/':
                decoded += '/';
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
            case 'u':
            {
                unsigned int unicode;
                if (!decodeUnicodeCodePoint(current, end, unicode))
                    return false;
                decoded += codePointToUTF8(unicode);
                break;
            }
            default:
                decoded += '\\';
                decoded += escape;
            }
        } else {
            decoded += c;
        }
    }
    
    string_replace(decoded, " ", "\\s+");
    string_replace(decoded, "%N", "[0-9\\.,-]+");
    return true;
}
