#include <iostream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

#include <json/value.h>

#include "parser.h"
#include "fixed_point.h"
#include "utils.h"
#include "intl.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    std::filesystem::path input_bls;

    bool skip_comments;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls");
    parser.AddSwitch("s", "skip-comments", "Skip Comments");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    input_bls = parser.GetParam(0).ToStdString();
    skip_comments = parser.FoundSwitch("s") == wxCMD_SWITCH_ON;
    return true;
}

std::string quoted_string(const std::string &str) {
    std::string ret = Json::Value(str).toStyledString();
    string_trim(ret);
    return ret;
}

int MainApp::OnRun() {
    try {
        parser my_parser;
        if (!skip_comments) {
            my_parser.add_flags(PARSER_ADD_COMMENTS);
        }
        my_parser.add_flags(PARSER_NO_EVAL_JUMPS);
        my_parser.read_layout(bill_layout_script::from_file(input_bls));

        std::multimap<size_t, std::string> inv_labels;
        for (auto &[label, addr] : my_parser.get_labels()) {
            inv_labels.emplace(addr, label);
        }

        const auto &code = my_parser.get_bytecode();
        for (auto it = code.begin(); it != code.end(); ++it) {
            auto [label_begin, label_end] = inv_labels.equal_range(it - code.begin());
            for (;label_begin != label_end; ++label_begin) {
                std::cout << label_begin->second << ':' << std::endl;
            }
            auto &line = *it;
            switch (line.command()) {
            case opcode::RDBOX: {
                auto box = line.get_args<opcode::RDBOX>();
                std::cout << '\t' << opcode_names[int(line.command())];
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                std::cout << ' ' << int(box.page) << ' ' << box.x << ' ' << box.y << ' ' << box.w << ' ' << box.h;
                break;
            }
            case opcode::MVBOX:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << spacer_index_names[int(line.get_args<opcode::MVBOX>())];
                break;
            case opcode::CALL: {
                auto args = line.get_args<opcode::CALL>();
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << args.name << ' ' << int(args.numargs);
                break;
            }
            case opcode::SELVAR: {
                auto args = line.get_args<opcode::SELVAR>();
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << args.name << ' ' << int(args.index);
                if (args.length != 1) {
                    std::cout << ':' << int(args.length);
                }
                for (uint8_t i=0; i<std::size(variable_select_flag_names); ++i) {
                    if (args.flags & (1 << i)) {
                        std::cout << ' ' << variable_select_flag_names[i];
                    }
                }
                break;
            }
            case opcode::SETVAR: {
                auto flags = line.get_args<opcode::SETVAR>();
                std::cout << '\t' << opcode_names[int(line.command())];
                for (uint8_t i=0; i<std::size(setvar_flags_names); ++i) {
                    if (flags & (1 << i)) {
                        std::cout << ' ' << setvar_flags_names[i];
                    }
                }
                break;
            }
            case opcode::PUSHNUM:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << line.get_args<opcode::PUSHNUM>();
                break;
            case opcode::PUSHSTR:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << quoted_string(line.get_args<opcode::PUSHSTR>());
                break;
            case opcode::SETLANG:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << intl::language_string(line.get_args<opcode::SETLANG>());
                break;
            case opcode::IMPORT:
            case opcode::SETLAYOUT:
                std::cout << '\t' << opcode_names[int(line.command())] << ' ' << quoted_string(line.get_args<opcode::IMPORT>().string());
                break;
            case opcode::UNEVAL_JUMP: {
                auto args = line.get_args<opcode::UNEVAL_JUMP>();
                std::cout << opcode_names[int(args.cmd)] << ' ' << args.label;
                break;
            }
            case opcode::JMP:
            case opcode::JSR:
            case opcode::JZ:
            case opcode::JNZ:
            case opcode::JTE: {
                auto addr = line.get_args<opcode::JMP>();
                if (auto jt = inv_labels.find(it - code.begin() + addr); jt != inv_labels.end()) {
                    std::cout << '\t' << opcode_names[int(line.command())] << ' ' << jt->second;
                } else {
                    std::cout << '\t' << opcode_names[int(line.command())] << ' ' << addr;
                }
                break;
            }
            case opcode::COMMENT:
                std::cout << line.get_args<opcode::COMMENT>();
                break;
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