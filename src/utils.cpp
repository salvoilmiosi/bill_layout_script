#include "utils.h"

#include <algorithm>
#include <iostream>

std::vector<std::string> string_split(const std::string &str, char separator) {
    std::vector<std::string> ret;

    size_t start = 0;
    size_t end = str.find(separator);
    while (end != std::string::npos) {
        ret.push_back(str.substr(start, end-start));
        start = end + 1;
        end = str.find(separator, start);
    }
    ret.push_back(str.substr(start, end));

    return ret;
}

std::string string_join(const std::vector<std::string> &vec, const std::string &separator) {
    std::string out;
    for (auto it = vec.begin(); it<vec.end(); ++it) {
        if (it != vec.begin()) {
            out += separator;
        }
        out += *it;
    }
    return out;
};

std::string string_tolower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](auto ch) {
        return std::tolower(ch);
    });
    return str;
}

std::string string_toupper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](auto ch) {
        return std::toupper(ch);
    });
    return str;
}

void string_trim(std::string &str) {
#ifdef _WIN32
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
#endif
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](auto ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](auto ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

int string_replace(std::string &str, const std::string &from, const std::string &to) {
    size_t index = 0;
    int count = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, from.size(), to);
        index += to.size();
        ++count;
    }
    return count;
}

std::string read_all(std::istream &stream) {
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}