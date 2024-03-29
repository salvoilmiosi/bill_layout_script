#include "datetime.h"

#include <boost/locale.hpp>
#include <sstream>

using namespace bls;

static const std::string ISO_FORMAT = "%Y-%m-%d";

static const std::locale calendar_locale = [] {
    boost::locale::generator gen;
    gen.categories(
        boost::locale::category_t::calendar |
        boost::locale::category_t::formatting |
        boost::locale::category_t::parsing
    );
    return gen("");
}();

std::string datetime::format(const std::locale &loc, const std::string &fmt_str) const {
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::locale::as::ftime(fmt_str) << m_date;
    return ss.str();
}

datetime datetime::parse_date(const std::locale &loc, std::string_view str, const std::string &fmt_str) {
    util::isviewstream ss{str};
    ss.imbue(loc);
    time_t ret;
    ss >> boost::locale::as::ftime(fmt_str) >> ret;
    if (ss.fail()) {
        throw conversion_error(intl::translate("CANT_PARSE_DATE", str));
    }
    return datetime(ret);
}

std::string datetime::to_string() const {
    return format(calendar_locale, ISO_FORMAT);
}

datetime datetime::from_string(std::string_view str) {
    return parse_date(calendar_locale, str, ISO_FORMAT);
}

datetime datetime::from_ymd(int year, int month, int day) {
    boost::locale::date_time t(0, boost::locale::calendar(calendar_locale));
    t.set(boost::locale::period::year(), year);
    t.set(boost::locale::period::month(), month - 1);
    t.set(boost::locale::period::day(), day);
    return datetime(t.time());
}

void datetime::set_day(int day) {
    boost::locale::date_time t(m_date, boost::locale::calendar(calendar_locale));
    t.set(boost::locale::period::day(), day);
    m_date = t.time();
}

void datetime::set_to_last_month_day() {
    boost::locale::date_time t(m_date, boost::locale::calendar(calendar_locale));
    t.set(boost::locale::period::day(), t.maximum(boost::locale::period::day()));
    m_date = t.time();
}

void datetime::add_years(int years) {
    boost::locale::date_time t(m_date, boost::locale::calendar(calendar_locale));
    t += boost::locale::period::year(years);
    m_date = t.time();
}

void datetime::add_months(int months) {
    boost::locale::date_time t(m_date, boost::locale::calendar(calendar_locale));
    t += boost::locale::period::month(months);
    m_date = t.time();
}

void datetime::add_days(int days) {
    boost::locale::date_time t(m_date, boost::locale::calendar(calendar_locale));
    t += boost::locale::period::day(days);
    m_date = t.time();
}