#include "parser.h"

#include <json/json.h>
#include <functional>

#include "../shared/utils.h"

parser::variable_page &parser::get_variable_page() {
    size_t reading_page_num = m_globals["PAGE_NUM"].number().getAsInteger();
    if (m_values.size() <= reading_page_num) {
        return m_values.emplace_back();
    } else {
        return m_values[reading_page_num];
    }
}

void parser::add_value(const std::string &name, const variable &value) {
    if (name.empty() || value.empty()) return;

    if (name.at(0) == '*') {
        m_globals[name.substr(1)] = value;
    } else if (name.at(name.size()-1) == '+') {
        if (name.at(0) == '%') {
            get_variable_page()[name.substr(1, name.size()-2)].emplace_back(parse_number(value.str()), VALUE_NUMBER);
        } else {
            get_variable_page()[name.substr(0, name.size()-1)].push_back(value);
        }
    } else {
        if (name.at(0) == '%') {
            get_variable_page()[name.substr(1)] = {variable(parse_number(value.str()), VALUE_NUMBER)};
        } else {
            get_variable_page()[name] = {value};
        }
    }
}

void parser::add_entry(const std::string &script, const std::string &value) {
    size_t equals = script.find_first_of('=');
    if (equals == std::string::npos) {
        add_value(script, value);
    } else if (equals > 0) {
        add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), value));
    } else {
        throw parsing_error("Identificatore vuoto", script);
    }
};

void parser::add_spacer(const std::string &script, const std::string &value, spacer size) {
    size_t equals = script.find_first_of('=');
    if (equals == std::string::npos) {
        throw parsing_error("Errore di sintassi", script);
    } else if (equals > 0) {
        auto calc = evaluate(script.substr(equals + 1), value);
        if (calc) {
            size.pageoffset = calc.number().getAsInteger();
            m_spacers[script.substr(0, equals)] = size;
        }
    } else {
        throw parsing_error("Identificatore vuoto", script);
    }
}

void parser::exec_conditional_jump(const std::string &script, const std::string &value) {
    size_t equals = script.find_first_of('=');
    if (equals == std::string::npos) {
        throw parsing_error("Errore di sintassi", script);
    } else if (equals > 0) {
        std::string label = script.substr(0, equals);
        auto it = goto_labels.find(label);
        if (it == goto_labels.end()) {
            throw parsing_error("Impossibile trovare etichetta goto", script);
        } else if (evaluate(script.substr(equals + 1), value)) {
            program_counter = it->second;
            jumped = true;
        }
    } else {
        throw parsing_error("Identificatore vuoto", script);
    }
}

void parser::read_layout(const std::string &file_pdf, const bill_layout_script &layout) {
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        if (!layout.boxes[i].goto_label.empty()) {
            goto_labels[layout.boxes[i].goto_label] = i;
        }
    }
    pdf_info info = pdf_get_info(file_pdf);
    program_counter = 0;
    while (program_counter < layout.boxes.size()) {
        read_box(file_pdf, info, layout.boxes[program_counter]);
        if (jumped) {
            jumped = false;
        } else {
            ++program_counter;
        }
    }
}

void parser::read_box(const std::string &file_pdf, const pdf_info &info, const layout_box &box) {
    pdf_rect box_moved = box;
    for (auto &name : tokenize(box.spacers)) {
        if (name.size() <= 2 || name.at(name.size()-2) != '.') {
            throw parsing_error("Identificatore spaziatore incorretto", name);
        }
        bool negative = name.at(0) == '-';
        auto it = m_spacers.find(negative ? name.substr(1, name.size()-3) : name.substr(0, name.size()-2));
        if (it == m_spacers.end()) continue;
        switch (name.at(name.size()-1)) {
        case 'x':
        case 'X':
        case 'w':
        case 'W':
            if (negative) box_moved.x -= it->second.w;
            else box_moved.x += it->second.w;
            break;
        case 'y':
        case 'Y':
        case 'h':
        case 'H':
            if (negative) box_moved.y -= it->second.h;
            else box_moved.y += it->second.h;
            break;
        case 'p':
        case 'P':
            if (negative) box_moved.page -= it->second.pageoffset;
            else box_moved.page += it->second.pageoffset;
            break;
        default:
            throw parsing_error("Identificatore spaziatore incorretto", name);
        }
    }
    std::vector<std::string> scripts = read_lines(box.script);
    switch (box.type) {
    case BOX_WHOLE_FILE:
    {
        std::string text = pdf_whole_file_to_text(file_pdf);
        for (auto &script : scripts) {
            add_entry(script, text);
        }
        break;
    }
    case BOX_SINGLE:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        for (auto &script : scripts) {
            add_entry(script, text);
        }
        break;
    }
    case BOX_MULTIPLE:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        std::vector<std::string> values = tokenize(text);
        for (size_t i = 0; i < scripts.size() && i < values.size(); ++i) {
            add_entry(scripts[i], values[i]);
        }
        break;
    }
    case BOX_COLUMNS:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        std::vector<std::string> values = tokenize(text);
        for (size_t i = 0; i < values.size();++i) {
            add_entry(scripts[i % scripts.size()], values[i]);
        }
        break;
    }
    case BOX_ROWS:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        std::vector<std::string> values = tokenize(text);
        for (size_t i = 0; i < values.size();++i) {
            int row_len = values.size() / scripts.size();
            if (row_len < 1) row_len = 1;
            if (i / row_len < scripts.size()) {
                add_entry(scripts[i / row_len], values[i]);
            }
        }
        break;
    }
    case BOX_SPACER:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        for (auto &script : scripts) {
            add_spacer(script, text, spacer(box.w, box.h));
        }
        break;
    }
    case BOX_CONDITIONAL_JUMP:
    {
        std::string text = pdf_to_text(file_pdf, info, box_moved);
        for (auto &script : scripts) {
            exec_conditional_jump(script, text);
        }
        break;
    }
    default:
        break;
    }
}

