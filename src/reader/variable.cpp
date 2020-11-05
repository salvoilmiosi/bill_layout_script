#include "variable.h"
#include "reader.h"

variable::operator bool() const {
    switch(m_type) {
    case VALUE_STRING:
        return !m_value.empty();
    case VALUE_NUMBER:
        return number() != fixed_point(0);
    default:
        return false;
    }
}

bool variable::operator == (const variable &other) const {
    switch(other.m_type) {
    case VALUE_STRING:
        return m_value == other.m_value;
    case VALUE_NUMBER:
        return number() == other.number();
    default:
        return false;
    }
}

bool variable::operator != (const variable &other) const {
    return !(*this == other);
}

bool variable::operator < (const variable &other) const {
    return number() < other.number();
}

bool variable::operator > (const variable &other) const {
    return number() > other.number();
}

variable variable::operator + (const variable &other) const {
    return number() + other.number();
}

variable variable::operator - (const variable &other) const {
    return number() - other.number();
}

variable variable::operator * (const variable &other) const {
    return number() * other.number();
}

variable variable::operator / (const variable &other) const {
    return number() / other.number();
}

variable &variable_ref::operator *() {
    if (flags & FLAGS_GLOBAL) {
        return parent.m_globals[name];
    }

    while (parent.m_values.size() <= pageidx) parent.m_values.emplace_back();

    auto &var = parent.m_values[pageidx][name];
    if (flags & FLAGS_CLEAR) {
        var.clear();
        index = 0;
    } else if (flags & FLAGS_APPEND) {
        index = var.size();
    }
    while (var.size() <= index) var.emplace_back();

    return var[index];
}

void variable_ref::clear() {
    if (parent.m_values.size() <= pageidx) return;

    if (flags & FLAGS_GLOBAL) {
        parent.m_globals.erase(name);
    } else {
        parent.m_values[pageidx].erase(name);
    }
}

size_t variable_ref::size() const {
    if (parent.m_values.size() <= pageidx) return 0;

    if (flags & FLAGS_GLOBAL) {
        return parent.m_globals.find(name) != parent.m_globals.end();
    }

    auto &page = parent.m_values[pageidx];
    auto it = page.find(name);
    if (it == page.end()) return 0;
    return it->second.size();
}

bool variable_ref::isset() const {
    if (parent.m_values.size() <= pageidx) return false;

    if (flags & FLAGS_GLOBAL) {
        return parent.m_globals.find(name) != parent.m_globals.end();
    }

    auto &page = parent.m_values[pageidx];
    auto it = page.find(name);
    if (it == page.end()) return false;

    if (index < 0 || index >= it->second.size()) return false;

    return true;
}