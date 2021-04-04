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
    std::filesystem::path input_file;
    std::filesystem::path output_cache;

    bitset<parser_flags> flags;
    bool do_read_cache = false;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-file");
    parser.AddSwitch("s", "skip-comments", "Skip Comments");
    parser.AddSwitch("r", "recursive-imports", "Recursive Imports");
    parser.AddSwitch("c", "read-cache", "Read Cache");
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

    input_file = parser.GetParam(0).ToStdString();
    check_flag(parser_flags::ADD_COMMENTS, "s", true);
    check_flag(parser_flags::RECURSIVE_IMPORTS, "r");
    do_read_cache = parser.FoundSwitch("c") == wxCMD_SWITCH_ON;
    check_option(output_cache, "o");
    return true;
}

static std::string quoted_string(const std::string &str) {
    return string_trim(Json::Value(str).toStyledString());
}

template<typename T> struct print_args {
    const T &data;
    print_args(const T &data) : data(data) {}
};

template<typename T> print_args(T) -> print_args<T>;

template<typename T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
    return out << args.data;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<readbox_options> &args) {
    return out << args.data.mode << ' ' << args.data.type << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_call> &args) {
    return out << args.data.fun->first << ' ' << num_tostring(args.data.numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<variable_selector> &args) {
    out << *args.data.name << ' ' << int(args.data.index);
    if (args.data.length != 1) {
        out << ':' << int(args.data.length);
    }
    return out << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << fixed_point_to_string(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<string_ptr> &args) {
    return out << quoted_string(*args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<small_int> &args) {
    return out << num_tostring(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<intl::language> &args) {
    return out << intl::language_string(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<import_options> &args) {
    return out << print_args(args.data.filename) << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jsr_address> &args) {
    return out << print_args(args.data.label) << ' ' << num_tostring(args.data.numargs);
}

int MainApp::OnRun() {
    try {
        bytecode code;

        if (!do_read_cache) {
            parser my_parser;
            my_parser.add_flags(flags);
            my_parser.read_layout(input_file, box_vector::from_file(input_file));
            code = std::move(my_parser).get_bytecode();
        } else {
            code = binary_bls::read(input_file);
        }

        if (!output_cache.empty()) {
            binary_bls::write(code, output_cache);
        }
        for (auto line = code.begin(); line != code.end(); ++line) {
            if (line->command() == opcode::COMMENT) {
                std::cout << *line->get_args<opcode::COMMENT>();
            } else {
                std::cout << '\t' << line->command();
                line->visit(overloaded{
                    [](std::monostate){},
                    [&](auto && args) {
                        std::cout << ' ' << print_args(args);
                    }
                });
            }
            std::cout << std::endl;
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