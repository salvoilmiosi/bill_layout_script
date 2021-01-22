#include <iostream>
#include <fstream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

#include "parser.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    std::filesystem::path input_bls;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
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
    return true;
}

int MainApp::OnRun() {
    try {
        parser my_parser;
        my_parser.add_flags(FLAGS_DEBUG);
        my_parser.read_layout(bill_layout_script::from_file(input_bls));
        for (const auto &line : my_parser.get_lines()) {
            if (!line.starts_with("COMMENT") && ! line.starts_with("LABEL")) {
                std::cout << '\t';
            }
            std::cout << line << std::endl;
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