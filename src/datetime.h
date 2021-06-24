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
        std::ostringstream oss;
        oss << m_date;
        return oss.str();
    }

    static datetime from_string(std::string_view str) {
        return parse_date(str, "%Y-%m-%d");
    }

    std::string format(std::string_view fmt_str) const {
        return std::format(std::format("{{0:{}}}", fmt_str), m_date);
    }

    static datetime parse_date(std::string_view str, const std::string &format_str) {
        datetime ret;
        std::istringstream iss;
        iss.rdbuf()->pubsetbuf(const_cast<char *>(str.data()), str.size());
        std::chrono::from_stream(iss, format_str.c_str(), ret.m_date);
        return ret;
    }

    void set_day(int day) {
        m_date = std::chrono::year_month_day(m_date.year(), m_date.month(), std::chrono::day(day));
    }

    void set_to_last_month_day() {
        m_date = std::chrono::year_month_day_last(m_date.year(), std::chrono::month_day_last(m_date.month()));
    }

    void add_months(int months) {
        m_date += std::chrono::months(months);
    }
};

#endif

#endif