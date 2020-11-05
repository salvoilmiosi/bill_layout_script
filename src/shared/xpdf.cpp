#include "xpdf.h"

#include <filesystem>
#include <regex>

#include "pipe.h"
#include "utils.h"

static const char *modes[] = {"-raw", "-simple", "-table"};

std::string pdf_to_text(const pdf_info &info, const pdf_rect &in_rect) {
    if (!std::filesystem::exists(info.filename)) {
        throw xpdf_error(std::string("File \"") + info.filename + "\" does not exist");
    }

    switch (in_rect.type) {
    case BOX_RECTANGLE:
    {
        if (in_rect.page >= info.num_pages) {
            return "";
        }

        auto page_str = std::to_string(in_rect.page);
        auto marginl = std::to_string(info.width * in_rect.x);
        auto marginr = std::to_string(info.width * (1.f - in_rect.x - in_rect.w));
        auto margint = std::to_string(info.height * in_rect.y);
        auto marginb = std::to_string(info.height * (1.f - in_rect.y - in_rect.h));

        const char *args[] = {
            "pdftotext",
            "-f", page_str.c_str(), "-l", page_str.c_str(),
            "-marginl", marginl.c_str(), "-marginr", marginr.c_str(),
            "-margint", margint.c_str(), "-marginb", marginb.c_str(),
            modes[in_rect.mode],
            info.filename.c_str(), "-",
            nullptr
        };

        return string_trim(open_process(args)->read_all());
    }
    case BOX_PAGE:
    {
        if (in_rect.page >= info.num_pages) {
            return "";
        }

        auto page_str = std::to_string(in_rect.page);

        const char *args[] = {
            "pdftotext",
            "-f", page_str.c_str(), "-l", page_str.c_str(),
            modes[in_rect.mode],
            info.filename.c_str(), "-",
            nullptr
        };

        return string_trim(open_process(args)->read_all());
    }
    case BOX_WHOLE_FILE:
    {
        const char *args[] = {
            "pdftotext",
            modes[in_rect.mode],
            info.filename.c_str(), "-",
            nullptr
        };

        return string_trim(open_process(args)->read_all());
    }
    default:
        break;
    }
    return "";
}

pdf_info pdf_get_info(const std::string &pdf) {
    if (!std::filesystem::exists(pdf)) {
        throw xpdf_error("File \"" + pdf + "\" does not exist");
    }

    const char *args[] = {
        "pdfinfo", pdf.c_str(), nullptr
    };

    std::string output = open_process(args)->read_all();

    std::smatch match;

    pdf_info ret;
    ret.filename = pdf;
    
    if (std::regex_search(output, match, std::regex("Pages: +([0-9]+)"))) {
        ret.num_pages = std::stoi(match.str(1));
    }
    if (std::regex_search(output, match, std::regex("Page size: +([0-9\\.]+) x ([0-9\\.]+)"))) {
        ret.width = std::stof(match.str(1));
        ret.height = std::stof(match.str(2));
    }

    return ret;
}