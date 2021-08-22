#ifndef __VARIABLE_SELECTOR_H__
#define __VARIABLE_SELECTOR_H__

#include <string>
#include <vector>
#include <map>

#include "variable.h"
#include "bytecode.h"

namespace bls {

    using variable_map = util::string_map<variable>;

    struct selector_index {
        size_t index = 0;
        size_t size = 1;
        bool append:1 = false;
        bool each:1 = false;
    };

    class variable_selector {
    private:
        variable_map &m_current_map;
        std::string m_name;

        std::vector<selector_index> m_indices;

    private:
        std::span<variable> get_variable_span() {
            variable *var = &m_current_map[m_name];
            size_t len = 1;
            for (auto index : m_indices) {
                if (!var->is_array()) *var = variable_array();
                auto &arr = var->as_array();
                if (index.append) {
                    index.index = arr.size();
                } else if (index.each) {
                    index.size = arr.size();
                }
                arr.resize(std::max(arr.size(), index.index + index.size));
                var = arr.data() + index.index;
                len = index.size;
            }
            return std::span(var, len);
        }

    public:
        variable_selector(variable_map &map, std::string name)
            : m_current_map(map)
            , m_name(std::move(name)) {}

        void add_index(size_t index) {
            m_indices.emplace_back(index, 1);
        }

        void set_size(size_t size) {
            m_indices.back().size = size;
        }

        void add_append() {
            m_indices.emplace_back(0, 1, true);
        }

        void add_each() {
            m_indices.emplace_back(0, 1, false, true);
        }

    public:
        void fwd_value(variable &&value) {
            std::span<variable> vars = get_variable_span();
            std::for_each_n(vars.begin(), vars.size() - 1, [&](variable &var) {
                var = value;
            });
            vars.back() = std::move(value);
        }

        void set_value(variable &&value) {
            if (!value.is_null()) force_value(std::move(value));
        }

        void force_value(variable &&value) {
            std::span<variable> vars = get_variable_span();
            std::for_each_n(vars.begin(), vars.size() - 1, [&](variable &var) {
                var.assign(value);
            });
            vars.back().assign(std::move(value));
        }

        void inc_value(const variable &value) {
            if (value.is_null()) return;
            for (variable &var : get_variable_span()) {
                var += value;
            }
        }

        void dec_value(const variable &value) {
            if (value.is_null()) return;
            for (variable &var : get_variable_span()) {
                var -= value;
            }
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