#include <charconv>
#include <array>
#include <iostream>
#include <string_view>

int main() {
    std::array<char, 16> str;
    if (auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), 3.1415); ec == std::errc()) {
        std::cout << std::string_view{str.data(), ptr} << '\n';
    }
    return 0;
}