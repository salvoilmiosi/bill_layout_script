#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <vector>
#include <map>

#include <json/json.h>

#include "../shared/layout.h"

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line) : message(message), line(line) {}
};

static constexpr const char *TYPES[] = {
    "undefined",
    "string",
    "number",
};

enum value_type {
    VALUE_UNDEFINED,
    VALUE_STRING,
    VALUE_NUMBER,
};
class var_value {
public:
    var_value();
    var_value(const std::string &value, value_type type = VALUE_STRING) : m_value(value), m_type(type) {}
    var_value(float value) : var_value(std::to_string(value), VALUE_NUMBER) {}

    var_value &operator = (const var_value &other);
    bool operator == (const var_value &other) const;
    bool operator != (const var_value &other) const;

    const std::string &asString() const;
    float asNumber() const;
    int precision() const;

    value_type type() const;

private:
    std::string m_value{};
    value_type m_type = VALUE_UNDEFINED;
};

class parser {
public:
    void read_box(const layout_box &box, const std::string &text);
    const std::vector<var_value> &get_value(const std::string &name);

    friend std::ostream & operator << (std::ostream &out, const parser &res);

private:
    var_value evaluate(const std::string &value);
    var_value parse_function(const std::string &value);
    var_value parse_number(const std::string &value);

    void search_value(const std::string &regex, const std::string &name, const std::string &value);
    void top_function(const std::string &name, const std::string &value);
    void add_entry(const std::string &name, const std::string &value);

private:
    std::map<std::string, std::vector<var_value>> m_values;
};

#endif