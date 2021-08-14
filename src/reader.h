#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <atomic>

#include "variable.h"
#include "variable_selector.h"
#include "functions.h"
#include "bytecode.h"
#include "stack.h"
#include "content_view.h"

namespace bls {

DEFINE_ENUM_FLAGS_IN_NS(bls, reader_flags,
    (HALT_ON_SETLAYOUT)
    (USE_CACHE)
)

struct function_call {
    arg_list args;
    size_t return_addr;
    bool getretvalue;
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
    
    const pdf_document &get_document() const {
        if (m_doc) {
            return *m_doc;
        } else {
            throw layout_error(intl::format("NO_DOCUMENT_OPEN"));
        }
    }

    // ritorna l'indirizzo del codice aggiunto
    size_t add_layout(const std::filesystem::path &filename);
    size_t add_code(bytecode &&new_code);

    void add_flag(reader_flags flag) {
        m_flags.set(flag);
    }

    void clear();
    void start();

    const auto &get_values() const { return m_values; }
    const auto &get_globals() const { return m_globals; }
    const auto &get_notes() const { return m_notes; }
    const auto &get_layouts() const { return m_layouts; }

    void abort() {
        m_running = false;
        m_aborted = true;
    }

private:
    void exec_command(const command_args &cmd);

    bytecode m_code;

    std::list<variable_map> m_values;
    decltype(m_values)::iterator m_current_table;

    variable_map m_globals;

    simple_stack<variable> m_stack;
    simple_stack<content_view> m_contents;
    simple_stack<function_call> m_calls;
    simple_stack<variable_selector> m_selected;

    std::vector<std::filesystem::path> m_layouts;
    decltype(m_layouts)::iterator m_current_layout;

    std::locale m_locale;

    std::vector<std::string> m_notes;

    pdf_rect m_current_box;

    size_t m_program_counter;
    
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_aborted = false;
    enums::bitset<reader_flags> m_flags;

    const pdf_document *m_doc = nullptr;

    friend class function_lookup;
};

}

#endif