#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>

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
class variable {
public:
    variable();
    variable(const std::string &value, value_type type = VALUE_STRING) : m_value(value), m_type(type) {}
    variable(float value) : variable(std::to_string(value), VALUE_NUMBER) {}

    variable &operator = (const variable &other);
    bool operator == (const variable &other) const;
    bool operator != (const variable &other) const;

    const std::string &asString() const;
    float asNumber() const;
    int precision() const;

    value_type type() const;

private:
    std::string m_value{};
    value_type m_type = VALUE_UNDEFINED;
};

#endif