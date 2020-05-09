#ifndef __RESULT_H__
#define __RESULT_H__

#include <string>
#include <vector>
#include <map>

#include "../shared/layout.h"

class result {
public:
    void read_box(const layout_box &box, const std::string &text);

    friend std::ostream & operator << (std::ostream &out, const result &res);

private:
    std::string evaluate(const std::string &value);
    std::string parse_function(const std::string &value);

    void top_function(const std::string &name, const std::string &value);
    void add_entry(const std::string &name, const std::string &value);

private:
    std::map<std::string, std::vector<std::string>> m_values;
};

#endif