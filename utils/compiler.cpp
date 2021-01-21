#include <iostream>
#include <fstream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

#include <fmt/format.h>

#include "parser.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    std::filesystem::path output_asm;

    bool no_out = false;
    bool debug = false;
    bool read_asm = false;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_OPTION, "o", "output", "output layout", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "t", "output-asm", "output assembler", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "d", "no-output", "no output", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "g", "debug", "debug", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "s", "input-asm", "input assempler", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    wxString str;

    if (parser.GetParamCount() >= 1) {
        input_file = parser.GetParam(0).ToStdString();
    }
    if (parser.Found("o", &str)) {
        output_file = str.ToStdString();
    }
    if (parser.Found("t", &str)) {
        output_asm = str.ToStdString();
    }
    no_out = parser.Found("d");
    read_asm = parser.Found("s");
    debug = parser.Found("g");
    return true;
}

int MainApp::OnRun() {
    std::ifstream ifs;

    if (input_file != "-") {
        ifs.open(input_file);

        if (!no_out && output_file.empty()) {
            output_file = input_file;
            output_file.replace_extension("out");
        }
    }

    bytecode out_code;

    try {
        if (!read_asm) {
            parser my_parser;
            if (debug) my_parser.add_flags(FLAGS_DEBUG);
            my_parser.read_layout(bill_layout_script::from_stream(ifs.is_open() ? ifs : std::cin));

            auto print_asm = [&](std::ostream &out) {
                for (auto &line : my_parser.get_lines()) {
                    auto cmd = line.substr(0, line.find_first_of(' '));
                    if (cmd != "COMMENT" && cmd != "LABEL") {
                        out << '\t';
                    }
                    out << line << std::endl;
                }
            };

            if (output_asm == "-") {
                print_asm(std::cout);
            } else if (!output_asm.empty()) {
                std::ofstream ofs(output_asm);
                print_asm(ofs);
            }
            
            out_code = my_parser.get_bytecode();
        } else {
            std::vector<std::string> lines;
            std::string line;
            while(std::getline(ifs.is_open() ? ifs : std::cin, line)) {
                lines.push_back(line.substr(line.find_first_not_of(" \t")));
            }
            
            out_code = bytecode::from_lines(lines);
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    
    if (!no_out && !output_file.empty()) {
        if (output_file == "-") {
#ifdef _WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
            std::cout << out_code;
        } else {
            std::ofstream ofs(output_file, std::ofstream::binary | std::ofstream::out);
            ofs << out_code;
        }
    }

    return 0;
}