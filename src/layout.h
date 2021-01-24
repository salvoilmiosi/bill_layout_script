#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"

#include <memory>
#include <vector>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fmt/format.h>

#include "intl.h"

#define RESIZE_TOP      1 << 0
#define RESIZE_BOTTOM   1 << 1
#define RESIZE_LEFT     1 << 2
#define RESIZE_RIGHT    1 << 4

struct layout_box : public pdf_rect {
    bool selected = false;
    std::string name;
    std::string script;
    std::string spacers;
    std::string goto_label;
};

struct layout_error : std::runtime_error {
    layout_error(const std::string &message) : std::runtime_error(message) {}
};

using box_ptr = std::shared_ptr<layout_box>;

std::ostream &operator << (std::ostream &out, const class bill_layout_script &obj);
std::istream &operator >> (std::istream &in, class bill_layout_script &obj);

class bill_layout_script {
public:
    std::vector<box_ptr> m_boxes;
    std::string language_code;

public:
    bill_layout_script() {
        language_code = intl::system_language();
    }

    void clear() {
        m_boxes.clear();
        m_filename.clear();
        language_code = intl::system_language();
    }

    const std::filesystem::path &filename() const {
        return m_filename;
    }

    void set_filename(const std::filesystem::path &value) {
        m_filename = std::filesystem::canonical(value);
    }

    static bill_layout_script from_file(const std::filesystem::path &filename) {
        bill_layout_script ret;
        std::ifstream ifs(filename);
        if (!ifs) {
            throw layout_error(fmt::format("Impossibile aprire il file {}", filename.string()));
        }
        ifs >> ret;
        ret.set_filename(filename);
        return ret;
    }

    bool save_file(const std::filesystem::path &filename) {
        std::ofstream ofs(filename);
        if (ofs << *this) {
            set_filename(filename);
            return true;
        }
        return false;
    }
    
private:
    std::filesystem::path m_filename;
};

#endif