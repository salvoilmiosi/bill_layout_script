#ifndef __DATI_FATTURA_H__
#define __DATI_FATTURA_H__

#include <string>
#include <tuple>
#include <optional>

#include "fixed_point.h"
#include "variable_ref.h"

#include "data_table_view.h"

struct default_formatter {
    template<typename T> wxString operator()(const T &value) {
        std::ostringstream ss;
        ss << value;
        return ss.str();
    }
    wxString operator()(const std::string &value) { return wxString::FromUTF8(value.c_str()); }
    wxString operator()(const fixed_point &value) { return fixed_point_to_string(value); }
    wxString operator()(const wxDateTime &value) { return value.Format("%d/%m/%Y"); }
    wxString operator()(const std::vector<std::string> &value) {
        return (*this)(string_join(value, "; "));
    }
};

template<typename T> inline T variable_to(const variable &var) = delete;
template<> inline std::string variable_to(const variable &var) { return var.as_string(); }
template<> inline fixed_point variable_to(const variable &var) { return var.as_number(); }
template<> inline wxDateTime variable_to(const variable &var) { return var.as_date(); }

struct table_value {
    const char *header;
    int column_width = 80;
    bool required = false;
};

struct variable_table_record : std::multimap<std::string, variable> {
    std::string filename;
    std::vector<std::string> status;
};

template<typename T, typename Formatter = default_formatter>
struct variable_table_value :  table_value {
    using value_type = T;
    using formatter_type = Formatter;

    const char *value;
    int index;

    variable_table_value(auto &&args, const char *value, int index = 0)
        : table_value{std::forward<decltype(args)>(args)}
        , value(value)
        , index(index) {}
    
    std::optional<T> operator()(const variable_table_record &map) const {
        auto [lower, upper] = map.equal_range(value);
        if (index < std::distance(lower, upper)) {
            const auto &var = std::next(lower, index)->second;
            if (!var.is_null()) {
                return variable_to<T>(var);
            }
        }
        return std::nullopt;
    }
};

template<typename T, typename TVal, typename Formatter = default_formatter>
struct member_ptr_value : table_value {
    using value_type = T;
    using formatter_type = Formatter;

    T TVal::* value;

    member_ptr_value(auto &&args, T TVal::* value)
        : table_value{std::forward<decltype(args)>(args)}
        , value(value) {}

    std::optional<T> operator()(const TVal &obj) const {
        return obj.*value;
    }
};

template<typename Formatter = default_formatter>
using string_table_value = variable_table_value<std::string, Formatter>;

template<typename Formatter = default_formatter>
using date_table_value = variable_table_value<wxDateTime, Formatter>;

template<typename Formatter = default_formatter>
using number_table_value = variable_table_value<fixed_point, Formatter>;

using month_table_value = date_table_value<decltype([](const wxDateTime &value) {
    return value.Format("%b %Y");
})>;

using percent_table_value = number_table_value<decltype([](const fixed_point &value) {
    return fixed_point_to_string(value) + "%";
})>;

using euro_table_value = number_table_value<decltype([](const fixed_point &value) {
    return wxString::Format(L"%s \u20ac", fixed_point_to_string(value));
})>;
using euro_kwh_table_value = number_table_value<decltype([](const fixed_point &value) {
    return wxString::Format(L"%s \u20ac/kWh", fixed_point_to_string(value));
})>;
using kwh_table_value = number_table_value<decltype([](const fixed_point &value) {
    return wxString::Format(L"%s kWh", fixed_point_to_string(value));
})>;
using kvar_table_value = number_table_value<decltype([](const fixed_point &value) {
    return wxString::Format(L"%s kVar", fixed_point_to_string(value));
})>;
using kw_table_value = number_table_value<decltype([](const fixed_point &value) {
    return wxString::Format(L"%s kW", fixed_point_to_string(value));
})>;

static inline std::tuple dati_fattura {
    member_ptr_value    {"Nome File",               &variable_table_record::filename},
    member_ptr_value    {"Status",                  &variable_table_record::status},
    string_table_value  {"Ragione Sociale",         "ragione_sociale"},
    string_table_value  {"Indirizzo Fornitura",     "indirizzo_fornitura"},
    string_table_value  {table_value{.header="POD", .required=true}, "codice_pod"},
    month_table_value   {table_value{.header="Mese", .required=true}, "mese_fattura"},
    string_table_value  {table_value{.header="Fornitore", .required=true}, "fornitore"},
    string_table_value  {table_value{.header="N. Fatt.", .required=true}, "numero_fattura"},
    date_table_value    {table_value{.header="Data Emissione", .required=true}, "data_fattura"},
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

struct reader_output {
    variable_map values;
    std::filesystem::path filename;
    std::vector<std::string> warnings;
};

class VariableMapTable : public DataTableView<variable_table_record> {
public:
    VariableMapTable(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize,
        const wxValidator &validator = wxDefaultValidator) :
        DataTableView<variable_table_record>(parent, id, position, size, validator)
    {
        [&]<size_t ... Is>(std::index_sequence<Is...>) {
            ([&](const auto &value) {
                using T = typename std::remove_reference_t<decltype(value)>::value_type;
                using Formatter = typename std::remove_reference_t<decltype(value)>::formatter_type;
                AddColumn<OptionalValueColumn<T, Formatter>> (value.header, value.column_width, std::ref(value));
            }(std::get<Is>(dati_fattura)), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(dati_fattura)>>{});
    }

    void AddReaderValues(const reader_output &data) {
        std::multimap<std::string, variable> current_map;

        auto check_missing_add_item = [&]() {
            variable_table_record record{std::move(current_map), data.filename.string(), data.warnings};
            current_map.clear();

            [&]<size_t ... Is>(std::index_sequence<Is...>) {
                ([&](const auto &value) {
                    if (value.required && !std::invoke(value, record)) {
                        record.status.push_back(fmt::format("Mancante {}", value.header));
                    }
                }(std::get<Is>(dati_fattura)), ...);
            }(std::make_index_sequence<std::tuple_size_v<decltype(dati_fattura)>>{});

            AddItem(std::move(record));
        };

        size_t table_idx = 0;
        for (const auto &[key, value] : data.values) {
            if (key.table_index == variable_key::global_index) continue;
            if (key.name.front() == '_') continue;

            if (key.table_index != table_idx) {
                check_missing_add_item();
                table_idx = key.table_index;
            }
            current_map.emplace(key.name, value);
        }
        check_missing_add_item();
    }
};

#endif