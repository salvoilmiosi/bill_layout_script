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
#include "variable_view.h"

namespace bls {

DEFINE_ENUM_FLAGS_IN_NS(bls, reader_flags,
    (FIND_LAYOUT)
)

struct function_call {
    variable_map vars;
    command_node return_addr;
    variable return_value;
    bool getretvalue;

    function_call() : getretvalue(false) {}

    function_call(command_node return_addr, bool getretvalue = false)
        : return_addr(return_addr)
        , getretvalue(getretvalue) {}
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
            throw layout_error(intl::translate("NO_DOCUMENT_OPEN"));
        }
    }

    // ritorna l'indirizzo del codice aggiunto
    command_node add_layout(const std::filesystem::path &filename);
    command_node add_code(command_list &&new_code);

    void add_flag(reader_flags flag) {
        m_flags.set(flag);
    }

    void clear();
    void start();

    const auto &get_values() const { return m_values; }
    const auto &get_notes() const { return m_notes; }
    const auto &get_layouts() const { return m_layouts; }
    const auto &get_current_layout() const { return *m_current_layout; }

    void abort() {
        m_running = false;
        m_aborted = true;
    }

private:
    void jump_to(command_node node) {
        m_program_counter_next = node;
    }

    void jump_subroutine(command_node node, bool getretvalue = false) {
        m_calls.emplace(std::next(m_program_counter), getretvalue);
        jump_to(node);
    }

    variable do_function_call(const command_call &call);

    void exec_command(const command_args &cmd);

private:
    command_list m_code;

    std::list<variable_map> m_values;
    std::list<variable_map>::iterator m_current_table;

    variable_map m_globals;

    util::simple_stack<variable> m_stack;
    util::simple_stack<variable_view> m_views;
    util::simple_stack<function_call> m_calls;
    util::simple_stack<variable_selector> m_selected;

    std::set<std::filesystem::path> m_layouts;
    std::set<std::filesystem::path>::const_iterator m_current_layout;

    const std::string *m_box_name;
    const std::string *m_last_line;
    size_t m_numargs;

    std::locale m_locale;

    std::vector<std::string> m_notes;

    pdf_rect m_current_box;

    command_node m_program_counter;
    command_node m_program_counter_next;
    
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_aborted = false;
    enums::bitset<reader_flags> m_flags;

    const pdf_document *m_doc = nullptr;

    friend class function_lookup;
};

}

#endif