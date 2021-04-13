#ifndef __DATI_FATTURA_H__
#define __DATI_FATTURA_H__

#include <string>
#include <tuple>
#include <optional>

#include "fixed_point.h"
#include "variable_ref.h"

#include "data_table_view.h"

template<typename T>
struct table_value {
    const char *header;
    const char *value;
    int index;
    int column_width = 120;
};

using string_table_value = table_value<std::string>;
using number_table_value = table_value<fixed_point>;
using date_table_value = table_value<wxDateTime>;

constexpr std::tuple dati_fattura {
    string_table_value{"Ragione Sociale",       "ragione_sociale"},
    string_table_value{"Indirizzo Fornitura",   "indirizzo_fornitura"},
    string_table_value{"POD",                   "codice_pod"},
    date_table_value  {"Mese",                  "mese_fattura"},
    string_table_value{"Fornitore",             "fornitore"},
    string_table_value{"N. Fatt.",              "numero_fattura"},
    date_table_value  {"Data Emissione",        "data_fattura"},
    date_table_value  {"Data Scadenza",         "data_scadenza"},
    number_table_value{"Costo Materia Energia", "spesa_materia_energia"},
    number_table_value{"Trasporto",             "trasporto_gestione"},
    number_table_value{"Oneri",                 "oneri"},
    number_table_value{"Accise",                "accise"},
    number_table_value{"Iva",                   "iva"},
    number_table_value{"Imponibile",            "imponibile"},
    number_table_value{"F1 Rilev.",             "energia_attiva_rilevata", 0},
    number_table_value{"F2 Rilev.",             "energia_attiva_rilevata", 1},
    number_table_value{"F3 Rilev.",             "energia_attiva_rilevata", 2},
    number_table_value{"F1",                    "energia_attiva", 0},
    number_table_value{"F2",                    "energia_attiva", 1},
    number_table_value{"F3",                    "energia_attiva", 2},
    number_table_value{"P1",                    "potenza", 0},
    number_table_value{"P2",                    "potenza", 1},
    number_table_value{"P3",                    "potenza", 2},
    number_table_value{"R1",                    "energia_reattiva", 0},
    number_table_value{"R2",                    "energia_reattiva", 1},
    number_table_value{"R3",                    "energia_reattiva", 2},
    number_table_value{"Oneri Ammin.",          "oneri_amministrativi"},
    number_table_value{"PCV",                   "pcv"},
    number_table_value{"CTS",                   "cts"},
    number_table_value{"<75%",                  "penale_reattiva_inf75"},
    number_table_value{">75%",                  "penale_reattiva_sup75"},
    number_table_value{"PEF1",                  "prezzo_energia", 0},
    number_table_value{"PEF2",                  "prezzo_energia", 1},
    number_table_value{"PEF3",                  "prezzo_energia", 2},
    number_table_value{"Sbilanciamento",        "sbilanciamento"},
    number_table_value{"Dispacciamento",        "dispacciamento"},
    number_table_value{"Disp. Var",             "disp_var"}
};

template<typename T> inline T variable_to(const variable &var) = delete;
template<> inline std::string variable_to(const variable &var) { return var.as_string(); }
template<> inline fixed_point variable_to(const variable &var) { return var.as_number(); }
template<> inline wxDateTime variable_to(const variable &var) { return var.as_date(); }

template<typename T> auto value_getter_lambda(const table_value<T> &value) {
    return [=](const variable_map &map) -> std::optional<T> {
        auto [lower, upper] = map.equal_range(variable_key{value.value, 0});
        if (std::distance(lower, upper) < value.index) {
            const auto &var = std::next(lower, value.index)->second;
            if (!var.is_null()) {
                return variable_to<T>(var);
            }
        }
        return std::nullopt;
    };
}

class VariableMapTable : public DataTableView<variable_map> {
public:
    VariableMapTable(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize,
        const wxValidator &validator = wxDefaultValidator) :
            DataTableView<variable_map>(parent, id, position, size, validator) {
        auto addVariableColumn = [&]<typename T>(const table_value<T> &value) {
            AddColumn<OptionalValueColumn<T>>(value.header, value.column_width, value_getter_lambda(value));
        };
        [&]<size_t ... Is>(std::index_sequence<Is...>) {
            (addVariableColumn(std::get<Is>(dati_fattura)), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(dati_fattura)>>{});
    }
};

#endif