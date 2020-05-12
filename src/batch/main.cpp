#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>

#include <json/json.h>
#include "../shared/pipe.h"

namespace fs = std::filesystem;

static constexpr char COMMA = ';';

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
        out << json_values["numero_cliente"][0].asString() << COMMA;
        out << json_values["ragione_sociale"][0].asString() << COMMA;
        out << json_values["totale_fattura"][0].asString() << COMMA;
        out << json_values["data_fattura"][0].asString() << COMMA;
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
        std::cout << "Utilizzo: layout_batch input_directory file.layout output" << std::endl;
        return 0;
    }

    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    std::string input_directory = argv[1];
    std::string input_layout = argv[2];
    std::string output_file = argv[3];

    if (!fs::exists(input_directory)) {
        std::cerr << "La directory " << input_directory << " non esiste" << std::endl;
        return 1;
    }

    if (!fs::exists(input_layout)) {
        std::cerr << "Il file di layout " << input_layout << " non esiste" << std::endl;
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

    *out << "Nome file" << COMMA << "Numero fattura" << COMMA << "Numero cliente" << COMMA << "Ragione sociale" << COMMA << "Totale fattura" << COMMA << "Periodo fattura" << COMMA << 
        "Spesa materia energia" << COMMA << "Trasporto gestione" << COMMA << "Oneri di sistema" << COMMA << "F1" << COMMA << "F2" << COMMA << "F3" << COMMA <<
        "R1" << COMMA << "R2" << COMMA << "R3" << COMMA << "Potenza" << COMMA << "Imponibile" << std::endl;

    for (auto &p : fs::recursive_directory_iterator(input_directory)) {
        if (p.is_regular_file()) {
            std::string ext = p.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (ext == ".pdf") {
                read_file(*out, app_dir, p.path().string(), input_layout);
            }
        }
    }

    out->flush();

    return 0;
}