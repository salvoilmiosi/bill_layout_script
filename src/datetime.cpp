#include "datetime.h"

#include "svstream.h"

#include <boost/locale.hpp>
#include <sstream>

namespace bl = boost::locale;

std::string datetime::format(const std::string &fmt_str) const {
    std::stringstream ss;
    ss << boost::locale::as::ftime(fmt_str) << m_date;
    return ss.str();
}

datetime datetime::parse_date(std::string_view str, const std::string &fmt_str) {
    isviewstream ss{str};
    datetime ret;
    ss >> boost::locale::as::ftime(fmt_str) >> ret.m_date;
    return ret;
}

datetime datetime::from_ymd(int year, int month, int day) {
    bl::date_time t{0};
    t.set(bl::period::year(), year);
    t.set(bl::period::month(), month - 1);
    t.set(bl::period::day(), day);
    return time_t(t.time());
}

void datetime::set_day(int day) {
    if (!is_valid()) return;
    bl::date_time t(m_date);
    t.set(bl::period::day(), day);
    m_date = t.time();
}

void datetime::set_to_last_month_day() {
    if (!is_valid()) return;
    bl::date_time t(m_date);
    t.set(bl::period::day(), t.maximum(bl::period::day()));
    m_date = t.time();
}

void datetime::add_years(int years) {
    if (!is_valid()) return;
    bl::date_time t(m_date);
    t += bl::period::year(years);
    m_date = t.time();
}

void datetime::add_months(int months) {
    if (!is_valid()) return;
    bl::date_time t(m_date);
    t += bl::period::month(months);
    m_date = t.time();
}

void datetime::add_days(int days) {
    if (!is_valid()) return;
    bl::date_time t(m_date);
    t += bl::period::day(days);
    m_date = t.time();
}