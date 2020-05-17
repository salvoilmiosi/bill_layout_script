#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>

#include <json/json.h>
#include "../shared/pipe.h"
#include "../shared/utils.h"

namespace fs = std::filesystem;

static constexpr char COMMA = ';';

void read_file(std::ostream &out, const std::string &app_dir, const std::string &pdf, const std::string &layout_script, const std::string &layout_dir) {
    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/layout_reader", app_dir.c_str());

    const char *args[] = {
        cmd_str,
        pdf.c_str(), "-",
        "-f", layout_dir.c_str(),
        nullptr
    };

    auto proc = open_process(args);
    proc->write_all(layout_script);
    proc->close_stdin();

    std::string output = proc->read_all();
    std::istringstream iss(output);

    try {
        Json::Value json_output;
        iss >> json_output;

        auto &json_values = json_output["values"];
        out << pdf << COMMA;
        out << json_values["numero_fattura"][0].asString() << COMMA;
        out << json_values["codice_pod"][0].asString() << COMMA;
        out << json_values["numero_cliente"][0].asString() << COMMA;
        out << json_values["ragione_sociale"][0].asString() << COMMA;
        out << json_values["periodo"][0].asString() << COMMA;
        out << json_values["totale_fattura"][0].asString() << COMMA;
        out << json_values["spesa_materia_energia"][0].asString() << COMMA;
        out << json_values["trasporto_gestione"][0].asString() << COMMA;
        out << json_values["oneri"][0].asString() << COMMA;
        out << json_values["energia_attiva"][0].asString() << COMMA;
        out << json_values["energia_attiva"][1].asString() << COMMA;
        out << json_values["energia_attiva"][2].asString() << COMMA;
        out << json_values["energia_reattiva"][0].asString() << COMMA;
        out << json_values["energia_reattiva"][1].asString() << COMMA;
        out << json_values["energia_reattiva"][2].asString() << COMMA;
        out << json_values["potenza"][0].asString() << COMMA;
        out << json_values["imponibile"][0].asString();

        if (json_values["conguaglio"]) {
            out << COMMA << "CONGUAGLIO";
        }

        out << std::endl;
    } catch (const std::exception &error) {
        out << pdf << COMMA << "Impossibile leggere questo file" << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cout << "Utilizzo: layout_batch input_directory controllo.txt output" << std::endl;
        return 0;
    }

    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    std::string input_directory = argv[1];
    std::string file_layout = argv[2];
    std::string output_file = argv[3];

    std::string layout_dir = file_layout.substr(0, file_layout.find_last_of("\\/"));

    if (!fs::exists(input_directory)) {
        std::cerr << "La directory " << input_directory << " non esiste" << std::endl;
        return 1;
    }

    if (!fs::exists(file_layout)) {
        std::cerr << "Il file di layout " << file_layout << " non esiste" << std::endl;
        return 1;
    }

    std::ifstream ifs(file_layout);
    std::string layout_string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    std::ostream *out = &std::cout;
    if (output_file != "-") {
        out = new std::ofstream(output_file);
        if (out->bad()) {
            std::cerr << "Impossibile aprire il file " << output_file << " in output" << std::endl;
            return 1;
        }
    }

    *out << "Nome file" << COMMA << "Numero Fattura" << COMMA << "Codice POD" << COMMA << "Numero cliente" << COMMA << "Ragione sociale" << COMMA <<
        "Periodo fattura" << COMMA << "Totale fattura" << COMMA <<  "Spesa materia energia" << COMMA << "Trasporto gestione" << COMMA <<
        "Oneri di sistema" << COMMA << "F1" << COMMA << "F2" << COMMA << "F3" << COMMA <<
        "R1" << COMMA << "R2" << COMMA << "R3" << COMMA << "Potenza" << COMMA << "Imponibile" << std::endl;

    for (auto &p : fs::recursive_directory_iterator(input_directory)) {
        if (p.is_regular_file()) {
            std::string ext = p.path().extension().string();
            string_tolower(ext);
            if (ext == ".pdf") {
                std::string pdf_file = p.path().string();
                read_file(*out, app_dir, pdf_file, layout_string, layout_dir);
            }
        }
    }

    if (auto *ofs = dynamic_cast<std::ofstream *>(out)) {
        ofs->close();
        delete ofs;
    }

    return 0;
}