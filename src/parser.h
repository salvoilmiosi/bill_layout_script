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

    struct loop_state {
        command_node continue_node;
        command_node break_node;
        int entry_content_level;
    };

    struct function_info {
        command_node node;
        size_t numargs;
    };

    struct parser_code : command_list {
        command_list unadded_labels;

        command_node make_label() {
            static int label_count = 0;
            return unadded_labels.emplace(unadded_labels.end(), make_command<opcode::LABEL>(label_count++));
        }

        void add_label(command_node node) {
            splice(end(), unadded_labels, node);
        }
        
        command_args &last_not_comment() {
            return *std::find_if_not(rbegin(), rend(), [](const command_args &cmd) {
                return cmd.command() == opcode::COMMENT;
            });
        }
    };

    class parser {
    public:
        command_list read_layout(const std::filesystem::path &path, const layout_box_list &layout);
        
    private:
        token read_goto_label(const layout_box &box);
        void read_box(const layout_box &box);
        
        void read_statement();
        void assignment_stmt();

        void read_expression();
        void sub_expression();

        void read_function(token tok_fun_name, bool top_level);
        void read_variable_name();
        void read_variable_indices();

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
        void parse_tie_stmt();

    private:
        std::filesystem::path m_path;
        std::string m_lang;

        lexer m_lexer;
        parser_code m_code;

        simple_stack<loop_state> m_loop_stack;
        util::string_map<function_info> m_functions;
        util::string_map<command_node> m_goto_labels;
        int m_content_level = 0;

        friend class lexer;
    };

}

#endif