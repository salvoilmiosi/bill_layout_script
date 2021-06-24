#ifndef __DATETIME_H__
#define __DATETIME_H__

#ifndef HAVE_STD_CALENDAR
#include <wx/datetime.h>

class datetime {
private:
    wxDateTime m_date;

public:
    datetime() : m_date(time_t(0)) {}

    auto operator <=> (const datetime &rhs) const {
        return m_date.GetValue().GetValue() <=> rhs.m_date.GetValue().GetValue();
    }

    bool operator == (const datetime &rhs) const {
        return m_date == rhs.m_date;
    }

    bool is_valid() const {
        return m_date.GetTicks() != 0;
    }

    std::string to_string() const {
        return m_date.FormatISODate().ToStdString();
    }

    static datetime from_string(const std::string &str) {
        datetime ret;
        ret.m_date.ParseISODate(str);
        return ret;
    }

    std::string format(const std::string &format_str) const {
        return m_date.Format(format_str).ToStdString();
    }

    static bool parse_date(datetime &out, const std::string &str, const std::string &format_str) {
        wxString::const_iterator end;
        return out.m_date.ParseFormat(str, format_str, wxDateTime(time_t(0)), &end);
    }

    void set_day(int day) {
        m_date.SetDay(day);
    }

    void set_to_last_month_day() {
        m_date.SetToLastMonthDay(m_date.GetMonth(), m_date.GetYear());
    }

    void add_months(int months) {
        m_date.Add(wxDateSpan(0, months));
    }
};

#endif

#endif