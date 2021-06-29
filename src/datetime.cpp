#include "datetime.h"

#include "svstream.h"
#include "intl.h"

#include <boost/locale.hpp>
#include <sstream>

static inline std::tm time_t_to_tm(time_t t) { return *std::localtime(&t); }
static inline time_t tm_to_time_t(std::tm t) { return std::mktime(&t); }

std::string datetime::format(const std::string &fmt_str) const {
    std::stringstream ss;
    ss << boost::locale::as::ftime(fmt_str) << m_date;
    return intl::to_utf8(ss.str());
}

datetime datetime::parse_date(std::string_view str, const std::string &fmt_str) {
    std::stringstream ss{intl::from_utf8(str)};
    datetime ret;
    ss >> boost::locale::as::ftime(fmt_str) >> ret.m_date;
    return ret;
}

void datetime::set_day(int day) {
    auto date = time_t_to_tm(m_date);
    date.tm_mday = day;
    m_date = tm_to_time_t(date);
}

void datetime::set_to_last_month_day() {
    auto date = time_t_to_tm(m_date);
    date.tm_mday = [month = date.tm_mon, year = date.tm_year + 1900]() {
        switch (month) {
        case 0: case 2: case 4: case 6: case 7: case 9: case 11:
            return 31;
        case 3: case 5: case 8: case 10:
            return 30;
        default:
            if (year % 4 == 0) {
                if (year % 100 == 0) {
                    if (year % 400 == 0)
                        return 29;
                    return 28;
                }
                return 29;
            }
            return 28;
        }
    }();
    m_date = tm_to_time_t(date);
}

void datetime::add_years(int years) {
    auto date = time_t_to_tm(m_date);
    date.tm_year += years;
    m_date = tm_to_time_t(date);
}

void datetime::add_months(int months) {
    auto date = time_t_to_tm(m_date);
    date.tm_mon += months;
    m_date = tm_to_time_t(date);
}

void datetime::add_weeks(int weeks) {
    add_days(7 * weeks);
}

void datetime::add_days(int days) {
    auto date = time_t_to_tm(m_date);
    date.tm_mday += days;
    m_date = tm_to_time_t(date);
}