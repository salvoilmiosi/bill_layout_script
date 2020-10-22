#include "parser.h"

#include <json/json.h>
#include <fmt/core.h>

#include "../shared/utils.h"

parser::variable_page &parser::get_variable_page() {
    size_t reading_page_num = m_globals["PAGE_NUM"].number().getAsInteger();
    if (m_values.size() <= reading_page_num) {
        return m_values.emplace_back();
    } else {
        return m_values[reading_page_num];
    }
}

variable parser::add_value(const std::string &name, const variable &value) {
    if (name.empty() || value.empty()) return variable();

    if (name.at(0) == '*') {
        return m_globals[name.substr(1)] = value;
    } else if (name.at(name.size()-1) == '+') {
        if (name.at(0) == '%') {
            return get_variable_page()[name.substr(1, name.size()-2)].emplace_back(parse_number(value.str()), VALUE_NUMBER);
        } else {
            return get_variable_page()[name.substr(0, name.size()-1)].emplace_back(value);
        }
    } else {
        if (name.at(0) == '%') {
            variable var(parse_number(value.str()), VALUE_NUMBER);
            get_variable_page()[name.substr(1)] = {var};
            return var;
        } else {
            get_variable_page()[name] = {value};
            return value;
        }
    }
}

variable parser::execute_line(const std::string &script, const box_content &content) {
    if (script.at(0) == '$') {
        return evaluate(script, content);
    } else {
        size_t equals = script.find_first_of('=');
        if (equals == std::string::npos) {
            return add_value(script, content.text);
        } else if (equals > 0) {
            return add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), content));
        } else {
            throw parsing_error("Identificatore vuoto", script);
        }
    }
    return variable();
};

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

void parser::read_box(const std::string &file_pdf, const pdf_info &info, layout_box box) {
    for (auto &name : tokenize(box.spacers)) {
        if (name.at(0) == '*') {
            if (auto it = m_globals.find(name.substr(1)); it != m_globals.end()) {
                box.page += it->second.number().getAsInteger();
            }
        } else {
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
                if (negative) box.x -= it->second.w;
                else box.x += it->second.w;
                break;
            case 'y':
            case 'Y':
            case 'h':
            case 'H':
                if (negative) box.y -= it->second.h;
                else box.y += it->second.h;
                break;
            default:
                throw parsing_error("Identificatore spaziatore incorretto", name);
            }
        }
    }
    std::vector<std::string> scripts = read_lines(box.script);
    box_content content(box);
    if (box.whole_file) {
        content.text = pdf_whole_file_to_text(file_pdf);
    } else {
        content.text = pdf_to_text(file_pdf, info, box);
    }
    for (auto &script : scripts) {
        execute_line(script, content);
    }
}

void parser::read_script(std::istream &stream, const std::string &text) {
    std::vector<std::string> scripts = read_lines(stream);

    box_content content(text);
    for (auto &script : scripts) {
        execute_line(script, content);
    }
}

