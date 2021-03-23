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

    flags_t flags;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls");
    parser.AddSwitch("s", "skip-comments", "Skip Comments");
    parser.AddSwitch("r", "recursive-imports", "Recursive Imports");
    parser.AddOption("o", "output-cache", "Output Cache");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    auto check_flag = [&](flags_t flag, const char *name, bool invert = false) {
        flags |= flag & -(invert ^ parser.FoundSwitch(name) == wxCMD_SWITCH_ON);
    };
    auto check_option = [&](auto &out, const char *name) {
        if (wxString str; parser.Found(name, &str)) {
            out = str.ToStdString();
        }
    };

    input_bls = parser.GetParam(0).ToStdString();
    check_flag(PARSER_ADD_COMMENTS, "s", true);
    check_flag(PARSER_RECURSIVE_IMPORTS, "r");
    check_option(output_cache, "o");
    return true;
}

static std::string quoted_string(const std::string &str) {
    return string_trim(Json::Value(str).toStyledString());
}

struct print_flags {
private:
    flags_t m_flags;
    const char **m_names;
    size_t m_num;

public:
    template<size_t N>
    print_flags(flags_t flags, const char * (&names)[N])
        : m_flags(flags), m_names(names), m_num(N) {}

    friend std::ostream &operator << (std::ostream &out, const print_flags &printer) {
        for (size_t i=0; i<printer.m_num; ++i) {
            if (printer.m_flags & (1 << i)) {
                out << ' ' << printer.m_names[i];
            }
        }
        return out;
    }
};

int MainApp::OnRun() {
    try {
        parser my_parser;
        my_parser.add_flags(flags);
        my_parser.read_layout(input_bls.parent_path(), box_vector::from_file(input_bls));

        std::multimap<size_t, std::string> inv_labels;
        for (auto &[label, addr] : my_parser.get_labels()) {
            inv_labels.emplace(addr, label);
        }

        const auto &code = my_parser.get_bytecode();
        if (!output_cache.empty()) {
            binary_bls::write(code, output_cache);
        }
        const auto &comments = my_parser.get_comments();
        for (size_t i=0; i < code.size(); ++i) {
            auto &line = code[i];

            auto [label_begin, label_end] = inv_labels.equal_range(i);
            for (;label_begin != label_end; ++label_begin) {
                std::cout << label_begin->second << ':' << std::endl;
            }
            auto [comment_begin, comment_end] = comments.equal_range(i);
            for (;comment_begin != comment_end; ++comment_begin) {
                std::cout << comment_begin->second << std::endl;
            }

            std::cout << '\t' << opcode_names[int(line.command())];
            switch (line.command()) {
            case OP_SETBOX: {
                auto box = line.get_args<OP_SETBOX>();
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                std::cout << print_flags(box.flags, pdf_flags_names);
                std::cout << fmt::format(" {:d} {} {} {} {}", box.page, box.x, box.y, box.w, box.h);
                break;
            }
            case OP_MVBOX:
            case OP_GETBOX:
                std::cout << ' ' << spacer_index_names[int(line.get_args<OP_MVBOX>())];
                break;
            case OP_CALL: {
                auto args = line.get_args<OP_CALL>();
                std::cout << ' ' << args.fun->first << ' ' << int(args.numargs);
                break;
            }
            case OP_SELVAR: {
                auto args = line.get_args<OP_SELVAR>();
                std::cout << ' ' << args.name << ' ' << int(args.index);
                if (args.length != 1) {
                    std::cout << ':' << int(args.length);
                }
                std::cout << print_flags(args.flags, selvar_flags_names);
                break;
            }
            case OP_SETVAR: {
                auto flags = line.get_args<OP_SETVAR>();
                std::cout << print_flags(flags, setvar_flags_names);
                break;
            }
            case OP_PUSHNUM:
                std::cout << ' ' << fixed_point_to_string(line.get_args<OP_PUSHNUM>());
                break;
            case OP_PUSHINT:
                std::cout << ' ' << line.get_args<OP_PUSHINT>();
                break;
            case OP_PUSHSTR:
                std::cout << ' ' << quoted_string(line.get_args<OP_PUSHSTR>());
                break;
            case OP_PUSHARG:
                std::cout << ' ' << int(line.get_args<OP_PUSHARG>());
                break;
            case OP_SETLANG:
                std::cout << ' ' << intl::language_string(line.get_args<OP_SETLANG>());
                break;
            case OP_IMPORT: {
                auto args = line.get_args<OP_IMPORT>();
                std::cout << ' ' << quoted_string(args.filename.string());
                std::cout << print_flags(args.flags, import_flags_names);
                break;
            }
            case OP_JMP:
            case OP_JZ:
            case OP_JNZ:
            case OP_JNTE: {
                auto addr = line.get_args<OP_JMP>();
                if (auto jt = inv_labels.find(i + addr); jt != inv_labels.end()) {
                    std::cout << ' ' << jt->second;
                } else {
                    std::cout << ' ' << addr;
                }
                break;
            }
            case OP_JSR: {
                auto addr = line.get_args<OP_JSR>();
                if (auto jt = inv_labels.find(i + addr.addr); jt != inv_labels.end()) {
                    std::cout << ' ' << jt->second;
                } else {
                    std::cout << ' ' << addr.addr;
                }
                std::cout << ' ' << int(addr.numargs);
                break;
            }
            default:
                break;
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