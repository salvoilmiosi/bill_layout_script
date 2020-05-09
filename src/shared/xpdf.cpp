#include "xpdf.h"

#include <sstream>
#include <filesystem>
#include <cstdio>

namespace xpdf {

std::string pdf_to_text(const std::string &app_dir, const std::string &pdf, const pdf_info &info, const rect &in_rect) {
    if (!std::filesystem::exists(pdf)) {
        throw pipe_error("Il file non esiste");
    }

    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/xpdf/pdftotext", app_dir.c_str());

    char page_str[16];
    snprintf(page_str, 16, "%d", in_rect.page);

    char marginl[16], marginr[16], margint[16], marginb[16];
    snprintf(marginl, 16, "%f", info.width * in_rect.x);
    snprintf(marginr, 16, "%f", info.width * (1.f - in_rect.x - in_rect.w));
    snprintf(margint, 16, "%f", info.height * in_rect.y);
    snprintf(marginb, 16, "%f", info.height * (1.f - in_rect.y - in_rect.h));

    char pdf_str[FILENAME_MAX];
    snprintf(pdf_str, FILENAME_MAX, "%s", pdf.c_str());

    char *const args[] = {
        cmd_str,
        "-f", page_str, "-l", page_str,
        "-marginl", marginl, "-marginr", marginr,
        "-margint", margint, "-marginb", marginb,
        "-simple",
        pdf_str, "-",
        nullptr
    };

    return open_process(args)->read_all();
}

pdf_info pdf_get_info(const std::string &app_dir, const std::string &pdf) {
    if (!std::filesystem::exists(pdf)) {
        throw pipe_error("Il file non esiste");
    }

    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/xpdf/pdfinfo", app_dir.c_str());

    char pdf_str[FILENAME_MAX];
    snprintf(pdf_str, FILENAME_MAX, "%s", pdf.c_str());

    char *const args[] = {
        cmd_str,
        pdf_str, nullptr
    };

    std::string output = open_process(args)->read_all();
    std::istringstream iss(output);

    std::string line;

    pdf_info ret;
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

}