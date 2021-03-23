#include <iostream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

#include <json/value.h>

#include "parser.h"
#include "fixed_point.h"
#include "utils.h"
#include "intl.h"
#include "binary_bls.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    std::filesystem::path input_bls;
    std::filesystem::path output_cache;

    bitset<parser_flags> flags;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls");
    parser.AddSwitch("s", "skip-comments", "Skip Comments");
    parser.AddSwitch("r", "recursive-imports", "Recursive Imports");
    parser.AddOption("o", "output-cache", "Output Cache");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    auto check_flag = [&](auto flag, const char *name, bool invert = false) {
        flags |= flags_t(flag) & -(invert ^ parser.FoundSwitch(name) == wxCMD_SWITCH_ON);
    };
    auto check_option = [&](auto &out, const char *name) {
        if (wxString str; parser.Found(name, &str)) {
            out = str.ToStdString();
        }
    };

    input_bls = parser.GetParam(0).ToStdString();
    check_flag(parser_flags::ADD_COMMENTS, "s", true);
    check_flag(parser_flags::RECURSIVE_IMPORTS, "r");
    check_option(output_cache, "o");
    return true;
}

static std::string quoted_string(const std::string &str) {
    return string_trim(Json::Value(str).toStyledString());
}

std::multimap<size_t, std::string> inv_labels;
size_t line_num;

template<typename T> std::ostream &print_args(std::ostream &out, const T &args) {
    return out << ' ' << args;
}

template<> std::ostream &print_args(std::ostream &out, const pdf_rect &box) {
    return out << ' ' << read_mode_strings[static_cast<int>(box.mode)]
        << ' ' << box_type_strings[static_cast<int>(box.type)]
        << print_flags(box.flags, box_flags_names)
        << fmt::format(" {} {} {} {} {}", box.page, box.x, box.y, box.w, box.h);
}

template<> std::ostream &print_args(std::ostream &out, const spacer_index &idx) {
    return out << ' ' << spacer_index_names[static_cast<int>(idx)];
}

template<> std::ostream &print_args(std::ostream &out, const command_call &args) {
    return out << ' ' << args.fun->first << ' ' << int(args.numargs);
}

template<> std::ostream &print_args(std::ostream &out, const variable_selector &args) {
    out << ' ' << args.name << ' ' << int(args.index);
    if (args.length != 1) {
        out << ':' << int(args.length);
    }
    return out << print_flags(args.flags, selvar_flags_names);
}

template<> std::ostream &print_args(std::ostream &out, const bitset<setvar_flags> &args) {
    return out << print_flags(args, setvar_flags_names);
}

template<> std::ostream &print_args(std::ostream &out, const fixed_point &num) {
    return out << ' ' << fixed_point_to_string(num);
}

template<> std::ostream &print_args(std::ostream &out, const std::string &str) {
    return out << ' ' << quoted_string(str);
}

template<> std::ostream &print_args(std::ostream &out, const small_int &num) {
    return out << ' ' << num_tostring(num);
}

template<> std::ostream &print_args(std::ostream &out, const intl::language &lang) {
    return out << ' ' << intl::language_string(lang);
}

template<> std::ostream &print_args(std::ostream &out, const import_options &args) {
    return out << ' ' << quoted_string(args.filename.string())
        << print_flags(args.flags, import_flags_names);
}

template<> std::ostream &print_args(std::ostream &out, const jump_address &addr) {
    out << ' ';
    if (auto jt = inv_labels.find(line_num + addr); jt != inv_labels.end()) {
        return out << jt->second;
    } else {
        return out << addr;
    }
}

template<> std::ostream &print_args(std::ostream &out, const jsr_address &addr) {
    print_args(out, addr.addr) << ' ' << num_tostring(addr.numargs);
    if (addr.nodiscard) {
        out << " NODISCARD";
    }
    return out;
}

template<> std::ostream &print_args(std::ostream &out, const jump_uneval &) {
    return out;
}

template<opcode Cmd>
std::ostream &print_line(std::ostream &out, const command_args &line) {
    out << '\t' << opcode_names[static_cast<int>(Cmd)];
    if constexpr (!std::is_void_v<opcode_type<Cmd>>) {
        print_args(out, line.get_args<Cmd>());
    }
    return out << std::endl;
}

int MainApp::OnRun() {
    try {
        parser my_parser;
        my_parser.add_flags(flags);
        my_parser.read_layout(input_bls.parent_path(), box_vector::from_file(input_bls));

        for (auto &[label, addr] : my_parser.get_labels()) {
            inv_labels.emplace(addr, label);
        }

        const auto &code = my_parser.get_bytecode();
        if (!output_cache.empty()) {
            binary_bls::write(code, output_cache);
        }
        const auto &comments = my_parser.get_comments();
        for (line_num=0; line_num < code.size(); ++line_num) {
            auto [label_begin, label_end] = inv_labels.equal_range(line_num);
            for (;label_begin != label_end; ++label_begin) {
                std::cout << label_begin->second << ':' << std::endl;
            }
            auto [comment_begin, comment_end] = comments.equal_range(line_num);
            for (;comment_begin != comment_end; ++comment_begin) {
                std::cout << comment_begin->second << std::endl;
            }

            auto &line = code[line_num];
#define O_IMPL(x, t) case opcode::x: print_line<opcode::x>(std::cout, line); break;
            switch (line.command()) { OPCODES }
#undef O_IMPL
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Errore sconosciuto" << std::endl;
        return 1;
    }

    return 0;
}