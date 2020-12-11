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

    const char *args[] = {
        "pdfinfo", filename.c_str(), nullptr
    };

    try {
        subprocess process(args);

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
        const char *args[30] = {0};
        size_t nargs = 0;

        auto p = std::to_string(rect.page);
        auto x = std::to_string((int)(m_width * rect.x * resolution_factor));
        auto y = std::to_string((int)(m_height * rect.y * resolution_factor));
        auto w = std::to_string((int)(m_width * rect.w * resolution_factor));
        auto h = std::to_string((int)(m_height * rect.h * resolution_factor));
        auto r = std::to_string((int)(RESOLUTION));

        args[nargs++] = "pdftotext";
        args[nargs++] = "-q";

        switch (rect.type) {
        case box_type::BOX_RECTANGLE:
            args[nargs++] = "-r";
            args[nargs++] = r.c_str();
            args[nargs++] = "-x";
            args[nargs++] = x.c_str();
            args[nargs++] = "-y";
            args[nargs++] = y.c_str();
            args[nargs++] = "-W";
            args[nargs++] = w.c_str();
            args[nargs++] = "-H";
            args[nargs++] = h.c_str();
            // fall through
        case box_type::BOX_PAGE:
            args[nargs++] = "-f";
            args[nargs++] = p.c_str();
            args[nargs++] = "-l";
            args[nargs++] = p.c_str();
            break;
        case box_type::BOX_FILE:
            break;
        default:
            return "";
        }

        if (read_mode_options[static_cast<int>(rect.mode)]) args[nargs++] = read_mode_options[static_cast<int>(rect.mode)];
        args[nargs++] = m_filename.c_str();
        args[nargs++] = "-";
        
        subprocess process(args);
#ifdef _WIN32
        process.stream_out.set_clear_cr(true);
#endif
        std::string str = read_all(process.stream_out);
        string_trim(str);
        return str;
    } catch (const process_error &error) {
        throw pdf_error(error.message);
    }
}