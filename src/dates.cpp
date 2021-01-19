#include "functions.h"

#include "utils.h"

#include <regex>
#include <fmt/core.h>
#include <wx/datetime.h>

struct date_t {
    int year = 0;
    int month = 0;
    int day = 0;
    
    std::string to_string();
    bool from_string(std::string_view str);
};

bool date_t::from_string(std::string_view str) {
    static std::regex expression("^(\\d{4})-(\\d{2})(-(\\d{2}))?$");
    try {
        std::cmatch match;
        if (std::regex_search(str.begin(), str.end(), match, expression)) {
            year = cstoi(match.str(1));
            month = cstoi(match.str(2));
            if (month > 12) {
                return false;
            }
            auto day = match.str(4);
            if (!day.empty()) day = cstoi(day);
            return true;
        }
    } catch (const std::invalid_argument &) {}
    return false;
}

std::string date_t::to_string() {
    if (!year || !month) {
        return "";
    } else if (!day) {
        return fmt::format("{:04}-{:02}", year, month);
    } else {
        return fmt::format("{:04}-{:02}-{:02}", year, month, day);
    }
}

static bool search_date(wxDateTime &dt, const std::string &format, std::string_view value, const std::string &regex, int index) {
    wxString::const_iterator end;
    return dt.ParseFormat(search_regex(regex, value, index), format, &end);
}

std::string parse_date(const std::string &format, std::string_view value, const std::string &regex, int index) {
    wxDateTime dt(time_t(0));
    if(!search_date(dt, format, value, regex, index)) {
        return "";
    }

    return date_t{dt.GetYear(), dt.GetMonth() + 1, dt.GetDay()}.to_string();
}

std::string parse_month(const std::string &format, std::string_view value, const std::string &regex, int index) {
    if (format.empty()) {
        date_t date;
        if (!date.from_string(value)) {
            return "";
        }
        date.day = 0;
        return date.to_string();
    } else {
        wxDateTime dt(time_t(0));
        if (!search_date(dt, format, value, regex, index)) {
            return "";
        }

        return date_t{dt.GetYear(), dt.GetMonth() + 1, 0}.to_string();
    }
}

std::string date_month_add(std::string_view str, int num) {
    date_t date;
    if (!date.from_string(str)) {
        return "";
    }

    date.month += num;
    while (date.month > 12) {
        date.month -= 12;
        ++date.year;
    }
    return date.to_string();
}

std::string date_format(std::string_view str, const std::string &format) {
    date_t date;
    if (!date.from_string(str)) {
        return "";
    }

    wxDateTime dt(std::max(1, date.day), static_cast<wxDateTime::Month>(date.month - 1), date.year);
    if (!dt.IsValid()) {
        return "";
    } else {
        return dt.Format(format).ToStdString();
    }
}