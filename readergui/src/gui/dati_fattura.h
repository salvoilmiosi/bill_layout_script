#ifndef __DATI_FATTURA_H__
#define __DATI_FATTURA_H__

#include <string>
#include <tuple>
#include <optional>

#include "fixed_point.h"
#include "variable_ref.h"

#include "data_table_view.h"

template<typename T> inline wxString to_string(const T &value) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

template<> inline wxString to_string(const std::string &value) { return value; }
template<> inline wxString to_string(const fixed_point &value) { return fixed_point_to_string(value); }
template<> inline wxString to_string(const wxDateTime &value) { return value.Format("%d/%m/%Y"); }

template<typename T>
struct table_value {
    const char *header;
    const char *value;
    int index;
    int column_width = 80;
    
    std::function<wxString(const T&)> formatter = to_string<T>;
};

using string_table_value = table_value<std::string>;
using number_table_value = table_value<fixed_point>;
using date_table_value = table_value<wxDateTime>;

struct month_table_value : date_table_value {
    template<typename ... Ts>
    month_table_value(Ts && ... args) : date_table_value{std::forward<Ts>(args) ...} {
        formatter = [](const wxDateTime &value) {
            return value.Format("%b %Y");
        };
    }
};

struct number_unit_table_value : number_table_value {
    template<typename ... Ts>
    number_unit_table_value(const wxString &unit, Ts && ... args) : number_table_value{std::forward<Ts>(args) ...} {
        formatter = [=](const fixed_point &value) {
            return wxString::Format(L"%s %s", fixed_point_to_string(value), unit);
        };
    }
};

struct percent_table_value : number_table_value {
    template<typename ... Ts> percent_table_value(Ts && ... args) : number_table_value(std::forward<Ts>(args) ...) {
        formatter = [](const fixed_point &value) {
            return fixed_point_to_string(value) + "%";
        };
    }
};

struct euro_table_value : number_unit_table_value {
    template<typename ... Ts> euro_table_value(Ts && ... args) : number_unit_table_value(L"\u20ac", std::forward<Ts>(args) ...) {}
};
struct euro_kwh_table_value : number_unit_table_value {
    template<typename ... Ts> euro_kwh_table_value(Ts && ... args) : number_unit_table_value(L"\u20ac/kWh", std::forward<Ts>(args) ...) {}
};
struct kwh_table_value : number_unit_table_value {
    template<typename ... Ts> kwh_table_value(Ts && ... args) : number_unit_table_value("kWh", std::forward<Ts>(args) ...) {}
};
struct kvar_table_value : number_unit_table_value {
    template<typename ... Ts> kvar_table_value(Ts && ... args) : number_unit_table_value("kVar", std::forward<Ts>(args) ...) {}
};
struct kw_table_value : number_unit_table_value {
    template<typename ... Ts> kw_table_value(Ts && ... args) : number_unit_table_value("kW", std::forward<Ts>(args) ...) {}
};

struct variable_table_record : std::multimap<std::string, variable> {
    wxString filename;
    wxString status;
};

template<typename T, typename TVal>
struct member_ptr_value {
    const char *header;
    T TVal::* value;
    int column_width = 80;
};

