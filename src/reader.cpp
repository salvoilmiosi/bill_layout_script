#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"
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
    m_notes.clear();
    m_layouts.clear();
    m_stack.clear();
    m_contents.clear();
    m_calls.clear();

    m_selected = {};
    m_current_box = {};

    m_current_locale = std::locale::classic();

    m_program_counter = 0;
    m_table_index = 0;
    m_table_count = 1;

    m_running = true;
    m_aborted = false;

    while (m_running && m_program_counter < m_code.size()) {
        exec_command(m_code[m_program_counter]);
        ++m_program_counter;
    }
    if (m_aborted) {
        throw reader_aborted{};
    }
    
    m_running = false;
}

void reader::exec_command(const command_args &cmd) {
    auto select_var = [&](variable_selector sel) {
        if (sel.flags.check(selvar_flags::DYN_LEN)) {
            sel.length = m_stack.pop().as_int();
        }
        if (sel.flags.check(selvar_flags::DYN_IDX)) {
            sel.index = m_stack.pop().as_int();
        }

        m_selected = variable_ref(m_values, variable_key{
            (sel.flags.check(selvar_flags::DYN_NAME))
                ? m_stack.pop().as_string() : sel.name,
            (sel.flags.check(selvar_flags::GLOBAL))
                ? variable_key::global_index : m_table_index},
            sel.index, sel.length);

        if (sel.flags.check(selvar_flags::EACH)) {
            m_selected.index = 0;
            m_selected.length = m_selected.size();
        }
        if (sel.flags.check(selvar_flags::APPEND)) {
            m_selected.index = m_selected.size();
        }
    };

    auto jump_to = [&](const jump_address &label) {
        m_program_counter += std::get<ptrdiff_t>(label);
    };

    auto jump_subroutine = [&](const jsr_address &address, bool nodiscard = false) {
        m_calls.push(function_call{m_program_counter, small_int(m_stack.size() - address.numargs), address.numargs, nodiscard});
        jump_to(address);
    };

    auto push_function_arg = [&](small_int idx) {
        m_stack.push(m_stack[m_calls.top().first_arg + idx]);
    };

    auto jump_return = [&](auto &&ret_value) {
        auto fun_call = m_calls.pop();
        m_program_counter = fun_call.return_addr;
        m_stack.resize(m_stack.size() - fun_call.numargs);
        if (fun_call.nodiscard) {
            m_stack.push(std::forward<decltype(ret_value)>(ret_value));
        }
    };

    auto move_box = [&](spacer_index idx, variable &&amt) {
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
    };

    auto get_box_info = [&](spacer_index idx) -> variable {
        switch (idx) {
        case spacer_index::PAGE:
            return m_current_box.page;
        case spacer_index::X:
        case spacer_index::LEFT:
            return m_current_box.x;
        case spacer_index::Y:
        case spacer_index::TOP:
            return m_current_box.y;
        case spacer_index::WIDTH:
            return m_current_box.w;
        case spacer_index::HEIGHT:
            return m_current_box.h;
        case spacer_index::RIGHT:
            return m_current_box.x + m_current_box.w;
        case spacer_index::BOTTOM:
            return m_current_box.y + m_current_box.h;
        default:
            return variable();
        }
    };

    auto check_doc_ptr = [&]() {
        if (!m_doc) {
            throw layout_error("Nessun documento aperto");
        }
    };

    auto get_sys_info = [&](sys_index idx) -> variable {
        switch (idx) {
        case sys_index::DOCFILE:
            check_doc_ptr();
            return m_doc->filename().string();
        case sys_index::DOCPAGES:
            check_doc_ptr();
            return m_doc->num_pages();
        case sys_index::ATE:
            check_doc_ptr();
            return m_current_box.page > m_doc->num_pages();
        case sys_index::LAYOUT:
            return m_current_layout->string();
        case sys_index::LAYOUTDIR:
            return m_current_layout->parent_path().string();
        case sys_index::CURTABLE:
            return m_table_index;
        case sys_index::NUMTABLES:
            return m_table_count;
        default:
            return variable();
        }
    };

    auto read_box = [&](readbox_options opts) {
        m_current_box.mode = opts.mode;
        m_current_box.flags = opts.flags;

        check_doc_ptr();
        m_contents.emplace(std::in_place_type<content_string>, m_doc->get_text(m_current_box));
    };

    auto call_function = [&](const command_call &cmd) {
        variable ret = cmd.fun->second(m_current_locale, arg_list(
            m_stack.end() - cmd.numargs,
            m_stack.end()));
        m_stack.resize(m_stack.size() - cmd.numargs);
        return ret;
    };

    auto import_layout = [&](const std::filesystem::path &filename) {
        jsr_address addr{ptrdiff_t(add_layout(filename) - m_program_counter), 0};
        m_code[m_program_counter] = make_command<opcode::JSR>(addr);
        jump_subroutine(addr);
    };

    auto push_layout = [&](const std::filesystem::path &filename) {
        m_layouts.push_back(filename);
        m_code[m_program_counter] = make_command<opcode::SETCURLAYOUT>(m_layouts.size() - 1);
        m_current_layout = m_layouts.end() - 1;
    };

    auto throw_error = [&]() {
        m_running = false;
        auto message = m_stack.pop().as_string();
        auto errcode = m_stack.pop().as_int();
        throw layout_runtime_error(message, errcode);
    };

    auto set_language = [&](const std::string &name) {
        try {
            m_current_locale = boost::locale::generator{}(name);
        } catch (std::runtime_error) {
            throw layout_error(fmt::format("Lingua non supportata: {}", name));
        }
    };

    switch (cmd.command()) {
    case opcode::NOP:
    case opcode::COMMENT:
    case opcode::LABEL:         break;
    case opcode::NEWBOX:        m_current_box = {}; break;
    case opcode::MVBOX:         move_box(cmd.get_args<opcode::MVBOX>(), m_stack.pop()); break;
    case opcode::MVNBOX:        move_box(cmd.get_args<opcode::MVNBOX>(), -m_stack.pop()); break;
    case opcode::RDBOX:         read_box(cmd.get_args<opcode::RDBOX>()); break;
    case opcode::NEXTTABLE:     if (++m_table_index >= m_table_count) ++m_table_count; break;
    case opcode::FIRSTTABLE:    m_table_index = 0; break;
    case opcode::SELVAR:        select_var(cmd.get_args<opcode::SELVAR>()); break;
    case opcode::SETVAR:        m_selected.set_value(m_stack.pop(), cmd.get_args<opcode::SETVAR>()); break;
    case opcode::CLEAR:         m_selected.clear_value(); break;
    case opcode::PUSHVAR:       m_stack.push(m_selected.get_value()); break;
    case opcode::PUSHREF:       m_stack.push(m_selected.get_value_ref()); break;
    case opcode::PUSHVIEW:      m_stack.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:       m_stack.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHINT:       m_stack.push(cmd.get_args<opcode::PUSHINT>()); break;
    case opcode::PUSHDOUBLE:    m_stack.push(cmd.get_args<opcode::PUSHDOUBLE>()); break;
    case opcode::PUSHSTR:       m_stack.push(cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHARG:       push_function_arg(cmd.get_args<opcode::PUSHARG>()); break;
    case opcode::GETBOX:        m_stack.push(get_box_info(cmd.get_args<opcode::GETBOX>())); break;
    case opcode::GETSYS:        m_stack.push(get_sys_info(cmd.get_args<opcode::GETSYS>())); break;
    case opcode::CALL:          m_stack.push(call_function(cmd.get_args<opcode::CALL>())); break;
    case opcode::CNTADDSTRING:  m_contents.emplace(std::in_place_type<content_string>, m_stack.pop()); break;
    case opcode::CNTADDLIST:    m_contents.emplace(std::in_place_type<content_list>, m_stack.pop()); break;
    case opcode::CNTPOP:        m_contents.pop_back(); break;
    case opcode::SETBEGIN:      m_contents.top().setbegin(m_stack.pop().as_int()); break;
    case opcode::SETEND:        m_contents.top().setend(m_stack.pop().as_int()); break;
    case opcode::NEXTRESULT:    m_contents.top().nextresult(); break;
    case opcode::JMP:           jump_to(cmd.get_args<opcode::JMP>()); break;
    case opcode::JZ:            if(!m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JZ>()); break;
    case opcode::JNZ:           if(m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JNZ>()); break;
    case opcode::JTE:           if(m_contents.top().tokenend()) jump_to(cmd.get_args<opcode::JTE>()); break;
    case opcode::JSRVAL:        jump_subroutine(cmd.get_args<opcode::JSRVAL>(), true); break;
    case opcode::JSR:           jump_subroutine(cmd.get_args<opcode::JSR>()); break;
    case opcode::THROWERROR:    throw_error(); break;
    case opcode::ADDNOTE:       m_notes.push_back(m_stack.pop().as_string()); break;
    case opcode::RET:           jump_return(variable()); break;
    case opcode::RETVAL:        jump_return(m_stack.pop()); break;
    case opcode::RETVAR:        jump_return(m_selected.get_value()); break;
    case opcode::IMPORT:        import_layout(cmd.get_args<opcode::IMPORT>()); break;
    case opcode::ADDLAYOUT:     push_layout(cmd.get_args<opcode::ADDLAYOUT>()); break;
    case opcode::SETCURLAYOUT   : m_current_layout = m_layouts.begin() + cmd.get_args<opcode::SETCURLAYOUT>(); break;
    case opcode::SETLAYOUT:     if (m_flags.check(reader_flags::HALT_ON_SETLAYOUT)) m_running = false; break;
    case opcode::SETLANG:       set_language(cmd.get_args<opcode::SETLANG>()); break;
    case opcode::HLT:           m_running = false; break;
    }
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
    size_t addr = m_code.size();

    m_code.reserve(m_code.size() + 4 + new_code.size());
    m_code.add_line<opcode::NOP>();

    std::ranges::move(new_code, std::back_inserter(m_code));
    if (addr == 0) {
        m_code.add_line<opcode::HLT>();
    } else {
        m_code.add_line<opcode::SETCURLAYOUT>(m_current_layout - m_layouts.begin());
        if (std::has_facet<boost::locale::info>(m_current_locale)) {
            m_code.add_line<opcode::SETLANG>(std::use_facet<boost::locale::info>(m_current_locale).name());
        } else {
            m_code.add_line<opcode::SETLANG>();
        }
        m_code.add_line<opcode::RET>();
    }
    
    return addr;
}