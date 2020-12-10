#include <iostream>
#include <fstream>

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>

#include "parser.h"
#include "assembler.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    wxString input_file;
    wxString output_file;
    bool quiet = false;
    bool debug = false;
    bool read_asm = false;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_OPTION, "o", "output", "output layout", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "d", "debug", "debug", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "q", "quiet", "quiet mode", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "s", "assembler", "assembler", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) input_file = parser.GetParam(0);
    parser.Found("o", &output_file);
    debug = parser.Found("d");
    quiet = parser.Found("q");
    read_asm = parser.Found("s");
    return true;
}

int MainApp::OnRun() {
    std::ifstream ifs;

    if (input_file != "-") {
        if (!wxFileExists(input_file)) {
            std::cerr << "Impossibile aprire il file layout " << input_file << std::endl;
            return 1;
        }
        ifs.open(input_file.ToStdString());

        if (!debug && output_file.empty()) {
            wxFileName f(input_file);
            f.SetExt("out");
            output_file = f.GetFullPath();
        }
    }

    bytecode out_code;

    try {
        if (!read_asm) {
            bill_layout_script layout;
            if (ifs.is_open()) {
                ifs >> layout;
                ifs.close();
            } else {
                std::cin >> layout;
            }

            parser my_parser;
            my_parser.read_layout(layout);

            if (!quiet) {
                for (auto &line : my_parser.get_output_asm()) {
                    std::cout << line << std::endl;
                }
            }
            
            out_code = read_lines(my_parser.get_output_asm());
        } else {
            std::vector<std::string> lines;
            std::string line;
            while(std::getline(ifs.is_open() ? ifs : std::cin, line)) {
                lines.push_back(line);
            }
            
            out_code = read_lines(lines);
        }
    } catch (const layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const assembly_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    }
    
    if (!debug && !output_file.empty()) {
        if (output_file == "-") {
#ifdef _WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
            out_code.write_bytecode(std::cout);
        } else {
            std::ofstream ofs(output_file.ToStdString(), std::ofstream::binary | std::ofstream::out);
            out_code.write_bytecode(ofs);
        }
    }

    return 0;
}