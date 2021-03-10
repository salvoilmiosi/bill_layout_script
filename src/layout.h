#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"
#include "intl.h"

#include <list>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include <fmt/format.h>

struct layout_box : public pdf_rect {
    std::string name;
    std::string script;
    std::string spacers;
    std::string goto_label;
};

struct layout_error : std::runtime_error {
    layout_error(auto &&message) : std::runtime_error(std::forward<decltype(message)>(message)) {}
};

template<template<typename> typename Container>
class bill_layout_script : public Container<layout_box> {
public:
    using base = Container<layout_box>;
    intl::language language_code{};

public:
    bill_layout_script() = default;

    void clear() {
        base::clear();
        language_code = {};
    }

    auto get_box_iterator(const layout_box *ptr) {
        return std::ranges::find_if(*this, [&](const auto &box) {
            return &box == ptr;
        });
    };
    
    template<typename ... Ts>
    auto insert_after(base::iterator pos, Ts && ... args) {
        if (pos != base::end()) ++pos;
        return base::emplace(pos, std::forward<Ts>(args) ...);
    }

    static bill_layout_script from_file(const std::filesystem::path &filename) {
        bill_layout_script ret;
        std::ifstream ifs(filename);
        if (!ifs) {
            throw layout_error(fmt::format("Impossibile aprire il file {}", filename.string()));
        }
        ifs >> ret;
        return ret;
    }

    void save_file(const std::filesystem::path &filename) {
        std::ofstream ofs(filename);
        if (!ofs) {
            throw layout_error(fmt::format("Impossibile salvare il file {}", filename.string()));
        }
        ofs << *this;
    }
};

using box_list = bill_layout_script<std::list>;
using box_vector = bill_layout_script<std::vector>;

#include "layout.tcc"

#endif