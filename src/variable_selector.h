#ifndef __VARIABLE_SELECTOR_H__
#define __VARIABLE_SELECTOR_H__

#include <string>
#include <vector>
#include <map>

#include "variable.h"

namespace bls {

    using variable_map = std::map<std::string, variable, std::less<>>;

    class variable_selector {
    private:
        variable_map &m_current_map;
        std::string m_name;

        std::vector<size_t> m_indices;

    private:
        variable *get_variable() {
            variable *var = &m_current_map[m_name];
            for (auto index : m_indices) {
                if (!var->is_array()) *var = variable_array();
                auto &arr = var->as_array();
                if (index == -1) {
                    index = arr.size();
                }
                arr.resize(std::max(arr.size(), index + 1));
                var = arr.data() + index;
            }
            return var;
        }

    public:
        variable_selector(variable_map &map, std::string name)
            : m_current_map(map)
            , m_name(std::move(name)) {}

        void add_index(size_t index) {
            m_indices.push_back(index);
        }

        void add_append() {
            m_indices.push_back(-1);
        }

    public:
        void fwd_value(variable &&value) {
            *get_variable() = std::move(value);
        }

        void set_value(variable &&value) {
            if (!value.is_empty()) get_variable()->assign(std::move(value));
        }

        void force_value(variable &&value) {
            get_variable()->assign(std::move(value));
        }

        void inc_value(const variable &value) {
            if (!value.is_empty()) *get_variable() += value;
        }

        void dec_value(const variable &value) {
            if (!value.is_empty()) *get_variable() -= value;
        }

        void clear_value() {
            m_current_map.erase(m_name);
        }

        variable get_value() const {
            auto it = m_current_map.find(m_name);
            if (it == m_current_map.end()) return variable();
            return it->second.as_pointer();
        }
    };
}

#endif