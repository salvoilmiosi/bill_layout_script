#ifndef __DATETIME_H__
#define __DATETIME_H__

#include <ctime>
#include <string>
#include <locale>

#include "utils.h"

namespace bls {

    class datetime {
    private:
        time_t m_date;

    public:
        explicit datetime(time_t date) : m_date{date} {}
        
        auto operator <=> (const datetime &rhs) const {
            return m_date <=> rhs.m_date;
        }

        bool operator == (const datetime &rhs) const {
            return m_date == rhs.m_date;
        }

        time_t data() const {
            return m_date;
        }

        std::string to_string() const;
        
        static datetime from_string(std::string_view str);

        std::string format(const std::locale &loc, const std::string &fmt_str) const;
        static datetime parse_date(const std::locale &loc, std::string_view str, const std::string &fmt_str);

        static datetime from_ymd(int year, int month, int day);

        void set_day(int day);
        void set_to_last_month_day();
        void add_years(int years);
        void add_months(int months);
        void add_days(int days);
    };

}

namespace util {
    template<> inline bls::datetime string_to(std::string_view str) {
        return bls::datetime::from_string(str);
    }
};

#endif