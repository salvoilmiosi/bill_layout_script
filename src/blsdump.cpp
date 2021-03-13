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

    bool skip_comments;
    bool recursive_imports;
    bool do_eval_jumps;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls");
    parser.AddSwitch("s", "skip-comments", "Skip Comments");
    parser.AddSwitch("r", "recursive-imports", "Recursive Imports");
    parser.AddSwitch("j", "eval-jumps", "Evaluate Jumps");
    parser.AddOption("o", "output-cache", "Output Cache");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    input_bls = parser.GetParam(0).ToStdString();
    skip_comments = parser.FoundSwitch("s") == wxCMD_SWITCH_ON;
    recursive_imports = parser.FoundSwitch("r") == wxCMD_SWITCH_ON;
    do_eval_jumps = parser.FoundSwitch("j") == wxCMD_SWITCH_ON;
    if (wxString str; parser.Found("o", &str)) {
        output_cache = str.ToStdString();
    }
    return true;
}

std::string quoted_string(const std::string &str) {
    return string_trim(Json::Value(str).toStyledString());
}

int MainApp::OnRun() {
    try {
        parser my_parser;
        if (!skip_comments) {
            my_parser.add_flags(PARSER_ADD_COMMENTS);
        }
        if (recursive_imports) {
            my_parser.add_flags(PARSER_RECURSIVE_IMPORTS);
        }
        if (!do_eval_jumps) {
            my_parser.add_flags(PARSER_NO_EVAL_JUMPS);
        }
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
        for (auto it = code.begin(); it != code.end(); ++it) {
            auto [comment_begin, comment_end] = comments.equal_range(it - code.begin());
            for (;comment_begin != comment_end; ++comment_begin) {
                std::cout << comment_begin->second << std::endl;
            }
            auto [label_begin, label_end] = inv_labels.equal_range(it - code.begin());
            for (;label_begin != label_end; ++label_begin) {
                std::cout << label_begin->second << ':' << std::endl;
            }
            auto &line = *it;
            switch (line.command()) {
            case OP_RDBOX: {
                auto box = line.get_args<OP_RDBOX>();
                std::cout << '\t' << opcode_names[int(line.command())];
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                for (size_t i=0; i<std::size(pdf_flags_names); ++i) {
                    if (box.flags & (1 << i)) {
                        std::cout << ' ' << pdf_flags_names[i];
                    }
                }
                std::cout << ' ' << int(box.page) << ' ' << box.x << ' ' << box.y << ' ' << box.w << ' ' << box.h;
                break;
            }
            case OP_MVBOX:
            case OP_GETBOX:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << spacer_index_names[int(line.get_args<OP_MVBOX>())];
                break;
            case OP_CALL: {
                auto args = line.get_args<OP_CALL>();
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << args.fun->first << ' ' << int(args.numargs);
                break;
            }
            case OP_SELVAR: {
                auto args = line.get_args<OP_SELVAR>();
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << args.name << ' ' << int(args.index);
                if (args.length != 1) {
                    std::cout << ':' << int(args.length);
                }
                for (size_t i=0; i<std::size(selvar_flags_names); ++i) {
                    if (args.flags & (1 << i)) {
                        std::cout << ' ' << selvar_flags_names[i];
                    }
                }
                break;
            }
            case OP_SETVAR: {
                auto flags = line.get_args<OP_SETVAR>();
                std::cout << '\t' << opcode_names[int(line.command())];
                for (size_t i=0; i<std::size(setvar_flags_names); ++i) {
                    if (flags & (1 << i)) {
                        std::cout << ' ' << setvar_flags_names[i];
                    }
                }
                break;
            }
            case OP_PUSHNUM:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << fixed_point_to_string(line.get_args<OP_PUSHNUM>());
                break;
            case OP_PUSHSTR:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << quoted_string(line.get_args<OP_PUSHSTR>());
                break;
            case OP_SETLANG:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << intl::language_string(line.get_args<OP_SETLANG>());
                break;
            case OP_IMPORT: {
                auto args = line.get_args<OP_IMPORT>();
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << quoted_string(args.filename.string());
                for (size_t i=0; i<std::size(import_flags_names); ++i) {
                    if (args.flags & (1 << i)) {
                        std::cout << ' ' << import_flags_names[i];
                    }
                }
                break;
            }
            case OP_UNEVAL_JUMP: {
                auto args = line.get_args<OP_UNEVAL_JUMP>();
                std::cout << '\t' << opcode_names[int(args.cmd)] << ' ' << args.label;
                break;
            }
            case OP_JMP:
            case OP_JSR:
            case OP_JZ:
            case OP_JNZ:
            case OP_JTE: {
                auto addr = line.get_args<OP_JMP>();
                if (auto jt = inv_labels.find(it - code.begin() + addr); jt != inv_labels.end()) {
                    std::cout << '\t' << opcode_names[int(line.command())] << ' ' << jt->second;
                } else {
                    std::cout << '\t' << opcode_names[int(line.command())] << ' ' << addr;
                }
                break;
            }
            default:
                std::cout << '\t' << opcode_names[int(line.command())];
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