const variable &parser::get_variable(const std::string &name) const {
    static const variable VAR_EMPTY;
    if (name.at(0) == '*') {
        if (auto it = m_globals.find(name.substr(1)); it != m_globals.end()) {
            return it->second;
        } else {
            return VAR_EMPTY;
        }
    }
    size_t reading_page_num = 0;
    if (auto it = m_globals.find("PAGE_NUM"); it != m_globals.end()) {
        reading_page_num = it->second.number().getAsInteger();
    }
    if (m_values.size() <= reading_page_num) {
        return VAR_EMPTY;
    }
    auto &page = m_values[reading_page_num];
    size_t open_bracket = name.find('[');
    size_t index = 0;
    if (open_bracket != std::string::npos) {
        size_t end_bracket = name.find(']', open_bracket + 1);
        if (end_bracket == std::string::npos) {
            throw parsing_error("Richiesto ']'", name);
        } else try {
            index = std::stoi(name.substr(open_bracket + 1, end_bracket - open_bracket - 1));
        } catch (std::invalid_argument &) {
            throw parsing_error("Indice non valido", name);
        }
    }
    auto it = page.find(name.substr(0, open_bracket));
    
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
            throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", name, min_args), script);
        } else if (max_args != (size_t) -1) {
            throw parsing_error(fmt::format("La funzione {0} richiede tra {1} e {2} argomenti", name, min_args, max_args), script);
        } else {
            throw parsing_error(fmt::format("La funzione {0} richiede minimo {1} argomenti", name, min_args), script);
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

struct hasher {
    size_t constexpr operator() (char const *input)const {
        return *input ? static_cast<unsigned int>(*input) + 33 * (*this)(input + 1) : 5381;
    }
    
    size_t operator() (const std::string& str) const {
        return (*this)(str.c_str());
    }
};

template<typename T> size_t constexpr hash(T&& t) {
    return hasher{}(std::forward<T>(t));
}

variable parser::evaluate(const std::string &script, const box_content &content) {
    if (!script.empty()) switch(script.at(0)) {
    case '$':
    {
        function_parser function(script);

        switch(hash(function.name)) {
        case hash("search"):
            if (function.is(2, 3)) {
                return search_regex(evaluate(function.args[0], content).str(), evaluate(function.args[1], content).str(),
                    function.args.size() >= 3 ? evaluate(function.args[2], content).number().getAsInteger() : 1);
            }
            break;
        case hash("nospace"):
            if (function.is(1, 1)) return nospace(evaluate(function.args[0], content).str());
            break;
        case hash("num"):
            if (function.is(1, 1)) return variable(parse_number(evaluate(function.args[0], content).str()), VALUE_NUMBER);
            break;
        case hash("date"):
            if (function.is(2, 3))
                return parse_date(evaluate(function.args[0], content).str(), evaluate(function.args[1], content).str(),
                    function.args.size() >= 3 ? evaluate(function.args[2], content).number().getAsInteger() : 1);
            break;
        case hash("month_begin"):
            if (function.is(1, 1)) return evaluate(function.args[0], content).str() + "-01";
            break;
        case hash("month_end"):
            if (function.is(1, 1)) return date_month_end(evaluate(function.args[0], content).str());
            break;
        case hash("coalesce"):
            if (function.is(1)) {
                for (auto &arg : function.args) {
                    if (auto var = evaluate(arg, content)) return var;
                }
            }
            break;
        case hash("do"):
            if (function.is(1)) {
                for (auto &arg : function.args) {
                    execute_line(arg, content);
                }
            }
            break;
        case hash("if"):
            if (function.is(2, 3)) {
                if (evaluate(function.args[0], content)) return evaluate(function.args[1], content);
                else if (function.args.size() >= 3) return evaluate(function.args[2], content);
            }
            break;
        case hash("ifnot"):
            if (function.is(2, 3)) {
                if (!evaluate(function.args[0], content)) return evaluate(function.args[1], content);
                else if (function.args.size() >= 3) return evaluate(function.args[2], content);
            }
            break;
        case hash("while"):
            if (function.is(2, 2)) {
                while (evaluate(function.args[0], content)) evaluate(function.args[1], content);
            }
            break;
        case hash("not"):
            if (function.is(1, 1)) return !evaluate(function.args[0], content);
            break;
        case hash("eq"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) == evaluate(function.args[1], content);
            break;
        case hash("neq"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) != evaluate(function.args[1], content);
            break;
        case hash("and"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) && evaluate(function.args[1], content);
            break;
        case hash("or"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) || evaluate(function.args[1], content);
            break;
        case hash("contains"):
            if (function.is(2, 2)) return evaluate(function.args[0], content).str().find(evaluate(function.args[1], content).str()) != std::string::npos;
            break;
        case hash("inc"):
            if (function.is(1, 2)) {
                return add_value(function.args[0], get_variable(function.args[0]) + (function.args.size() >= 2 ? evaluate(function.args[1], content) : variable(1)));
            }
            break;
        case hash("dec"):
            if (function.is(1, 2)) {
                return add_value(function.args[0], get_variable(function.args[0]) - (function.args.size() >= 2 ? evaluate(function.args[1], content) : variable(1)));
            }
            break;
        case hash("add"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) + evaluate(function.args[1], content);
            break;
        case hash("sub"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) - evaluate(function.args[1], content);
            break;
        case hash("mul"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) * evaluate(function.args[1], content);
            break;
        case hash("div"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) / evaluate(function.args[1], content);
            break;
        case hash("gt"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) > evaluate(function.args[0], content);
            break;
        case hash("geq"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) >= evaluate(function.args[0], content);
            break;
        case hash("lt"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) < evaluate(function.args[1], content);
            break;
        case hash("leq"):
            if (function.is(2, 2)) return evaluate(function.args[0], content) <= evaluate(function.args[1], content);
            break;
        case hash("max"):
            if (function.is(2, 2)) return std::max(evaluate(function.args[0], content), evaluate(function.args[1], content));
            break;
        case hash("min"):
            if (function.is(2, 2)) return std::min(evaluate(function.args[0], content), evaluate(function.args[1], content));
            break;
        case hash("int"):
            if (function.is(1, 1)) return variable(std::to_string(evaluate(function.args[0], content).number().getAsInteger()), VALUE_NUMBER);
            // converte da stringa a fixed_point a int a stringa ...
            break;
        case hash("cat"):
            if (function.is(1)) {
                std::string var;
                for (auto &arg : function.args) {
                    var += evaluate(arg, content).str();
                }
                return var;
            }
            break;
        case hash("error"):
            if (function.is(1, 1)) throw layout_error(function.args[0].c_str());
            break;
        case hash("goto"):
            if (function.is(1, 1)) {
                if (function.args[0].at(0) == '%') {
                    try {
                        program_counter = std::stoi(function.args[0].substr(1));
                        jumped = true;
                    } catch (std::invalid_argument &) {
                        throw parsing_error("Indirizzo goto invalido", script);
                    }
                } else {
                    auto it = goto_labels.find(function.args[0]);
                    if (it == goto_labels.end()) {
                        throw parsing_error("Impossibile trovare etichetta", script);
                    } else {
                        program_counter = it->second;
                        jumped = true;
                    }
                }
            }
            break;
        case hash("addspacer"):
            if (function.is(1, 1)) {
                m_spacers[function.args[0]] = spacer(content.w, content.h);
            }
            break;
        case hash("tokens"):
            if (function.is(2)) {
                auto tokens = tokenize(evaluate(function.args[0], content).str());
                auto con_token = content;
                for (size_t i=1; i<function.args.size(); ++i) {
                    con_token.text = tokens[(i-1) % tokens.size()];
                    execute_line(function.args[i], con_token);
                }
            }
            break;
        default:
            throw parsing_error(fmt::format("Funzione non riconosciuta: {0}", function.name), script);
        }

        break;
    }
    case '%':
        return variable(script.substr(1), VALUE_NUMBER);
        break;
    case '&':
        return get_variable(script.substr(1));
    case '*':
        return get_variable(script);
    case '@':
        if (script.size() > 1) throw parsing_error("Valore inatteso dopo '@'", script);
        return variable(content.text);
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