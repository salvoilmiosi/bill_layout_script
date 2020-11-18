#include "xpdf.h"

#include <filesystem>
#include <regex>

#include "pipe.h"
#include "utils.h"

static const char *modes[] = {nullptr, "-layout", "-raw"};

std::string pdf_to_text(const pdf_info &info, const pdf_rect &in_rect) {
    if (in_rect.page > info.num_pages) return "";

    try {
        const char *args[30] = {0};
        size_t nargs = 0;

        auto p = std::to_string(in_rect.page);
        static const float RESOLUTION = 300;
        float resolution_factor = RESOLUTION / 72.f;
        auto x = std::to_string((int)(info.width * in_rect.x * resolution_factor));
        auto y = std::to_string((int)(info.height * in_rect.y * resolution_factor));
        auto w = std::to_string((int)(info.width * in_rect.w * resolution_factor));
        auto h = std::to_string((int)(info.height * in_rect.h * resolution_factor));
        auto r = std::to_string((int)(RESOLUTION));

        args[nargs++] = "pdftotext";
        args[nargs++] = "-q";

        switch (in_rect.type) {
        case BOX_RECTANGLE:
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
        case BOX_PAGE:
            args[nargs++] = "-f";
            args[nargs++] = p.c_str();
            args[nargs++] = "-l";
            args[nargs++] = p.c_str();
            break;
        case BOX_FILE:
            break;
        default:
            return "";
        }

        if (modes[in_rect.mode]) args[nargs++] = modes[in_rect.mode];
        args[nargs++] = info.filename.c_str();
        args[nargs++] = "-";
        
        return string_trim(open_process(args)->read_all());
    } catch (const pipe_error &error) {
        throw xpdf_error(error.message);
    }
}

pdf_info pdf_get_info(const std::string &pdf) {
    if (!std::filesystem::exists(pdf)) {
        throw xpdf_error("File \"" + pdf + "\" does not exist");
    }

    const char *args[] = {
        "pdfinfo", pdf.c_str(), nullptr
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