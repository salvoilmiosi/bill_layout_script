#include "xpdf.h"

#include <sstream>
#include <filesystem>
#include <cstdio>

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

        char page_str[16];
        snprintf(page_str, 16, "%d", in_rect.page);

        char marginl[16], marginr[16], margint[16], marginb[16];
        snprintf(marginl, 16, "%f", info.width * in_rect.x);
        snprintf(marginr, 16, "%f", info.width * (1.f - in_rect.x - in_rect.w));
        snprintf(margint, 16, "%f", info.height * in_rect.y);
        snprintf(marginb, 16, "%f", info.height * (1.f - in_rect.y - in_rect.h));

        const char *args[] = {
            "pdftotext",
            "-f", page_str, "-l", page_str,
            "-marginl", marginl, "-marginr", marginr,
            "-margint", margint, "-marginb", marginb,
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

        char page_str[16];
        snprintf(page_str, 16, "%d", in_rect.page);

        const char *args[] = {
            "pdftotext",
            "-f", page_str, "-l", page_str,
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
    std::istringstream iss(output);

    std::string line;

    pdf_info ret;
    ret.filename = pdf;
    while (std::getline(iss, line)) {
        size_t colon_pos = line.find_first_of(':');
        std::string token = line.substr(0, colon_pos);
        if (token == "Pages") {
            size_t start_pos = line.find_first_not_of(' ', colon_pos + 1);
            token = line.substr(start_pos);
            ret.num_pages = std::stoi(token);
        } else if (token == "Page size") {
            size_t start_pos = line.find_first_not_of(' ', colon_pos + 1);
            size_t end_pos = line.find_first_of(' ', start_pos);
            token = line.substr(start_pos, end_pos);
            ret.width = std::stof(token);
            start_pos = end_pos + 3;
            end_pos = line.find_first_of(' ', start_pos);
            token = line.substr(start_pos, end_pos);
            ret.height = std::stof(token);
        }
    }

    return ret;
}