void parser::read_script(std::istream &stream, const std::string &text) {
    std::vector<std::string> scripts = read_lines(stream);

    for (auto &script : scripts) {
        add_entry(script, text);
    }
}

const variable &parser::get_variable(const std::string &name, size_t index) const {
    static const variable VAR_EMPTY;
    size_t reading_page_num = 0;
    if (auto it = m_globals.find("PAGE_NUM"); it != m_globals.end()) {
        reading_page_num = it->second.number().getAsInteger();
    }
    if (m_values.size() <= reading_page_num) {
        return VAR_EMPTY;
    }
    auto &page = m_values[reading_page_num];
    auto it = page.find(name);
    
    if (it == page.end() || it->second.size() <= index) {
        return VAR_EMPTY;
    } else {
        return it->second.at(index);
    }
}

struct function_parser {
    const std::string &script;

    std::string name;
    std::vector<std::string> args;

    function_parser(const std::string &script);

    bool is(size_t min_args, size_t max_args = -1) const {
        if (args.size() >= min_args && args.size() <= max_args) {
            return true;
        } else if (min_args == max_args) {
            throw parsing_error(std::string("La funzione ") + name + " richiede " +
                std::to_string(min_args) + " argomenti", script);
        } else if (max_args != (size_t) -1) {
            throw parsing_error(std::string("La funzione ") + name + " richiede tra " +
                std::to_string(min_args) + " e " + std::to_string(max_args) + " argomenti", script);
        } else {
            throw parsing_error(std::string("La funzione ") + name + " richiede minimo " +
                std::to_string(min_args) + " argomenti", script);
        }
        return false;
    }
};

function_parser::function_parser(const std::string &script) : script(script) {
    size_t brace_start = script.find_first_of('(');
    if (brace_start == std::string::npos) throw parsing_error("Previsto '('", script);
    size_t brace_end = script.find_last_of(')');
    if (brace_end  == std::string::npos) throw parsing_error("Previsto ')'", script);
    name = script.substr(1, brace_start - 1);

    std::string reading_arg;
    int bracecount = 0;
    bool string_flag = false;
    bool escape_flag = false;
    for (size_t i = brace_start; i <= brace_end; ++i) {
        if (string_flag) {
            if (escape_flag) {
                switch(script[i]) {
                case 'n':
                    reading_arg += '\n';
                    break;
                default:
                    reading_arg += script[i];
                    break;
                }
                escape_flag = false;
            } else switch(script[i]) {
            case '"':
                string_flag = false;
                break;
            case '\\':
                escape_flag = true;
                break;
            default:
                reading_arg += script[i];
            }
        } else {
            switch(script[i]) {
            case '"':
                string_flag = true;
                break;
            case '(':
                ++bracecount;
                if (bracecount != 1) {
                    reading_arg += '(';
                }
                break;
            case ')':
                --bracecount;
                if (bracecount == 0) {
                    args.push_back(reading_arg);
                    if (i < brace_end) {
                        throw parsing_error("Valori non previsti dopo ')'", script);
                    }
                } else {
                    reading_arg += ')';
                }
                break;
            case ',':
                if (bracecount == 1) {
                    args.push_back(reading_arg);
                    reading_arg.clear();
                } else {
                    reading_arg += ',';
                }
                break;
            default:
                reading_arg += script[i];
                break;
            }
        }
    }
}

template<class>struct hasher;
template<>
struct hasher<std::string> {
    std::size_t constexpr operator() (char const *input)const {
        return *input ?
        static_cast<unsigned int>(*input) + 33 * (*this)(input + 1) : 5381;
    }
    
    std::size_t operator()( const std::string& str ) const {
        return (*this)(str.c_str());
    }
};
template<typename T>
std::size_t constexpr hash(T&& t) {
    return hasher< typename std::decay<T>::type >()(std::forward<T>(t));
}

