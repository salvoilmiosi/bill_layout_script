#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <atomic>

#include "variable.h"
#include "variable_ref.h"
#include "bytecode.h"
#include "stack.h"
#include "content_view.h"

DEFINE_ENUM_FLAGS(reader_flags,
    (HALT_ON_SETLAYOUT)
    (USE_CACHE)
    (RECURSIVE)
)

struct function_call {
    size_t return_addr;
    small_int first_arg;
    small_int numargs;
    bool nodiscard;
};

class reader {
public:
    reader() = default;

    reader(const pdf_document &doc) {
        set_document(doc);
    }

    void set_document(const pdf_document &doc) {
        m_doc = &doc;
    }

    void set_document(pdf_document &&doc) = delete;

    // ritorna l'indirizzo del codice aggiunto
    jump_label add_layout(const std::filesystem::path &filename);
    jump_label add_code(bytecode &&new_code);

    void add_flags(auto flag) {
        m_flags |= flag;
    }

    void clear();
    void start();

    const auto &get_values() { return m_values; }
    const auto &get_warnings() { return m_warnings; }
    const size_t get_table_count() { return m_table_index + 1; }
    const auto get_layouts() { return m_layouts; }

    bool is_running() {
        return m_running;
    }
    void halt() {
        m_running = false;
    }

private:
    void exec_command(const command_args &cmd);
    bytecode m_code;
    std::map<jump_label, size_t> m_labels;

    variable_map m_values;
    small_int m_table_index = 0;

    simple_stack<variable> m_stack;
    simple_stack<content_view> m_contents;
    static_stack<function_call> m_calls;

    variable_ref m_selected;

    std::vector<std::filesystem::path> m_layouts;

    std::vector<std::string> m_warnings;

    pdf_rect m_current_box;

    size_t m_program_counter;
    
    std::atomic<bool> m_running = false;
    bitset<reader_flags> m_flags;

    const pdf_document *m_doc = nullptr;
};

#endif