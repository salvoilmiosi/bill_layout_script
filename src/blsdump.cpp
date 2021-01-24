#include <iostream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

#include <json/value.h>

#include "parser.h"
#include "fixed_point.h"
#include "utils.h"

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

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_SWITCH, "s", "skip-comments", "Skip Comments", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input-bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) {
        input_bls = parser.GetParam(0).ToStdString();
    }
    skip_comments = parser.Found("s");
    return true;
}

#define O(x) #x
static const char *opcode_names[] = OPCODES;
#undef O

static const char *spacer_index_names[] = {
    "PAGE", "X", "Y", "W", "H"
};

std::string quoted_string(const std::string &str) {
    std::string ret = Json::Value(str).toStyledString();
    string_trim(ret);
    return ret;
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
            case opcode::RDBOX:
            {
                auto box = line.get<pdf_rect>();
                std::cout << ' ' << read_mode_strings[int(box.mode)];
                std::cout << ' ' << box_type_strings[int(box.type)];
                std::cout << ' ' << int(box.page) << ' ' << box.x << ' ' << box.y << ' ' << box.w << ' ' << box.h;
                break;
            }
            case opcode::MVBOX:
                std::cout << ' ' << spacer_index_names[int(line.get<spacer_index>())];
                break;
            case opcode::CALL:
            {
                auto args = line.get<command_call>();
                std::cout << ' ' << args.name << ' ' << int(args.numargs);
                break;
            }
            case opcode::SELVAR:
            {
                auto args = line.get<variable_idx>();
                std::cout << ' ' << args.name << ' ' << int(args.index) << ':' << int(args.range_len);
                break;
            }
            case opcode::SELVARTOP:
            case opcode::SELRANGETOP:
            case opcode::SELRANGEALL:
                std::cout << ' ' << line.get<std::string>();
                break;
            case opcode::PUSHNUM:
                std::cout << ' ' << line.get<fixed_point>();
                break;
            case opcode::PUSHSTR:
            case opcode::IMPORT:
            case opcode::SETLAYOUT:
            case opcode::SETLANG:
                std::cout << ' ' << quoted_string(line.get<std::string>());
                break;
            case opcode::JMP:
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