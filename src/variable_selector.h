#ifndef __VARIABLE_SELECTOR_H__
#define __VARIABLE_SELECTOR_H__

#include <string>
#include <vector>
#include <map>

#include "variable.h"
#include "bytecode.h"

namespace bls {

    using variable_map = util::string_map<variable>;

    class variable_selector {
    private:
        variable_map &m_current_map;
        std::string m_name;

        std::vector<small_int> m_indices;
        small_int m_size = 1;

    private:
        variable_ptr get_variable_const() const {
            auto it = m_current_map.find(m_name);
            if (it == m_current_map.end()) return nullptr;
            variable_ptr var = &it->second;
            for (small_int i : m_indices) {
                if (!var->is_array()) return nullptr;
                const auto &arr = var->as_array();
                if (i >= arr.size()) return nullptr;
                var = arr.data() + i;
            }
            return var;
        }

        variable *get_variable() {
            variable *var = &m_current_map[m_name];
            for (int i=0; i<m_indices.size(); ++i) {
                if (!var->is_array()) *var = variable_array();
                auto &arr = var->as_array();
                arr.resize(std::max(arr.size(), size_t(m_indices[i] + (i + 1 == m_indices.size() ? m_size : 1))));
                var = arr.data() + m_indices[i];
            }
            return var;
        }

    public:
        variable_selector(std::string name, variable_map &map)
            : m_current_map(map)
            , m_name(std::move(name)) {}

        void add_index(small_int index) {
            m_indices.push_back(index);
        }

        void set_size(small_int size) {
            m_size = size;
        }

        void add_append() {
            if (auto var = get_variable_const(); var && var->is_array()) {
                m_indices.push_back(var->as_array().size());
            } else {
                m_indices.push_back(0);
            }
        }

        void add_each() {
            if (auto var = get_variable_const(); var && var->is_array()) {
                m_indices.push_back(0);
                m_size = var->as_array().size();
            } else {
                m_size = 0;
            }
        }

    public:
        void set_value(variable &&value) {
            if (!value.is_null()) force_value(std::move(value));
        }

        void force_value(variable &&value) {
            variable *var = get_variable();
            for (int n = m_size; n > 1; ++var, --n) {
                var->assign(value);
            }
            var->assign(std::move(value));
        }

        void inc_value(const variable &value) {
            if (value.is_null()) return;
            std::for_each_n(get_variable(), m_size, [&](variable &var) {
                var += value;
            });
        }

        void dec_value(const variable &value) {
            if (value.is_null()) return;
            std::for_each_n(get_variable(), m_size, [&](variable &var) {
                var -= value;
            });
        }

        void clear_value() {
            m_current_map.erase(m_name);
        }

        variable get_value() const {
            if (auto var = get_variable_const()) {
                return var;
            } else {
                return variable();
            }
        }
    };
}

#endif