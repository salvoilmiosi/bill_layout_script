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

std::string get_file_layout(const std::string &app_dir, const std::string &pdf, const std::string &controllo) {
    static std::string layout_dir = controllo.substr(0, controllo.find_last_of("\\/"));

    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/layout_reader", app_dir.c_str());

    const char *args[] = {
        cmd_str,
        pdf.c_str(), controllo.c_str(),
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::istringstream iss(output);

    Json::Value json_output;
    if (iss >> json_output) {
        auto &json_values = json_output["values"];
        for (Json::ValueConstIterator it = json_values.begin(); it != json_values.end(); ++it) {
            auto &obj = *it;
            std::string layout_filename = obj[0].asString();
            return layout_dir + '/' + layout_filename;
        }
    }

    return "";
}

void read_file(std::ostream &out, const std::string &app_dir, const std::string &pdf, const std::string &layout) {
    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/layout_reader", app_dir.c_str());

    const char *args[] = {
        cmd_str,
        pdf.c_str(), layout.c_str(),
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::istringstream iss(output);

    Json::Value json_output;
    if (iss >> json_output) {
        auto &json_values = json_output["values"];
        out << pdf << COMMA;
        out << json_values["numero_fattura"][0].asString() << COMMA;
        out << json_values["codice_pod"][0].asString() << COMMA;
        out << json_values["numero_cliente"][0].asString() << COMMA;
        out << json_values["ragione_sociale"][0].asString() << COMMA;
        out << json_values["totale_fattura"][0].asString() << COMMA;
        out << json_values["periodo"][0].asString() << COMMA;
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

        if (json_values["ricalcoli"]) {
            out << COMMA << "CONGUAGLIO";
        }

        out << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cout << "Utilizzo: layout_batch input_directory controllo.layout output" << std::endl;
        return 0;
    }

    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    std::string input_directory = argv[1];
    std::string controllo_layout = argv[2];
    std::string output_file = argv[3];

    if (!fs::exists(input_directory)) {
        std::cerr << "La directory " << input_directory << " non esiste" << std::endl;
        return 1;
    }

    if (!fs::exists(controllo_layout)) {
        std::cerr << "Il file di layout " << controllo_layout << " non esiste" << std::endl;
        return 1;
    }

    std::unique_ptr<std::ostream> out;
    if (output_file == "-") {
        out.reset(&std::cout);
    } else {
        out.reset(new std::ofstream(output_file));
        if (out->bad()) {
            std::cerr << "Impossibile aprire il file " << output_file << " in output" << std::endl;
            return 1;
        }
    }

    *out << "Nome file" << COMMA << "Numero Fattura" << COMMA << "Codice POD" << COMMA << "Numero cliente" << COMMA << "Ragione sociale" << COMMA <<
        "Totale fattura" << COMMA << "Periodo fattura" << COMMA <<  "Spesa materia energia" << COMMA << "Trasporto gestione" << COMMA <<
        "Oneri di sistema" << COMMA << "F1" << COMMA << "F2" << COMMA << "F3" << COMMA <<
        "R1" << COMMA << "R2" << COMMA << "R3" << COMMA << "Potenza" << COMMA << "Imponibile" << std::endl;

    for (auto &p : fs::recursive_directory_iterator(input_directory)) {
        if (p.is_regular_file()) {
            std::string ext = string_tolower(p.path().extension().string());
            if (ext == ".pdf") {
                std::string pdf_file = p.path().string();
                std::string file_layout = get_file_layout(app_dir, pdf_file, controllo_layout);
                if (file_layout.empty()) {
                    *out << pdf_file << COMMA << "Impossibile trovare il layout per questo file" << std::endl;
                } else {
                    read_file(*out, app_dir, pdf_file, file_layout);
                }
            }
        }
    }

    out->flush();

    return 0;
}