#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "binary_bls.h"

#include <boost/locale.hpp>

using namespace bls;

void reader::clear() {
    m_code.clear();
    m_flags = 0;
    m_doc = nullptr;
}

void reader::start() {
    m_values.clear();
    m_globals.clear();
    m_notes.clear();
    m_layouts.clear();
    m_stack.clear();
    m_contents.clear();
    m_selected.clear();

    m_values.emplace_back();
    m_current_table = m_values.begin();

    m_calls.clear();
    m_calls.emplace(std::numeric_limits<decltype(m_program_counter)>::max());

    m_current_box = {};

    m_locale = std::locale::classic();

    m_program_counter = 0;
    m_program_counter_next = 0;

    m_running = true;
    m_aborted = false;

    try {
        while (m_running && m_program_counter < m_code.size()) {
            m_program_counter_next = m_program_counter + 1;
            exec_command(m_code[m_program_counter]);
            m_program_counter = m_program_counter_next;
        }
    } catch (const layout_error &err) {
        throw reader_error(std::format("{0}: Ln {1}\n{2}\n{3}",
            m_box_name, m_last_line.line, m_last_line.comment,
            err.what()));
    }
    if (m_aborted) {
        throw reader_aborted{};
    }
    
    m_running = false;
}

void reader::jump_to(const jump_address &address) {
    m_program_counter_next = m_program_counter + address.address;
}

void reader::jump_subroutine(const jump_address &address, bool getretvalue) {
    m_calls.emplace(m_program_counter + 1, getretvalue);
    jump_to(address);
}

void reader::jump_return() {
    auto fun_call = m_calls.pop();
    m_program_counter_next = fun_call.return_addr;

    if (fun_call.getretvalue) {
        m_stack.emplace();
    }
}

void reader::jump_return_value() {
    auto fun_call = m_calls.pop();
    m_program_counter_next = fun_call.return_addr;

    if (!fun_call.getretvalue) {
        m_stack.pop();
    }
}

void reader::move_box(spacer_index idx, const variable &amt) {
    switch (idx) {
    case spacer_index::PAGE:
        m_current_box.page += amt.as_int(); break;
    case spacer_index::ROTATE:
        m_current_box.rotate(amt.as_int()); break;
    case spacer_index::X:
        m_current_box.x += amt.as_double(); break;
    case spacer_index::Y:
        m_current_box.y += amt.as_double(); break;
    case spacer_index::WIDTH:
    case spacer_index::RIGHT:
        m_current_box.w += amt.as_double(); break;
    case spacer_index::HEIGHT:
    case spacer_index::BOTTOM:
        m_current_box.h += amt.as_double(); break;
    case spacer_index::TOP:
        m_current_box.y += amt.as_double();
        m_current_box.h -= amt.as_double();
        break;
    case spacer_index::LEFT:
        m_current_box.x += amt.as_double();
        m_current_box.w -= amt.as_double();
        break;
    }
}

std::string reader::read_box(readbox_options opts) {
    m_current_box.mode = opts.mode;
    m_current_box.flags = opts.flags;

    return get_document().get_text(m_current_box);
};

variable reader::call_function(const command_call &call) {
    return call.fun->second(this, m_stack, call.numargs);
}

void reader::import_layout(const std::string &path) {
    jump_address addr = add_layout(path) - m_program_counter;
    m_code[m_program_counter] = make_command<opcode::JSR>(addr);
    jump_subroutine(addr);
}

void reader::push_layout(const std::string &path) {
    m_layouts.push_back(path);
    m_code[m_program_counter] = make_command<opcode::SETCURLAYOUT>(m_layouts.size() - 1);
    m_current_layout = m_layouts.end() - 1;
}

void reader::set_language(const std::string &lang) {
    try {
        m_locale = boost::locale::generator{}(lang);
    } catch (std::runtime_error) {
        throw layout_error(intl::format("UNSUPPORTED_LANGUAGE", lang));
    }
}

