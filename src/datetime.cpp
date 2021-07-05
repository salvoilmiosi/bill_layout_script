#include "datetime.h"

#include "svstream.h"

#include <boost/locale.hpp>
#include <sstream>

using namespace bls;

std::string datetime::format(const std::string &fmt_str) const {
    std::stringstream ss;
    ss << boost::locale::as::ftime(fmt_str) << m_date;
    return ss.str();
}

datetime datetime::parse_date(std::string_view str, const std::string &fmt_str) {
    util::isviewstream ss{str};
    datetime ret;
    ss >> boost::locale::as::ftime(fmt_str) >> ret.m_date;
    return ret;
}

datetime datetime::from_ymd(int year, int month, int day) {
    boost::locale::date_time t{0};
    t.set(boost::locale::period::year(), year);
    t.set(boost::locale::period::month(), month - 1);
    t.set(boost::locale::period::day(), day);
    return time_t(t.time());
}

void datetime::set_day(int day) {
    if (!is_valid()) return;
    boost::locale::date_time t(m_date);
    t.set(boost::locale::period::day(), day);
    m_date = t.time();
}

void datetime::set_to_last_month_day() {
    if (!is_valid()) return;
    boost::locale::date_time t(m_date);
    t.set(boost::locale::period::day(), t.maximum(boost::locale::period::day()));
    m_date = t.time();
}

void datetime::add_years(int years) {
    if (!is_valid()) return;
    boost::locale::date_time t(m_date);
    t += boost::locale::period::year(years);
    m_date = t.time();
}

void datetime::add_months(int months) {
    if (!is_valid()) return;
    boost::locale::date_time t(m_date);
    t += boost::locale::period::month(months);
    m_date = t.time();
}

void datetime::add_days(int days) {
    if (!is_valid()) return;
    boost::locale::date_time t(m_date);
    t += boost::locale::period::day(days);
    m_date = t.time();
}