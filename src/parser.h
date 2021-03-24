#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <fmt/core.h>

#include "layout.h"
#include "lexer.h"
#include "bytecode.h"
#include "stack.h"

DEFINE_ENUM_FLAGS(variable_prefixes,
    (OVERWRITE)
    (PARSENUM)
    (AGGREGATE)
    (NEGATE)
    (CAPITALIZE)
    (REF)
    (FORCE)
)

DEFINE_ENUM_FLAGS(parser_flags,
    (ADD_COMMENTS)
    (RECURSIVE_IMPORTS)
)

struct loop_label_pair {
    std::string continue_label;
    std::string break_label;
};

class invalid_numargs : public parsing_error {
private:
    static std::string get_message(const std::string &fun_name, size_t minargs, size_t maxargs) {
        if (maxargs == std::numeric_limits<size_t>::max()) {
            return fmt::format("La funzione {0} richiede almeno {1} argomenti", fun_name, minargs);
        } else if (minargs == maxargs) {
            return fmt::format("La funzione {0} richiede {1} argomenti", fun_name, minargs);
        } else {
            return fmt::format("La funzione {0} richiede {1}-{2} argomenti", fun_name, minargs, maxargs);
        }
    }

public:
    invalid_numargs(const std::string &fun_name, size_t minargs, size_t maxargs, token &tok)
        : parsing_error(get_message(fun_name, minargs, maxargs), tok) {}
};

class parser {
public:
    parser() = default;

    void add_flags(auto flags) {
        m_flags |= flags;
    }

    void read_layout(const std::filesystem::path &path, const box_vector &layout);

    const auto &get_bytecode() const & {
        return m_code;
    }

    auto &&get_bytecode() && {
        return std::move(m_code);
    }
    
    const auto &get_labels() const {
        return m_labels;
    }

    const auto &get_comments() const {
        return m_comments;
    }

private:
    void read_box(const layout_box &box);
    const layout_box *current_box = nullptr;
    
    bool read_statement(bool throw_on_eof = true);
    void read_expression();
    void sub_expression();

    void read_function();
    void read_keyword();

    void read_variable_name();
    bitset<variable_prefixes> read_variable(bool read_only = false);

private:
    template<opcode Cmd, typename ... Ts>
    void add_line(Ts && ... args) {
        m_code.push_back(make_command<Cmd>(std::forward<Ts>(args) ... ));
    }

    template<opcode Cmd, typename ... Ts>
    void add_jump(const std::string &label, Ts && ... args) {
        m_code.push_back(make_command<opcode::UNEVAL_JUMP>(label, Cmd, opcode_type<Cmd>{ jump_address(0), std::forward<Ts>(args) ... }));
    }

    template<opcode Cmd> requires std::is_same_v<opcode_type<Cmd>, void>
    void add_jump(const std::string &label) {
        m_code.push_back(make_command<opcode::UNEVAL_JUMP>(label, Cmd));
    }

    void add_label(const std::string &label);
    void add_comment(const std::string &comment);

private:
    const box_vector *m_layout = nullptr;
    std::filesystem::path m_path;

    lexer m_lexer;
    bytecode m_code;
    std::multimap<size_t, std::string> m_comments;
    std::map<std::string, size_t> m_labels;

    simple_stack<loop_label_pair> m_loop_labels;
    int m_content_level = 0;

    bitset<parser_flags> m_flags;

    friend class lexer;
};

#endif