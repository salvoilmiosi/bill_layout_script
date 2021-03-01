#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <atomic>

#include "variable.h"
#include "variable_ref.h"
#include "bytecode.h"
#include "stack.h"
#include "content_view.h"
#include "intl.h"

struct box_spacer {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    int page = 0;
};

#define READER_FLAGS \
F(HALT_ON_SETLAYOUT) \
F(USE_CACHED) \
F(RECURSIVE)

#define F(x) POS_READER_##x,
enum { READER_FLAGS };
#undef F
#define F(x) READER_##x = (1 << POS_READER_##x),
enum reader_flags { READER_FLAGS };
#undef F

class reader {
public:
    reader() = default;

    reader(const pdf_document &doc) {
        set_document(doc);
    }

    reader(const pdf_document &doc, const bill_layout_script &layout) {
        set_document(doc);
        add_layout(layout);
    }

    void set_document(const pdf_document &doc) {
        m_doc = &doc;
    }

    void set_document(pdf_document &&doc) = delete;

    // ritorna l'indirizzo del codice aggiunto
    size_t add_layout(const bill_layout_script &layout);
    
    size_t add_cached_layout(const std::filesystem::path &filename);

    void add_flags(reader_flags flags) {
        m_flags |= flags;
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

    variable_map m_values;
    small_int m_table_index = 0;

    simple_stack<variable> m_vars;
    simple_stack<content_view> m_contents;
    static_stack<size_t> m_return_addrs;

    variable_ref m_selected;

    std::vector<std::filesystem::path> m_layouts;

    std::vector<std::string> m_warnings;

    box_spacer m_spacer;
    int m_last_box_page;

    size_t m_program_counter;
    bool m_jumped = false;
    
    std::atomic<bool> m_running = false;
    flags_t m_flags = 0;

    const pdf_document *m_doc = nullptr;
};

#endif