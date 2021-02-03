#include "utils.h"

#include <regex>
#include <wx/datetime.h>

static std::string date_fmt_to_regex(std::string_view fmt_string, std::string_view regex = "") {
    std::string date_regex = "\\b";
    for (auto it = fmt_string.begin(); it != fmt_string.end(); ++it) {
        if (*it == '.') {
            date_regex += "\\.";
        } else if (*it == '%') {
            ++it;
            switch (*it) {
            case 'h':
            case 'b':
            case 'B':
                date_regex += "\\w+";
                break;
            case 'd':
            case 'm':
            case 'y':
                date_regex += "\\d{2}";
                break;
            case 'Y':
                date_regex += "\\d{4}";
                break;
            case 'n':
                date_regex += '\n';
                break;
            case 't':
                date_regex += '\t';
                break;
            case '%':
                date_regex += '%';
                break;
            default:
                throw std::invalid_argument("Stringa formato data non valida");
            }
        } else {
            date_regex += *it;
        }
    }
    date_regex += "\\b";

    if (regex.empty()) {
        return "(" + date_regex +  ")";
    } else {
        std::string ret(regex);
        string_replace(ret, "\\D", date_regex);
        return ret;
    }
}

static wxDateTime string_to_date(std::string_view str) {
    static std::regex expression("(\\d{4})-(\\d{2})(-(\\d{2}))?");

    std::cmatch match;
    if (std::regex_match(str.begin(), str.end(), match, expression)) {
        int year = cstoi(match.str(1));
        int month = cstoi(match.str(2));
        int day = 1;
        auto day_str = match.str(4);
        if (!day_str.empty()) {
            day = cstoi(day_str);
        }
        if (month <= 0 || month > 12) {
            throw std::invalid_argument("Data non valida");
        }
        if (day <= 0 || day > wxDateTime::GetNumberOfDays(static_cast<wxDateTime::Month>(month - 1), year)) {
            throw std::invalid_argument("Data non valida");
        }
        return wxDateTime(day, static_cast<wxDateTime::Month>(month - 1), year);
    } else {
        throw std::invalid_argument("Data non valida");
    }
}

inline bool search_date(wxDateTime &dt, const std::string &format, std::string_view value, std::string_view regex, int index) {
    wxString::const_iterator end;
    return dt.ParseFormat(search_regex(date_fmt_to_regex(format, regex), value, index), format, wxDateTime(time_t(0)), &end);
}

std::string parse_date(const std::string &format, std::string_view value, std::string_view regex, int index) {
    wxDateTime dt;
    if(!search_date(dt, format, value, regex, index)) {
        return "";
    }

    return dt.Format("%Y-%m-%d").ToStdString();
}

std::string parse_month(const std::string &format, std::string_view value, std::string_view regex, int index) {
    wxDateTime dt;
    if (format.empty()) {
        try {
            dt = string_to_date(value);
        } catch (const std::invalid_argument &) {
            return "";
        }
    } else {
        if (!search_date(dt, format, value, regex, index)) {
            return "";
        }
    }
    return dt.Format("%Y-%m").ToStdString();
}

std::string date_month_add(std::string_view str, int num) {
    try {
        wxDateTime dt = string_to_date(str);
        dt += wxDateSpan(0, num);

        return dt.Format("%Y-%m").ToStdString();
    } catch (const std::invalid_argument &) {
        return "";
    }
}

std::string date_format(std::string_view str, const std::string &format) {
    try {
        wxDateTime dt = string_to_date(str);

        return dt.Format(format).ToStdString();
    } catch (const std::invalid_argument &) {
        return "";
    }
}