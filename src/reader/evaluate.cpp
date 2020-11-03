#include "parser.h"
#include "../shared/utils.h"

#include <fmt/core.h>

struct tokens {
    std::string name;
    std::vector<std::string> args;
};

tokens parse_function(const std::string &script) {
    tokens ret;
    
    size_t brace_start = script.find_first_of('(');
    if (brace_start == std::string::npos) throw parsing_error("Previsto '('", script);
    ret.name = script.substr(1, brace_start - 1);

    std::string reading_arg;
    int bracecount = 0;
    bool string_flag = false;
    bool escape_flag = false;
    for (size_t i = brace_start; i < script.size(); ++i) {
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
                    ret.args.push_back(reading_arg);
                } else if (bracecount > 0) {
                    reading_arg += ')';
                } else {
                    throw parsing_error("Valori non previsti dopo ')'", script);
                }
                break;
            case ',':
                if (bracecount == 1) {
                    ret.args.push_back(reading_arg);
                    reading_arg.clear();
                } else {
                    reading_arg += ',';
                }
                break;
            default:
                if (bracecount == 0) {
                    throw parsing_error("Valori non previsti dopo ')'", script);
                }
                reading_arg += script[i];
                break;
            }
        }
    }
    if (bracecount > 0) {
        throw parsing_error("Previsto ')'", script);
    }

    return ret;
}