void reader::stack_top_to_subitem(small_int idx) {
    auto &var = m_stack.top();
    if (var.is_array()) {
        const auto &arr = var.deref().as_array();
        if (arr.size() > idx) {
            if (var.is_pointer()) {
                var = arr[idx].as_pointer();
            } else {
                var = arr[idx];
            }
            return;
        }
    }
    var = variable();
}

size_t reader::add_layout(const std::filesystem::path &filename) {
    std::filesystem::path cache_filename = filename;
    cache_filename.replace_extension(".cache");

    auto is_modified = [&](const std::filesystem::path &file) {
        return std::filesystem::exists(file) && std::filesystem::last_write_time(file) > std::filesystem::last_write_time(cache_filename);
    };
    
    bytecode new_code;

    // Se settata flag USE_CACHE, leggi il file di cache e
    // ricompila solo se uno dei file importati è stato modificato.
    // Altrimenti ricompila sempre
    if (m_flags.check(reader_flags::USE_CACHE) && std::filesystem::exists(cache_filename) && !is_modified(filename)) {
        new_code = binary::read(cache_filename);
    } else {
        parser my_parser;
        my_parser.read_layout(filename, layout_box_list::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags.check(reader_flags::USE_CACHE)) {
            binary::write(new_code, cache_filename);
        }
    }

    return add_code(std::move(new_code));
}

size_t reader::add_code(bytecode &&new_code) {
    if (!m_code.empty()) {
        new_code.add_line<opcode::SETCURLAYOUT>(m_current_layout - m_layouts.begin());
        if (std::has_facet<boost::locale::info>(m_locale)) {
            new_code.add_line<opcode::SETLANG>(std::use_facet<boost::locale::info>(m_locale).name());
        } else {
            new_code.add_line<opcode::SETLANG>();
        }
    }
    new_code.add_line<opcode::RET>();

    return m_code.insert(m_code.end(), std::make_move_iterator(new_code.begin()), std::make_move_iterator(new_code.end())) - m_code.begin();
}

#define CMDFUN_TYPE(cmd, type) [this](command_tag<opcode::cmd>, type)
#define CMDFUN_VOID(cmd) [this](command_tag<opcode::cmd>)

#define GET_3RD(a, b, c, ...) c
#define CMDFUN_CHOOSER(...) GET_3RD(__VA_ARGS__, CMDFUN_TYPE, CMDFUN_VOID)
#define CMDFUN(...) CMDFUN_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

