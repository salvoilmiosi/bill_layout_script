#include "pdf_document.h"

#include <filesystem>
#include <regex>

#include <fmt/format.h>

#include "subprocess.h"
#include "utils.h"

void pdf_document::open(const std::string &filename) {
    if (!std::filesystem::exists(filename)) {
        throw pdf_error(fmt::format("Impossibile aprire il file \"{0}\"", filename));
    }

    try {
        subprocess process(arguments(
            "pdfinfo", filename
        ));

        m_filename = filename;

        std::string line;
        while (std::getline(process.stream_out, line)) {
            std::smatch match;
            
            if (std::regex_search(line, match, std::regex("Pages: +([0-9]+)"))) {
                m_num_pages = std::stoi(match.str(1));
            } else if (std::regex_search(line, match, std::regex("Page size: +([0-9\\.]+) x ([0-9\\.]+)"))) {
                m_width = std::stof(match.str(1));
                m_height = std::stof(match.str(2));
            }
        }
    } catch (const process_error &error) {
        throw pdf_error(error.message);
    }
}

constexpr float RESOLUTION = 300;
constexpr float resolution_factor = RESOLUTION / 72.f;

std::string pdf_document::get_text(const pdf_rect &rect) const {
    if (!isopen()) return "";
    if (rect.page > m_num_pages) return "";

    try {
        arguments<30> args("pdftotext", "-q", "-eol", "unix");

        auto p = std::to_string(rect.page);
        auto x = std::to_string((int)(m_width * rect.x * resolution_factor));
        auto y = std::to_string((int)(m_height * rect.y * resolution_factor));
        auto w = std::to_string((int)(m_width * rect.w * resolution_factor));
        auto h = std::to_string((int)(m_height * rect.h * resolution_factor));
        auto r = std::to_string((int)(RESOLUTION));

        switch (rect.type) {
        case box_type::BOX_RECTANGLE:
            args.add_args("-r", r, "-x", x, "-y", y, "-W", w, "-H", h);
            // fall through
        case box_type::BOX_PAGE:
            args.add_args("-f", p, "-l", p);
            break;
        case box_type::BOX_FILE:
            break;
        default:
            return "";
        }

        if (read_mode_options[static_cast<int>(rect.mode)]) args.add_args(read_mode_options[static_cast<int>(rect.mode)]);
        args.add_args(m_filename, "-");
        
        subprocess process(args);
        std::string str = read_all(process.stream_out);
        string_trim(str);
        return str;
    } catch (const process_error &error) {
        throw pdf_error(error.message);
    }
}