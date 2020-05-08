#include "xpdf.h"

#include <sstream>
#include <filesystem>

namespace xpdf {

std::string pdf_to_text(const std::string &app_dir, const std::string &pdf, const pdf_info &info, const rect &in_rect) {
    if (!std::filesystem::exists(pdf)) {
        throw pipe_error("Il file non esiste");
    }
    std::string args;
    args += "-f " + std::to_string(in_rect.page) + " ";
    args += "-l " + std::to_string(in_rect.page) + " ";
    args += "-marginl " + std::to_string(info.width * in_rect.x) + " ";
    args += "-marginr " + std::to_string(info.width * (1.f - in_rect.x - in_rect.w)) + " ";
    args += "-margint " + std::to_string(info.height * in_rect.y) + " ";
    args += "-marginb " + std::to_string(info.height * (1.f - in_rect.y - in_rect.h)) + " ";
    args += "-simple ";
    args += '"' + pdf + "\" -";

    return open_process(app_dir + "/xpdf/pdftotext.exe", args)->read_all();
}

pdf_info pdf_get_info(const std::string &app_dir, const std::string &pdf) {
    if (!std::filesystem::exists(pdf)) {
        throw pipe_error("Il file non esiste");
    }
    std::string output = open_process(app_dir + "/xpdf/pdfinfo.exe", '"' + pdf + '"')->read_all();
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