#include "utils.h"

#include <regex>
#include <wx/datetime.h>

static bool search_date(wxDateTime &dt, std::string_view value, const std::string &format, std::string regex, int index) {
    std::string date_regex = "\\b";
    for (auto it = format.begin(); it != format.end(); ++it) {
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
                date_regex += "\\d{1,2}";
                break;
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
        regex = "(" + date_regex +  ")";
    } else {
        string_replace(regex, "\\D", date_regex);
    }

    wxString::const_iterator end;
    return dt.ParseFormat(search_regex(regex, value, index), format, wxDateTime(time_t(0)), &end);
}

time_t parse_date(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        return dt.GetTicks();
    }
    return 0;
}

time_t parse_month(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        dt.SetDay(1);
        return dt.GetTicks();
    }
    return 0;
}

time_t date_month_add(time_t date, int num) {
    wxDateTime dt(date);
    dt += wxDateSpan(0, num);

    return dt.GetTicks();
}

time_t date_last_day(time_t date) {
    wxDateTime dt(date);
    dt.SetToLastMonthDay(dt.GetMonth(), dt.GetYear());
    return dt.GetTicks();
}

bool date_is_between(time_t date, time_t date_begin, time_t date_end) {
    return wxDateTime(date).IsBetween(wxDateTime(date_begin), wxDateTime(date_end));
}

std::string date_format(time_t date, const std::string &format) {
    return wxDateTime(date).Format(format).ToStdString();
}