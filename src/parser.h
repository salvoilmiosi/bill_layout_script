#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include "layout.h"
#include "lexer.h"
#include "bytecode.h"
#include "stack.h"
#include "utils.h"

namespace bls {

    struct loop_label_pair {
        std::string continue_label;
        std::string break_label;
    };

    struct function_info {
        small_int numargs;
        bool has_contents;
    };

    class invalid_numargs : public token_error {
    private:
        static std::string get_message(const std::string &fun_name, size_t minargs, size_t maxargs) {
            if (maxargs == std::numeric_limits<size_t>::max()) {
                return intl::format("FUNCTION_REQUIRES_ARGS_LEAST", fun_name, minargs);
            } else if (minargs == maxargs) {
                return intl::format("FUNCTION_REQUIRES_ARGS_EXACT", fun_name, minargs);
            } else {
                return intl::format("FUNCTION_REQUIRES_ARGSS_RANGE", fun_name, minargs, maxargs);
            }
        }

    public:
        invalid_numargs(const std::string &fun_name, size_t minargs, size_t maxargs, token &tok)
            : token_error(get_message(fun_name, minargs, maxargs), tok) {}
    };

    class parser {
    public:
        parser() : m_parser_id(parser_counter++) {}

        void read_layout(const std::filesystem::path &path, const layout_box_list &layout);

        const auto &get_bytecode() const & {
            return m_code;
        }

        auto &&get_bytecode() && {
            return std::move(m_code);
        }
        
    private:
        void read_box(const layout_box &box);
        const layout_box *current_box = nullptr;
        
        void read_statement();
        void assignment_stmt();
        void read_tie_assignment();

        void read_expression();
        void sub_expression();

        void read_function(token tok_fun_name, bool top_level);
        void read_variable_name();
        void read_variable_indices();

        jump_label make_label(std::string_view label);

        void parse_if_stmt();
        void parse_while_stmt();
        void parse_for_stmt();
        void parse_goto_stmt();
        void parse_function_stmt();
        void parse_foreach_stmt();
        void parse_with_stmt();
        void parse_import_stmt();
        void parse_break_stmt();
        void parse_continue_stmt();
        void parse_return_stmt();
        void parse_clear_stmt();

    private:
        static inline int parser_counter = 0;
        int m_parser_id;

        const layout_box_list *m_layout = nullptr;
        std::filesystem::path m_path;

        lexer m_lexer;
        bytecode m_code;

        simple_stack<loop_label_pair> m_loop_labels;
        util::string_map<function_info> m_functions;
        int m_content_level = 0;
        int m_function_level = 0;

        friend class lexer;
    };

}

#endif