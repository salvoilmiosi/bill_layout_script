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

struct box_spacer {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    int page = 0;
};

using variable_map = std::multimap<std::string, variable>;

struct reader_output {
    variable_map globals;
    std::vector<variable_map> values;
    std::vector<std::string> warnings;
    std::vector<std::filesystem::path> layouts;
};

enum reader_flags {
    READER_HALT_ON_SETLAYOUT = 1 << 0,
};

class reader {
public:
    reader();
    ~reader();

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
    size_t add_layout(const std::filesystem::path &filename);

    void add_flags(reader_flags flags) {
        m_flags |= flags;
    }

    void start();

    const reader_output &get_output() {
        return m_out;
    }

    void halt() {
        m_running = false;
    }

private:
    void exec_command(const command_args &cmd);
    void read_box(pdf_rect box);
    void call_function(const std::string &name, size_t numargs);
    void import_layout(const std::string &layout_name, bool set_layout = false);

private:
    simple_stack<variable> m_vars;
    simple_stack<content_view> m_contents;
    simple_stack<variable_ref> m_refs;

    // {nome file layout caricato, indirizzo di ritorno}
    simple_stack<std::pair<std::filesystem::path, size_t>> m_call_stack;

    // nome file layout -> indirizzo in m_code
    std::map<std::filesystem::path, size_t> m_loaded_layouts;

    box_spacer m_spacer;
    int m_last_box_page;

    size_t m_current_table;

    size_t m_program_counter;
    bool m_jumped = false;
    
    std::atomic<bool> m_running = false;

private:
    const pdf_document *m_doc = nullptr;

    bytecode m_code;

    uint8_t m_flags = 0;

    reader_output m_out;
};

#endif