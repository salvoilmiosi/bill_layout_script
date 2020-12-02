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
    bool from_string(const std::string &str);
};

bool date_t::from_string(const std::string &str) {
    static std::regex expression("^(\\d{4})-(\\d{2})(-(\\d{2}))?$");
    try {
        std::smatch match;
        if (std::regex_search(str, match, expression)) {
            year = std::stoi(match.str(1));
            month = std::stoi(match.str(2));
            if (month > 12) {
                return false;
            }
            auto day = match.str(4);
            if (!day.empty()) day = std::stoi(day);
            return true;
        }
    } catch (std::invalid_argument &) {}
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

static bool search_date(wxDateTime &dt, const std::string &format, const std::string &value, std::string regex, int index) {
    if (regex.empty()) regex = "(%D)";

    std::string date_regex = format;
    string_replace(date_regex, ".", "\\.");
    string_replace_regex(date_regex, "%[aAbBh]", "\\w+");
    string_replace_regex(date_regex, "%[edmyY]", "\\d{2,4}");

    string_replace(regex, "%D", date_regex);

    wxString date_str = search_regex(regex, value, index);
    if (date_str.empty()) {
        return false;
    }

    wxString::const_iterator end;
    if (!dt.ParseFormat(date_str, format, &end)) {
        return false;
    }

    return true;
}

std::string parse_date(const std::string &format, const std::string &value, std::string regex, int index) {
    wxDateTime dt;
    if(!search_date(dt, format, value, regex, index)) {
        return "";
    }

    return date_t{dt.GetYear(), dt.GetMonth() + 1, dt.GetDay()}.to_string();
}

std::string parse_month(const std::string &format, const std::string &value, std::string regex, int index) {
    if (format.empty()) {
        date_t date;
        if (!date.from_string(value)) {
            return "";
        }
        date.day = 0;
        return date.to_string();
    }
    wxDateTime dt;
    if (!search_date(dt, format, value, regex, index)) {
        return "";
    }

    return date_t{dt.GetYear(), dt.GetMonth() + 1, 0}.to_string();
}

std::string date_month_add(const std::string &str, int num) {
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

std::string date_format(const std::string &str, const std::string &format) {
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