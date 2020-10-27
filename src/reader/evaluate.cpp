#include "parser.h"
#include "../shared/utils.h"

#include <fmt/core.h>

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

variable parser::evaluate(const std::string &script, const box_content &content) {
    if (!script.empty()) switch(script.at(0)) {
    case '$':
    {
        function_parser function(script);

        switch(hash(function.name)) {
        case hash("search"):
            if (function.is(1, 3)) {
                return search_regex(evaluate(function.args[0], content).str(),
                    function.args.size() >= 2 ? evaluate(function.args[1], content).str() : content.text,
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
            if (function.is(1, 3))
                return parse_date(evaluate(function.args[0], content).str(),
                    function.args.size() >= 2 ? evaluate(function.args[1], content).str() : content.text,
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
        case hash("strlen"):
            if (function.is(1, 1)) return evaluate(function.args[0], content).str().size();
            break;
        case hash("isempty"):
            if (function.is(1, 1)) return evaluate(function.args[0], content).str().empty();
            break;
        case hash("substr"):
            if (function.is(2, 3)) {
                auto str = evaluate(function.args[0], content).str();
                size_t pos = evaluate(function.args[1], content).number().getAsInteger();
                size_t count = function.args.size() >= 3 ? evaluate(function.args[2], content).number().getAsInteger() : std::string::npos;
                if (pos <= str.size()) {
                    return str.substr(pos, count);
                }
            }
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
                        return_address = program_counter;
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
                        return_address = program_counter;
                        program_counter = it->second;
                        jumped = true;
                    }
                }
            }
            break;
        case hash("return"):
            if (function.is(1, 1)) {
                if (return_address >= 0) {
                    program_counter = return_address + 1;
                    return_address = -1;
                    jumped = true;
                } else {
                    throw parsing_error("Indirizzo return invalido", script);
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
                for (size_t i=1; i < function.args.size() && i <= tokens.size(); ++i) {
                    con_token.text = tokens[i-1];
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