std::size_t constexpr operator "" _hash(const char* s,size_t) {
    return hasher<std::string>()(s);
}

variable parser::evaluate(const std::string &script, const std::string &value) {
    if (!script.empty()) switch(script.at(0)) {
    case '$':
    {
        function_parser function(script);

        switch(hash(function.name)) {
        case "search"_hash:
            if (function.is(2, 3)) {
                return search_regex(evaluate(function.args[0], value).str(), evaluate(function.args[1], value).str(),
                    function.args.size() >= 3 ? evaluate(function.args[2], value).number().getAsInteger() : 1);
            }
            break;
        case "nospace"_hash:
            if (function.is(1, 1)) return nospace(evaluate(function.args[0], value).str());
            break;
        case "num"_hash:
            if (function.is(1, 1)) return variable(parse_number(evaluate(function.args[0], value).str()), VALUE_NUMBER);
            break;
        case "date"_hash:
            if (function.is(2, 3))
                return parse_date(evaluate(function.args[0], value).str(), evaluate(function.args[1], value).str(),
                    function.args.size() >= 3 ? evaluate(function.args[2], value).number().getAsInteger() : 1);
            break;
        case "month_begin"_hash:
            if (function.is(1, 1)) return evaluate(function.args[0], value).str() + "-01";
            break;
        case "month_end"_hash:
            if (function.is(1, 1)) return date_month_end(evaluate(function.args[0], value).str());
            break;
        case "coalesce"_hash:
            if (function.is(1)) {
                for (auto &arg : function.args) {
                    if (auto var = evaluate(arg, value)) return var;
                }
            }
            break;
        case "if"_hash:
            if (function.is(2, 3)) {
                if (evaluate(function.args[0], value)) return evaluate(function.args[1], value);
                else if (function.args.size() >= 3) return evaluate(function.args[2], value);
            }
            break;
        case "ifnot"_hash:
            if (function.is(2, 3)) {
                if (!evaluate(function.args[0], value)) return evaluate(function.args[1], value);
                else if (function.args.size() >= 3) return evaluate(function.args[2], value);
            }
            break;
        case "not"_hash:
            if (function.is(1, 1)) return !evaluate(function.args[0], value);
            break;
        case "eq"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) == evaluate(function.args[1], value);
            break;
        case "neq"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) != evaluate(function.args[1], value);
            break;
        case "and"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) && evaluate(function.args[1], value);
            break;
        case "or"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) || evaluate(function.args[1], value);
            break;
        case "contains"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value).str().find(evaluate(function.args[1], value).str()) != std::string::npos;
            break;
        case "add"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) + evaluate(function.args[1], value);
            break;
        case "sub"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) - evaluate(function.args[1], value);
            break;
        case "mul"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) * evaluate(function.args[1], value);
            break;
        case "div"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) / evaluate(function.args[1], value);
            break;
        case "gt"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) > evaluate(function.args[0], value);
            break;
        case "geq"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) >= evaluate(function.args[0], value);
            break;
        case "lt"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) < evaluate(function.args[1], value);
            break;
        case "leq"_hash:
            if (function.is(2, 2)) return evaluate(function.args[0], value) <= evaluate(function.args[1], value);
            break;
        case "max"_hash:
            if (function.is(2, 2)) return std::max(evaluate(function.args[0], value), evaluate(function.args[1], value));
            break;
        case "min"_hash:
            if (function.is(2, 2)) return std::min(evaluate(function.args[0], value), evaluate(function.args[1], value));
            break;
        case "int"_hash:
            if (function.is(1, 1)) return variable(std::to_string(evaluate(function.args[0], value).number().getAsInteger()), VALUE_NUMBER);
            // converte da stringa a fixed_point a int a stringa ...
            break;
        case "cat"_hash:
            if (function.is(1)) {
                std::string var;
                for (auto &arg : function.args) {
                    var += evaluate(arg, value).str();
                }
                return var;
            }
            break;
        case "error"_hash:
            if (function.is(1, 1)) throw layout_error(function.args[0].c_str());
            break;
        default:
            throw parsing_error("Funzione non riconosciuta: " + function.name, script);
        }

        break;
    }
    case '&':
        return get_variable(script.substr(1));
    case '@':
        return value + script.substr(1);
    case '~':
        return m_spacers[script.substr(1)].pageoffset;
    default:
        return script;
    }

    return variable();
}

std::ostream & operator << (std::ostream &out, const parser &res) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::arrayValue;

    for (auto &page : res.m_values) {
        auto &page_values = values.append(Json::objectValue);
        for (auto &pair : page) {
            if(!res.debug && pair.first.at(0) == '#') continue;
            auto &json_arr = page_values[pair.first] = Json::arrayValue;
            for (auto &val : pair.second) {
                json_arr.append(val.str());
            }
        }
    }

    return out << root << std::endl;
}