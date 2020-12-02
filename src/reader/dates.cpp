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
    static bool from_string(const std::string &str, date_t &date);
};

bool date_t::from_string(const std::string &str, date_t &date) {
    static std::regex expression("^(\\d{4})-(\\d{2})(-(\\d{2}))?$");
    try {
        std::smatch match;
        if (std::regex_search(str, match, expression)) {
            date.year = std::stoi(match.str(1));
            date.month = std::stoi(match.str(2));
            if (date.month > 12) {
                return false;
            }
            auto day = match.str(4);
            if (!day.empty()) date.day = std::stoi(day);
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

static wxDateTime search_date(const std::string &format, const std::string &value, std::string regex, int index) {
    if (regex.empty()) regex = "(%D)";

    std::string date_regex = format;
    string_replace(date_regex, ".", "\\.");
    string_replace(date_regex, "%Y", "\\d{4}");
    date_regex = string_replace_regex("%[bB]", date_regex, "\\w+");
    date_regex = string_replace_regex("%[dmy]", date_regex, "\\d{2}");

    string_replace(regex, "%D", date_regex);

    wxString date_str = search_regex(regex, value, index);
    if (date_str.empty()) {
        return wxDateTime();
    }

    wxDateTime dt;
    wxString::const_iterator end;
    if (!dt.ParseFormat(date_str, format, &end)) {
        return wxDateTime();
    }

    return dt;
}

std::string parse_date(const std::string &format, const std::string &value, std::string regex, int index) {
    wxDateTime dt = search_date(format, value, regex, index);

    if (!dt.IsValid()) {
        return "";
    }

    date_t date;
    date.day = dt.GetDay();
    date.month = dt.GetMonth() + 1;
    date.year = dt.GetYear();
    return date.to_string();
}

std::string parse_month(const std::string &format, const std::string &value, std::string regex, int index) {
    wxDateTime dt = search_date(format, value, regex, index);

    if (!dt.IsValid()) {
        return "";
    }

    date_t date;
    date.month = dt.GetMonth() + 1;
    date.year = dt.GetYear();
    return date.to_string();
}

std::string date_month_add(const std::string &str, int num) {
    date_t date;
    if (!date_t::from_string(str, date)) {
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
    if (!date_t::from_string(str, date)) {
        return "";
    }

    wxDateTime dt(std::max(1, date.day), static_cast<wxDateTime::Month>(date.month - 1), date.year);
    if (!dt.IsValid()) {
        return "";
    } else {
        return dt.Format(format).ToStdString();
    }
}