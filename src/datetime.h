#ifndef __DATETIME_H__
#define __DATETIME_H__

#ifndef HAVE_STD_CALENDAR
#include <wx/datetime.h>

class datetime {
private:
    wxDateTime m_date{time_t(0)};

public:
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

    static datetime parse_date(std::string_view str, const std::string &format_str) {
        datetime ret;
        wxString::const_iterator end;
        ret.m_date.ParseFormat(wxString(str.data(), str.size()), format_str, wxDateTime(time_t(0)), &end);
        return ret;
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

#else
#include <chrono>
#include <sstream>
#include <iomanip>
#include "svstream.h"

class datetime {
private:
    std::chrono::year_month_day m_date;

public:
    auto operator <=> (const datetime &rhs) const {
        return m_date <=> rhs.m_date;
    }

    bool operator == (const datetime &rhs) const {
        return m_date == rhs.m_date;
    }

    bool is_valid() const {
        return m_date.ok();
    }

    std::string to_string() const {
        return format("%F");
    }

    static datetime from_string(std::string_view str) {
        return parse_date(str, "%F");
    }

    std::string format(const std::string &fmt_str) const {
        time_t t = std::chrono::system_clock::to_time_t(std::chrono::sys_days{m_date});
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), fmt_str.c_str());
        return ss.str();
    }

    static datetime parse_date(std::string_view str, const std::string &fmt_str) {
        datetime ret;
        isviewstream iss{str};
        std::chrono::from_stream(iss, fmt_str.c_str(), ret.m_date);
        return ret;
    }

    void set_day(int day) {
        m_date = m_date.year() / m_date.month() / day;
    }

    void set_to_last_month_day() {
        m_date = m_date.year() / m_date.month() / std::chrono::last;
    }

    void add_months(int months) {
        m_date += std::chrono::months(months);
    }
};

#endif

#endif