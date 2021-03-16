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
    parser.AddSwitch("j", "eval-jumps", "Evaluate Jumps");
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
    check_flag(PARSER_NO_EVAL_JUMPS, "j", true);
    check_option(output_cache, "o");
    return true;
}

static std::string quoted_string(const std::string &str) {
    return string_trim(Json::Value(str).toStyledString());
}

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

            auto [comment_begin, comment_end] = comments.equal_range(i);
            for (;comment_begin != comment_end; ++comment_begin) {
                std::cout << comment_begin->second << std::endl;
            }
            auto [label_begin, label_end] = inv_labels.equal_range(i);
            for (;label_begin != label_end; ++label_begin) {
                std::cout << label_begin->second << ':' << std::endl;
            }

            switch (line.command()) {
            case OP_SETBOX: {
                auto box = line.get_args<OP_SETBOX>();
                std::cout << '\t' << opcode_names[int(line.command())];
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                for (size_t j=0; j<std::size(pdf_flags_names); ++j) {
                    if (box.flags & (1 << j)) {
                        std::cout << ' ' << pdf_flags_names[j];
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
                for (size_t j=0; j<std::size(selvar_flags_names); ++j) {
                    if (args.flags & (1 << j)) {
                        std::cout << ' ' << selvar_flags_names[j];
                    }
                }
                break;
            }
            case OP_SETVAR: {
                auto flags = line.get_args<OP_SETVAR>();
                std::cout << '\t' << opcode_names[int(line.command())];
                for (size_t j=0; j<std::size(setvar_flags_names); ++j) {
                    if (flags & (1 << j)) {
                        std::cout << ' ' << setvar_flags_names[j];
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
                for (size_t j=0; j<std::size(import_flags_names); ++j) {
                    if (args.flags & (1 << j)) {
                        std::cout << ' ' << import_flags_names[j];
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
            case OP_JNTE: {
                auto addr = line.get_args<OP_JMP>();
                if (auto jt = inv_labels.find(i + addr); jt != inv_labels.end()) {
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