static inline std::tuple dati_fattura {
    member_ptr_value    {"Nome File",               &variable_table_record::filename},
    string_table_value  {"Ragione Sociale",         "ragione_sociale"},
    string_table_value  {"Indirizzo Fornitura",     "indirizzo_fornitura"},
    string_table_value  {"POD",                     "codice_pod"},
    month_table_value   {"Mese",                    "mese_fattura"},
    string_table_value  {"Fornitore",               "fornitore"},
    string_table_value  {"N. Fatt.",                "numero_fattura"},
    date_table_value    {"Data Emissione",          "data_fattura"},
    date_table_value    {"Data Scadenza",           "data_scadenza"},
    euro_table_value    {"Costo Materia Energia",   "spesa_materia_energia"},
    euro_table_value    {"Trasporto",               "trasporto_gestione"},
    euro_table_value    {"Oneri",                   "oneri"},
    euro_table_value    {"Accise",                  "accise"},
    percent_table_value {"Iva",                     "iva"},
    euro_table_value    {"Imponibile",              "imponibile"},
    kwh_table_value     {"F1 Rilev.",               "energia_attiva_rilevata", 0},
    kwh_table_value     {"F2 Rilev.",               "energia_attiva_rilevata", 1},
    kwh_table_value     {"F3 Rilev.",               "energia_attiva_rilevata", 2},
    kwh_table_value     {"F1",                      "energia_attiva", 0},
    kwh_table_value     {"F2",                      "energia_attiva", 1},
    kwh_table_value     {"F3",                      "energia_attiva", 2},
    kw_table_value      {"P1",                      "potenza", 0},
    kw_table_value      {"P2",                      "potenza", 1},
    kw_table_value      {"P3",                      "potenza", 2},
    kvar_table_value    {"R1",                      "energia_reattiva", 0},
    kvar_table_value    {"R2",                      "energia_reattiva", 1},
    kvar_table_value    {"R3",                      "energia_reattiva", 2},
    euro_table_value    {"Oneri Ammin.",            "oneri_amministrativi"},
    euro_table_value    {"PCV",                     "pcv"},
    euro_table_value    {"CTS",                     "cts"},
    euro_table_value    {"<75%",                    "penale_reattiva_inf75"},
    euro_table_value    {">75%",                    "penale_reattiva_sup75"},
    euro_kwh_table_value{"PEF1",                    "prezzo_energia", 0},
    euro_kwh_table_value{"PEF2",                    "prezzo_energia", 1},
    euro_kwh_table_value{"PEF3",                    "prezzo_energia", 2},
    euro_kwh_table_value{"Sbilanciamento",          "sbilanciamento"},
    euro_kwh_table_value{"Dispacciamento",          "dispacciamento"},
    euro_kwh_table_value{"Disp. Var",               "disp_var"}
};

template<typename T> inline T variable_to(const variable &var) = delete;
template<> inline std::string variable_to(const variable &var) { return var.as_string(); }
template<> inline fixed_point variable_to(const variable &var) { return var.as_number(); }
template<> inline wxDateTime variable_to(const variable &var) { return var.as_date(); }

class VariableMapTable : public DataTableView<variable_table_record> {
public:
    VariableMapTable(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize,
        const wxValidator &validator = wxDefaultValidator) :
            DataTableView<variable_table_record>(parent, id, position, size, validator) {
        auto addVariableColumn = overloaded{
            [&]<typename T>(const table_value<T> &value) {
                AddColumn<OptionalValueColumn<T, decltype(value.formatter)>>(value.header, value.column_width,
                [=](const variable_table_record &map) -> std::optional<T> {
                    auto [lower, upper] = map.equal_range(value.value);
                    if (value.index < std::distance(lower, upper)) {
                        const auto &var = std::next(lower, value.index)->second;
                        if (!var.is_null()) {
                            return variable_to<T>(var);
                        }
                    }
                    return std::nullopt;
                }, value.formatter);
            },
            [&]<typename T, typename TVal>(const member_ptr_value<T, TVal> &value) {
                AddColumn<T>(value.header, value.column_width, value.value);
            }
        };
        [&]<size_t ... Is>(std::index_sequence<Is...>) {
            (addVariableColumn(std::get<Is>(dati_fattura)), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(dati_fattura)>>{});
    }

    void AddReaderValues(const wxString &path, const variable_map &values) {
        variable_table_record current;
        current.filename = path;
        size_t table_idx = 0;
        for (const auto &[key, value] : values) {
            if (key.table_index == variable_key::global_index) continue;

            current.emplace(key.name, value);
            if (key.table_index != table_idx) {
                AddItem(std::move(current));
                current.clear();
                table_idx = key.table_index;
            }
        }
        AddItem(std::move(current));
    }
};

#endif