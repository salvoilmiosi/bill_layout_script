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

static std::ostream &operator << (std::ostream &out, const variable_name &name) {
    out << name.name;
    if (name.global) {
        out << " GLOBAL";
    }
    return out;
}

int MainApp::OnRun() {
    try {
        parser my_parser;
        if (!skip_comments) {
            my_parser.add_flags(FLAGS_DEBUG);
        }
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
            if (line.command() == opcode::COMMENT) {
                std::cout << line.get<std::string>() << std::endl;
                continue;
            }
            std::cout << '\t' << opcode_names[int(line.command())];
            switch (line.command()) {
            case opcode::RDBOX: {
                auto box = line.get<pdf_rect>();
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                std::cout << ' ' << int(box.page) << ' ' << box.x << ' ' << box.y << ' ' << box.w << ' ' << box.h;
                break;
            }
            case opcode::MVBOX:
                std::cout << ' ' << spacer_index_names[int(line.get<spacer_index>())];
                break;
            case opcode::CALL: {
                auto args = line.get<command_call>();
                std::cout << ' ' << args.name << ' ' << int(args.numargs);
                break;
            }
            case opcode::SELVAR: {
                auto args = line.get<variable_selector>();
                std::cout << ' ' << args.name << ' ' << int(args.index);
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
            case opcode::ISSET:
            case opcode::GETSIZE:
                std::cout << ' ' << line.get<variable_name>();
                break;
            case opcode::PUSHNUM:
                std::cout << ' ' << line.get<fixed_point>();
                break;
            case opcode::PUSHSTR:
                std::cout << ' ' << quoted_string(line.get<std::string>());
                break;
            case opcode::SETLANG:
                std::cout << ' ' << intl::language_string(line.get<int>());
                break;
            case opcode::IMPORT:
            case opcode::SETLAYOUT:
                std::cout << ' ' << quoted_string(line.get<std::filesystem::path>().string());
                break;
            case opcode::JMP:
            case opcode::JSR:
            case opcode::JZ:
            case opcode::JNZ:
            case opcode::JTE:
                std::cout << ' ' << line.get<jump_address>().label;
                break;
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