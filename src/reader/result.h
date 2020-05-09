#ifndef __RESULT_H__
#define __RESULT_H__

#include <string>
#include <vector>
#include <map>

#include "../shared/layout.h"

class result {
public:
    void parse_values(const layout_box &box, const std::string &text);

    std::ostream & writeTo (std::ostream &out) const;

private:
    void parse_entry(const std::string &name, const std::string &value);

private:
    std::map<std::string, std::vector<std::string>> m_values;
};

inline std::ostream &operator << (std::ostream &out, const result &res) {
    return res.writeTo(out);
}

#endif