#include "xpdf.h"

#include <filesystem>
#include <regex>

#include "pipe.h"
#include "utils.h"

static const char *modes[] = {"-raw", "-layout"};

#define PDFTOTEXT_BIN "pdftotext"
#define PDFINFO_BIN "pdfinfo"

std::string pdf_to_text(const pdf_info &info, const pdf_rect &in_rect) {
    if (!std::filesystem::exists(info.filename)) {
        throw xpdf_error(std::string("File \"") + info.filename + "\" does not exist");
    }

    try {
        switch (in_rect.type) {
        case BOX_RECTANGLE:
        {
            if (in_rect.page >= info.num_pages) {
                return "";
            }

            auto page_str = std::to_string(in_rect.page);
            auto x = std::to_string((int)(info.width * in_rect.x));
            auto y = std::to_string((int)(info.height * in_rect.y));
            auto w = std::to_string((int)(info.width * in_rect.w));
            auto h = std::to_string((int)(info.height * in_rect.h));

            const char *args[] = {
                PDFTOTEXT_BIN,
                "-f", page_str.c_str(), "-l", page_str.c_str(),
                "-x", x.c_str(), "-y", y.c_str(),
                "-W", w.c_str(), "-H", h.c_str(),
                "-q", modes[in_rect.mode],
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
                PDFTOTEXT_BIN,
                "-f", page_str.c_str(), "-l", page_str.c_str(),
                "-q", modes[in_rect.mode],
                info.filename.c_str(), "-",
                nullptr
            };

            return string_trim(open_process(args)->read_all());
        }
        case BOX_FILE:
        {
            const char *args[] = {
                PDFTOTEXT_BIN,
                "-q", modes[in_rect.mode],
                info.filename.c_str(), "-",
                nullptr
            };

            return string_trim(open_process(args)->read_all());
        }
        default:
            return "";
        }
    } catch (const pipe_error &error) {
        throw xpdf_error(error.message);
    }
}

pdf_info pdf_get_info(const std::string &pdf) {
    if (!std::filesystem::exists(pdf)) {
        throw xpdf_error("File \"" + pdf + "\" does not exist");
    }

    const char *args[] = {
        PDFINFO_BIN, pdf.c_str(), nullptr
    };

    try {
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
    } catch (const pipe_error &error) {
        throw xpdf_error(error.message);
    }
}