void reader::exec_command(const command_args &cmd) {
    visit_command(util::overloaded{
        CMDFUN(NOP) {},
        CMDFUN(LABEL, jump_label) {},
        CMDFUN(BOXNAME, const std::string &name)    { m_box_name = name; },
        CMDFUN(COMMENT, const comment_line &line)   { m_last_line = line; },
        CMDFUN(NEWBOX)                              { m_current_box = {}; },
        CMDFUN(MVBOX, spacer_index idx)             { move_box(idx, m_stack.pop()); },
        CMDFUN(MVNBOX, spacer_index idx)            { move_box(idx, -m_stack.pop()); },
        CMDFUN(RDBOX, readbox_options opts)         { m_contents.emplace(read_box(opts)); },
        CMDFUN(SELVAR, const std::string &name)     { m_selected.emplace(name, *m_current_table); },
        CMDFUN(SELVARDYN)                           { m_selected.emplace(m_stack.pop().as_string(), *m_current_table); },
        CMDFUN(SELGLOBAL, const std::string &name)  { m_selected.emplace(name, m_globals); },
        CMDFUN(SELGLOBALDYN)                        { m_selected.emplace(m_stack.pop().as_string(), m_globals); },
        CMDFUN(SELLOCAL, const std::string &name)   { m_selected.emplace(name, m_calls.top().vars); },
        CMDFUN(SELLOCALDYN)                         { m_selected.emplace(m_stack.pop().as_string(), m_globals); },
        CMDFUN(SELINDEX, small_int idx)             { m_selected.top().add_index(idx); },
        CMDFUN(SELINDEXDYN)                         { m_selected.top().add_index(m_stack.pop().as_int()); },
        CMDFUN(SELSIZE, small_int size)             { m_selected.top().set_size(size); },
        CMDFUN(SELSIZEDYN)                          { m_selected.top().set_size(m_stack.pop().as_int()); },
        CMDFUN(SELAPPEND)                           { m_selected.top().add_append(); },
        CMDFUN(SELEACH)                             { m_selected.top().add_each(); },
        CMDFUN(FWDVAR)                              { m_selected.pop().fwd_value(m_stack.pop()); },
        CMDFUN(SETVAR)                              { m_selected.pop().set_value(m_stack.pop()); },
        CMDFUN(FORCEVAR)                            { m_selected.pop().force_value(m_stack.pop()); },
        CMDFUN(INCVAR)                              { m_selected.pop().inc_value(m_stack.pop()); },
        CMDFUN(DECVAR)                              { m_selected.pop().dec_value(m_stack.pop()); },
        CMDFUN(CLEAR)                               { m_selected.pop().clear_value(); },
        CMDFUN(SUBITEM, small_int idx)              { stack_top_to_subitem(idx); },
        CMDFUN(SUBITEMDYN)                          { stack_top_to_subitem(m_stack.pop().as_int()); },
        CMDFUN(PUSHVAR)                             { m_stack.push(m_selected.pop().get_value()); },
        CMDFUN(PUSHVIEW)                            { m_stack.push(m_contents.top().view()); },
        CMDFUN(PUSHNUM, fixed_point num)            { m_stack.push(num); },
        CMDFUN(PUSHINT, big_int num)                { m_stack.push(num); },
        CMDFUN(PUSHDOUBLE, double num)              { m_stack.push(num); },
        CMDFUN(PUSHSTR, const std::string &str)     { m_stack.push(str); },
        CMDFUN(PUSHREGEX, const std::string &str)   { m_stack.emplace(str, as_regex_tag); },
        CMDFUN(CALL, const command_call &call)      { m_stack.push(call_function(call)); },
        CMDFUN(SYSCALL, const command_call &call)   { call_function(call); },
        CMDFUN(CNTADD)                              { m_contents.emplace(m_stack.pop()); },
        CMDFUN(CNTADDLIST)                          { m_contents.emplace(m_stack.pop(), as_array_tag); },
        CMDFUN(CNTPOP)                              { m_contents.pop_back(); },
        CMDFUN(NEXTRESULT)                          { m_contents.top().nextresult(); },
        CMDFUN(JMP, const jump_address &address)    { jump_to(address); },
        CMDFUN(JZ, const jump_address &address)     { if(!m_stack.pop().is_true()) jump_to(address); },
        CMDFUN(JNZ, const jump_address &address)    { if(m_stack.pop().is_true()) jump_to(address); },
        CMDFUN(JTE, const jump_address &address)    { if(m_contents.top().tokenend()) jump_to(address); },
        CMDFUN(JSR, const jump_address &address)    { jump_subroutine(address); },
        CMDFUN(JSRVAL, const jump_address &address) { jump_subroutine(address, true); },
        CMDFUN(RET)                                 { jump_return(); },
        CMDFUN(RETVAL)                              { jump_return_value(); },
        CMDFUN(IMPORT, const std::string &path)     { import_layout(path); },
        CMDFUN(ADDLAYOUT, const std::string &path)  { push_layout(path); },
        CMDFUN(SETCURLAYOUT, small_int idx)         { m_current_layout = m_layouts.begin() + idx; },
        CMDFUN(SETLANG, const std::string &lang)    { set_language(lang); },
        CMDFUN(FOUNDLAYOUT)                         { if (m_flags.check(reader_flags::FIND_LAYOUT)) m_running = false; },
    }, cmd);
}