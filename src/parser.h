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

    struct variable_prefixes {
        enums::bitset<setvar_flags> flags;
        command_args call;
        bool function_arg:1 = false;
        bool pushref:1 = false;
    };
    
    DEFINE_ENUM_FLAGS_IN_NS(bls, parser_flags,
        (ADD_COMMENTS)
    )

    struct loop_label_pair {
        string_ptr continue_label;
        string_ptr break_label;
    };

    struct function_info {
        small_int numargs;
        bool has_contents;
    };

    class invalid_numargs : public parsing_error {
    private:
        static std::string get_message(const std::string &fun_name, size_t minargs, size_t maxargs) {
            if (maxargs == std::numeric_limits<size_t>::max()) {
                return std::format("La funzione {0} richiede almeno {1} argomenti", fun_name, minargs);
            } else if (minargs == maxargs) {
                return std::format("La funzione {0} richiede {1} argomenti", fun_name, minargs);
            } else {
                return std::format("La funzione {0} richiede {1}-{2} argomenti", fun_name, minargs, maxargs);
            }
        }

    public:
        invalid_numargs(const std::string &fun_name, size_t minargs, size_t maxargs, token &tok)
            : parsing_error(get_message(fun_name, minargs, maxargs), tok) {}
    };

    class parser {
    public:
        parser() : m_parser_id(parser_counter++) {}

        void add_flag(parser_flags flag) {
            m_flags.set(flag);
        }

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
        void sub_statement();

        void read_expression();
        void sub_expression();

        void read_function();
        void read_keyword();

        variable_prefixes read_variable(bool read_only = false);

        template<opcode Cmd> void add_enum_index_command();

    private:
        static inline int parser_counter = 0;
        int m_parser_id;

        const layout_box_list *m_layout = nullptr;
        std::filesystem::path m_path;

        lexer m_lexer;
        bytecode m_code;

        simple_stack<loop_label_pair> m_loop_labels;
        std::vector<std::string> m_fun_args;
        std::map<std::string, function_info, std::less<>> m_functions;
        int m_content_level = 0;
        int m_function_level = 0;

        enums::bitset<parser_flags> m_flags;

        friend class lexer;
    };

}

#endif