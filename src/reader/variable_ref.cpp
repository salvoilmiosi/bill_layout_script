#include "reader.h"

void variable_ref::set_value(variable &&value, set_flags flags) {
    if (value.empty()) return;

    auto &var = parent.get_page(page_idx)[name];
    if (flags & SET_RESET) var.clear();
    while (var.size() < index + range_len) var.emplace_back();
    if (flags & SET_INCREASE) {
        for (size_t i=index; i < index + range_len; ++i) {
            var[i] += value;
        }
    } else if (range_len == 1) {
        var[index] = std::move(value);
    } else {
        for (size_t i=index; i < index + range_len; ++i) {
            var[i] = value;
        }
    }
}

const variable &variable_ref::get_value() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        auto &page = parent.get_page(page_idx);
        auto it = page.find(name);
        if (it != page.end()) {
            return it->second[index];
        }
    }
    return variable::null_var();
}

void variable_ref::clear() {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        parent.get_page(page_idx).erase(name);
    }
}

size_t variable_ref::size() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        auto &page = parent.get_page(page_idx);
        auto it = page.find(name);
        if (it != page.end()) {
            return it->second.size();
        }
    }
    return 0;
}

bool variable_ref::isset() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        auto &page = parent.get_page(page_idx);
        return page.contains(name);
    }
    return false;
}