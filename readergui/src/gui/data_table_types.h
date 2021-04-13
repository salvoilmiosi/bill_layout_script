#ifndef __DATA_TABLE_TYPES_H__
#define __DATA_TABLE_TYPES_H__

#include <wx/variant.h>
#include <wx/datetime.h>

#include <sstream>

#include "fixed_point.h"

class ColumnValueVariantData : public wxVariantData {
public:
    static constexpr const char *CLASS_NAME = "ColumnValueVariantData";

    virtual wxString GetType() const override { return CLASS_NAME; }

    virtual int CompareTo (const ColumnValueVariantData &other) const = 0;
};

template<typename T, typename Formatter> class OptionalValueColumn : public ColumnValueVariantData {
public:
    std::optional<T> m_value;

    OptionalValueColumn(std::optional<T> value) : m_value(std::move(value)) {}

    virtual bool Eq(wxVariantData &other) const override {
        const auto &obj = dynamic_cast<const OptionalValueColumn &>(other);
        return m_value == obj.m_value;
    }

    virtual int CompareTo(const ColumnValueVariantData &other) const override {
        const auto &obj = dynamic_cast<const OptionalValueColumn &>(other);
        auto less = [](const OptionalValueColumn &a, const OptionalValueColumn &b) {
            if (a.m_value) {
                if (b.m_value) {
                    return a.m_value.value() < b.m_value.value();
                } else {
                    return true;
                }
            } else {
                return false;
            }
        };
        if (less(*this, obj)) return -1;
        if (less(obj, *this)) return 1;
        return 0;
    }

    virtual bool Write(wxString &out) const override {
        if (m_value) {
            out = Formatter{}(m_value.value());
        } else {
            out.Clear();
        }
        return true;
    }
};

#endif