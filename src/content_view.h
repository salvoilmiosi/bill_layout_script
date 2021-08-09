#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {
    class content_view {
    private:
        size_t m_index = 0;
        std::vector<variable> m_data;

    public:
        content_view(const std::string &str) : m_data({str}) {}
        content_view(std::string &&str) : m_data({std::move(str)}) {}

        content_view(const variable &var) {
            if (var.is_view()) {
                m_data.push_back(var);
            } else {
                m_data.push_back(var.as_string());
            }
        }

        content_view(variable &&var) {
            if (var.is_view()) {
                m_data.push_back(var);
            } else {
                m_data.push_back(std::move(var).as_string());
            }
        }

        content_view(const std::vector<variable> &vec) : m_data(vec) {}
        content_view(std::vector<variable> &&vec) : m_data(std::move(vec)) {}

        size_t index() const {
            return m_index;
        }

        void nextresult() {
            ++m_index;
        }

        bool tokenend() const {
            return m_index >= m_data.size();
        }

        variable view() const {
            if (tokenend()) return {};
            auto &var = m_data[m_index];
            if (var.is_string()) {
                return var.as_view();
            } else {
                return var;
            }
        }
    };

}

#endif