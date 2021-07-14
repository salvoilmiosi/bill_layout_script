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

namespace bls {

DEFINE_ENUM_FLAGS_IN_NS(bls, reader_flags,
    (HALT_ON_SETLAYOUT)
    (USE_CACHE)
)

struct function_call {
    size_t return_addr;
    small_int first_arg;
    small_int numargs;
    bool nodiscard;
};

struct reader_aborted{};

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
    size_t add_layout(const std::filesystem::path &filename);
    size_t add_code(bytecode &&new_code);

    void add_flag(reader_flags flag) {
        m_flags.set(flag);
    }

    void clear();
    void start();

    const auto &get_values() const { return m_values; }
    const auto &get_notes() const { return m_notes; }
    const auto &get_layouts() const { return m_layouts; }
    
    size_t get_table_count() const { return m_table_index + 1; }

    void abort() {
        m_running = false;
        m_aborted = true;
    }

private:
    void exec_command(const command_args &cmd);
    bytecode m_code;

    variable_map m_values;
    small_int m_table_index = 0;

    simple_stack<variable> m_stack;
    simple_stack<content_view> m_contents;
    simple_stack<function_call> m_calls;

    variable_ref m_selected;

    std::vector<std::filesystem::path> m_layouts;
    decltype(m_layouts)::iterator m_current_layout;

    std::locale m_current_locale;

    std::vector<std::string> m_notes;

    pdf_rect m_current_box;

    size_t m_program_counter;
    
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_aborted = false;
    enums::bitset<reader_flags> m_flags;

    const pdf_document *m_doc = nullptr;
};

}

#endif