variable parser::evaluate(const std::string &script, const box_content &content) {
    if (!script.empty()) switch(script.front()) {
    case '$':
    {
        auto function = parse_function(script);

        auto eval = [&](int i){ return evaluate(function.args[i], content); };

        auto fun_is = [&](size_t min_args, size_t max_args = -1) {
            if (function.args.size() >= min_args && function.args.size() <= max_args) {
                return true;
            } else if (min_args == max_args) {
                throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", function.name, min_args), script);
            } else if (max_args != (size_t) -1) {
                throw parsing_error(fmt::format("La funzione {0} richiede tra {1} e {2} argomenti", function.name, min_args, max_args), script);
            } else {
                throw parsing_error(fmt::format("La funzione {0} richiede minimo {1} argomenti", function.name, min_args), script);
            }
            return false;
        };

        switch(hash(function.name)) {
        case hash("search"): // $search(regex,[contenuto,index])
            if (fun_is(1, 3)) {
                return search_regex(eval(0).str(),
                    function.args.size() >= 2 ? eval(1).str() : content.text,
                    function.args.size() >= 3 ? eval(2).asInt() : 1);
            }
            break;
        case hash("nospace"): // converte qualsiasi spazio in ' '
            if (fun_is(1, 1)) return nospace(eval(0).str());
            break;
        case hash("num"): // parsa il formato numerico italiano
            if (fun_is(1, 1)) return variable(parse_number(eval(0).str()), VALUE_NUMBER);
            break;
        case hash("date"): // $date(regex[,contenuto,index]) -- come search ma cerca le date
            if (fun_is(1, 3))
                return parse_date(eval(0).str(),
                    function.args.size() >= 2 ? eval(1).str() : content.text,
                    function.args.size() >= 3 ? eval(2).asInt() : 1);
            break;
        case hash("month_begin"):
            if (fun_is(1, 1)) return eval(0).str() + "-01";
            break;
        case hash("month_end"):
            if (fun_is(1, 1)) return date_month_end(eval(0).str());
            break;
        case hash("coalesce"):
            if (fun_is(1)) {
                for (auto &arg : function.args) {
                    if (auto var = evaluate(arg, content)) return var;
                }
            }
            break;
        case hash("do"):
            if (fun_is(1)) {
                for (auto &arg : function.args) {
                    execute_line(arg, content);
                }
            }
            break;
        case hash("if"): // $if(cond,then[,else])
            if (fun_is(2, 3)) {
                if (eval(0)) return eval(1);
                else if (function.args.size() >= 3) return eval(2);
            }
            break;
        case hash("ifnot"): // $ifnot(cond,then[,else])
            if (fun_is(2, 3)) {
                if (!eval(0)) return eval(1);
                else if (function.args.size() >= 3) return eval(2);
            }
            break;
        case hash("while"): // $while(cond,line)
            if (fun_is(2, 2)) {
                while (eval(0)) eval(1);
            }
            break;
        case hash("for"): // $for(idx,range,line)
            if (fun_is(3, 3)) {
                auto idx = get_variable(function.args[0], content);
                size_t len = eval(1).asInt();
                for (size_t i=0; i<len; ++i) {
                    *idx = i;
                    eval(2);
                }
                idx.clear();
            }
            break;
        case hash("clear"):
            if (fun_is(1, 1)) get_variable(function.args[0], content).clear();
            break;
        case hash("size"):
            if (fun_is(1, 1)) return get_variable(function.args[0], content).size();
            break;
        case hash("not"):
            if (fun_is(1, 1)) return !eval(0);
            break;
        case hash("eq"):
            if (fun_is(2, 2)) return eval(0) == eval(1);
            break;
        case hash("neq"):
            if (fun_is(2, 2)) return eval(0) != eval(1);
            break;
        case hash("and"):
            if (fun_is(2, 2)) return eval(0) && eval(1);
            break;
        case hash("or"):
            if (fun_is(2, 2)) return eval(0) || eval(1);
            break;
        case hash("contains"):
            if (fun_is(2, 2)) return eval(0).str().find(eval(1).str()) != std::string::npos;
            break;
        case hash("strlen"):
            if (fun_is(1, 1)) return eval(0).str().size();
            break;
        case hash("isempty"):
            if (fun_is(1, 1)) return eval(0).str().empty();
            break;
        case hash("substr"):
            if (fun_is(2, 3)) {
                auto str = eval(0).str();
                size_t pos = eval(1).asInt();
                size_t count = function.args.size() >= 3 ? eval(2).asInt() : std::string::npos;
                if (pos <= str.size()) {
                    return str.substr(pos, count);
                }
            }
            break;
        case hash("inc"):
            if (fun_is(1, 2)) {
                auto &var = *get_variable(function.args[0], content);
                auto inc = function.args.size() >= 2 ? eval(1) : variable(1);
                if (inc) return var = var + inc;
                else return var;
            }
            break;
        case hash("dec"):
            if (fun_is(1, 2)) {
                auto &var = *get_variable(function.args[0], content);
                auto inc = function.args.size() >= 2 ? eval(1) : variable(1);
                if (inc) return var = var - inc;
                else return var;
            }
            break;
        case hash("add"):
            if (fun_is(2, 2)) return eval(0) + eval(1);
            break;
        case hash("sub"):
            if (fun_is(2, 2)) return eval(0) - eval(1);
            break;
        case hash("mul"):
            if (fun_is(2, 2)) return eval(0) * eval(1);
            break;
        case hash("div"):
            if (fun_is(2, 2)) return eval(0) / eval(1);
            break;
        case hash("gt"):
            if (fun_is(2, 2)) return eval(0) > eval(0);
            break;
        case hash("geq"):
            if (fun_is(2, 2)) return eval(0) >= eval(0);
            break;
        case hash("lt"):
            if (fun_is(2, 2)) return eval(0) < eval(1);
            break;
        case hash("leq"):
            if (fun_is(2, 2)) return eval(0) <= eval(1);
            break;
        case hash("max"):
            if (fun_is(2, 2)) return std::max(eval(0), eval(1));
            break;
        case hash("min"):
            if (fun_is(2, 2)) return std::min(eval(0), eval(1));
            break;
        case hash("int"):
            if (fun_is(1, 1)) return variable(std::to_string(eval(0).asInt()), VALUE_NUMBER);
            // converte da stringa a fixed_point a int a stringa ...
            break;
        case hash("cat"):
            if (fun_is(1)) {
                std::string var;
                for (auto &arg : function.args) {
                    var += evaluate(arg, content).str();
                }
                return var;
            }
            break;
        case hash("error"):
            if (fun_is(1, 1)) throw layout_error(function.args[0].c_str());
            break;
        case hash("goto"):
            if (fun_is(1, 1)) {
                if (function.args[0].front() == '%') {
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
            if (fun_is(1, 1)) {
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
            if (fun_is(1, 1)) {
                m_spacers[function.args[0]] = spacer(content.w, content.h);
            }
            break;
        case hash("tokens"):
            if (fun_is(2)) {
                auto tokens = tokenize(eval(0).str());
                auto con_token = content;
                for (size_t i=1; i < function.args.size() && i <= tokens.size(); ++i) {
                    con_token.text = tokens[i-1];
                    execute_line(function.args[i], con_token);
                }
            }
            break;
        case hash("lines"):
            if (fun_is(2, 2)) {
                auto lines = read_lines(eval(0).str());
                auto con_token = content;
                for (auto &line : lines) {
                    con_token.text = line;
                    execute_line(function.args[1], con_token);
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
        if (auto ref = get_variable(script.substr(1), content); ref.isset()) return *ref;
        break;
    case '*':
        return get_global(script.substr(1));
    case '@':
        if (script.size() > 1) throw parsing_error("Valore inatteso dopo '@'", script);
        return variable(content.text);
    default:
        return script;
    }

    return variable();
}