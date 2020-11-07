#include "reader.h"

#include <json/json.h>
#include <fmt/format.h>

#include <utility>
#include <functional>

#include "../shared/utils.h"

void reader::read_layout(const pdf_info &info, const bill_layout_script &layout) {
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        if (!layout.boxes[i].goto_label.empty()) {
            goto_labels[layout.boxes[i].goto_label] = i;
        }
    }
    program_counter = 0;
    while (program_counter < layout.boxes.size()) {
        read_box(info, layout.boxes[program_counter]);
        if (jumped) {
            jumped = false;
        } else {
            ++program_counter;
        }
    }
}

void reader::read_box(const pdf_info &info, layout_box box) {
    for (auto &name : tokenize(box.spacers)) {
        if (name.front() == '*') {
            if (auto it = m_globals.find(name.substr(1)); it != m_globals.end()) {
                box.page += it->second.asInt();
            }
        } else {
            if (name.size() <= 2 || name.at(name.size()-2) != '.') {
                throw parsing_error("Identificatore spaziatore incorretto", box.spacers);
            }
            bool negative = name.front() == '-';
            auto it = m_spacers.find(negative ? name.substr(1, name.size()-3) : name.substr(0, name.size()-2));
            if (it == m_spacers.end()) continue;
            switch (name.back()) {
            case 'x':
                if (negative) box.x -= it->second.w;
                else box.x += it->second.w;
                break;
            case 'w':
                if (negative) box.w -= it->second.w;
                else box.w += it->second.w;
                break;
            case 'y':
                if (negative) box.y -= it->second.h;
                else box.y += it->second.h;
                break;
            case 'h':
                if (negative) box.h -= it->second.h;
                else box.h += it->second.h;
                break;
            default:
                throw parsing_error("Identificatore spaziatore incorretto", box.spacers);
            }
        }
    }
    tokenizer tokens(box.script);
    box_content content(box, pdf_to_text(info, box));
    while (!tokens.ate()) {
        exec_line(tokens, content);
    }
}

variable reader::add_value(variable_ref ref, variable value, bool ignore) {
    if (ignore) return variable();

    if (ref.flags & variable_ref::FLAGS_NUMBER && value.type() != VALUE_NUMBER) {
        value = variable(parse_number(value.str()), VALUE_NUMBER);
    }

    if (ref.flags & variable_ref::FLAGS_DEBUG) {
        value.debug = true;
    }

    if (!value.empty()) {
        *ref = value;
    }
    return value;
}

variable reader::exec_line(tokenizer &tokens, const box_content &content, bool ignore) {
    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_BRACE_BEGIN:
        tokens.advance();
        while (true) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            exec_line(tokens, content, ignore);
        }
        break;
    case TOK_COMMENT:
        tokens.advance();
        break;
    case TOK_FUNCTION:
        return exec_function(tokens, content, ignore);
    case TOK_SYS_FUNCTION:
        exec_sys_function(tokens, content, ignore);
        break;
    default:
    {
        variable_ref ref = get_variable(tokens, content);
        tokens.next();
        switch (tokens.current().type) {
        case TOK_EQUALS:
            return add_value(ref, evaluate(tokens, content, ignore), ignore);
        case TOK_BRACE_END:
        case TOK_END_OF_FILE:
            return add_value(ref, content.text, ignore);
        default:
            throw parsing_error("Token imprevisto", tokens.getLocation(tokens.current()));
        }
    }
    }
    return variable();
}

variable reader::evaluate(tokenizer &tokens, const box_content &content, bool ignore) {
    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_FUNCTION:
        return exec_function(tokens, content, ignore);
    case TOK_NUMBER:
        tokens.advance();
        return variable(std::string(tokens.current().value), VALUE_NUMBER);
    case TOK_STRING:
        tokens.advance();
        return parse_string(tokens.current().value);
    case TOK_CONTENT:
        tokens.advance();
        return content.text;
    case TOK_IDENTIFIER:
    {
        auto ref = get_variable(tokens, content);
        return ref.isset() ? *ref : variable();
    }
    default:
        throw parsing_error("Token imprevisto", tokens.getLocation(tokens.current()));
    }
    return variable();
}

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

struct scripted_error {
    const std::string &msg;
};

using arg_list = std::vector<variable>;
using function_handler = std::function<variable(const arg_list &)>;

template<typename Function, std::size_t ... Is>
variable exec_helper(Function fun, const arg_list &args, std::index_sequence<Is...>) {
    auto get_arg = [](const arg_list &args, size_t index) -> const variable & {
        static const variable VAR_NULL;
        if (args.size() <= index) {
            return VAR_NULL;
        } else {
            return args[index];
        }
    };
    return fun(get_arg(args, Is)...);
}

template<size_t Minargs, size_t Maxargs, typename Function>
function_handler create_function(Function fun) {
    return [fun](const arg_list &vars) -> variable {
        if (vars.size() < Minargs || vars.size() > Maxargs) {
            throw invalid_numargs{vars.size(), Minargs, Maxargs};
        }
        return exec_helper(fun, vars, std::make_index_sequence<Maxargs>{});
    };
}

template<size_t Argnum, typename Function>
inline function_handler create_function(Function fun) {
    return create_function<Argnum, Argnum>(fun);
}

template<typename Function>
inline function_handler create_function(Function fun) {
    return create_function<1>(fun);
}

variable reader::exec_function(tokenizer &tokens, const box_content &content, bool ignore) {
    tokens.next();
    if (tokens.current().type != TOK_FUNCTION) {
        throw parsing_error("Previsto '$'", tokens.getLocation(tokens.current()));
    }

    tokens.next();
    if (tokens.current().type != TOK_IDENTIFIER) {
        throw parsing_error("Previsto identificatore", tokens.getLocation(tokens.current()));
    }

    token fun_opening = tokens.current();
    std::string fun_name = std::string(tokens.current().value);
    arg_list args;

    tokens.next();
    if (tokens.current().type != TOK_PAREN_BEGIN) {
        throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
    }

    bool in_fun_loop = true;
    while (in_fun_loop) {
        args.push_back(evaluate(tokens, content, ignore));
        tokens.next();
        switch (tokens.current().type) {
        case TOK_COMMA:
            break;
        case TOK_PAREN_END:
            in_fun_loop=false;
            break;
        default:
            throw parsing_error("Token imprevisto", tokens.getLocation(tokens.current()));
        }
    }

    static const std::map<std::string, function_handler> dispatcher = {
        {"eq",      create_function<2>([](const variable &a, const variable &b) { return a == b; })},
        {"neq",     create_function<2>([](const variable &a, const variable &b) { return a != b; })},
        {"and",     create_function<2>([](const variable &a, const variable &b) { return a && b; })},
        {"or",      create_function<2>([](const variable &a, const variable &b) { return a || b; })},
        {"not",     create_function   ([](const variable &var) { return !var; })},
        {"add",     create_function<2>([](const variable &a, const variable &b) { return a + b; })},
        {"sub",     create_function<2>([](const variable &a, const variable &b) { return a - b; })},
        {"mul",     create_function<2>([](const variable &a, const variable &b) { return a * b; })},
        {"div",     create_function<2>([](const variable &a, const variable &b) { return a / b; })},
        {"gt",      create_function<2>([](const variable &a, const variable &b) { return a > b; })},
        {"lt",      create_function<2>([](const variable &a, const variable &b) { return a < b; })},
        {"geq",     create_function<2>([](const variable &a, const variable &b) { return a >= b; })},
        {"leq",     create_function<2>([](const variable &a, const variable &b) { return a <= b; })},
        {"max",     create_function<2>([](const variable &a, const variable &b) { return std::max(a, b); })},
        {"min",     create_function<2>([](const variable &a, const variable &b) { return std::min(a, b); })},
        {"search", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            return search_regex(regex.str(), str.str(), index ? index.asInt() : 1);
        })},
        {"date", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            return parse_date(regex.str(), str.str(), index ? index.asInt() : 1);
        })},
        {"month_begin", create_function([](const variable &str) { return str.str() + "-01"; })},
        {"month_end", create_function([](const variable &str) { return date_month_end(str.str()); })},
        {"nospace", create_function([](const variable &str) { return nospace(str.str()); })},
        {"num", create_function([](const variable &str) { return parse_number(str.str()); })},
        {"if", create_function<2, 3>   ([](const variable &condition, const variable &var_if, const variable &var_else) { return condition ? var_if : var_else; })},
        {"ifnot", create_function<2, 3>([](const variable &condition, const variable &var_if, const variable &var_else) { return condition ? var_else : var_if; })},
        {"coalesce", [](const arg_list &args) {
            for (auto &arg : args) {
                if (arg) return arg;

            }
            return variable();
        }},
        {"contains", create_function<2>([](const variable &str, const variable &str2) {
            return str.str().find(str2.str()) != std::string::npos;
        })},
        {"substr", create_function<2, 3>([](const variable &str, const variable &pos, const variable &count) {
            if ((size_t) pos.asInt() < str.str().size()) {
                return variable(str.str().substr(pos.asInt(), count ? count.asInt() : std::string::npos));
            }
            return variable();
        })},
        {"strlen", create_function([](const variable &str) { return str.str().size(); })},
        {"isempty", create_function([](const variable &str) { return str.empty(); })},
        {"strcat", [](const arg_list &args) {
            std::string var;
            for (auto &arg : args) {
                var += arg.str();
            }
            return var;
        }},
        {"error", create_function([](const variable &msg) {
            if (msg) throw scripted_error{msg.str()};
            return variable();
        })},
        {"int", create_function([](const variable &str) {
            return variable(std::to_string(str.asInt()), VALUE_NUMBER);
            // converte da stringa a fixed_point a int a stringa ...
        })}
    };

    auto it = dispatcher.find(fun_name);
    if (it != dispatcher.end()) {
        try {
            if (!ignore) {
                return it->second(args);
            }
        } catch (invalid_numargs &err) {
            if ((err.minargs == err.maxargs) && err.numargs != err.maxargs) {
                throw parsing_error(fmt::format("Richiesti {0} argomenti", err.maxargs), tokens.getLocation(fun_opening));
            } else if (err.numargs < err.minargs) {
                throw parsing_error(fmt::format("Richiesti almeno {0} argomenti", err.minargs), tokens.getLocation(fun_opening));
            } else if (err.numargs > err.maxargs) {
                throw parsing_error(fmt::format("Richiesti al massimo {1} argomenti", err.maxargs), tokens.getLocation(fun_opening));
            }
        } catch (scripted_error &err) {
            throw parsing_error(err.msg, tokens.getLocation(fun_opening));
        }
    } else {
        throw parsing_error(fmt::format("Funzione {0} non riconosciuta", fun_name), tokens.getLocation(fun_opening));
    }

    return variable();
}

void reader::exec_sys_function(tokenizer &tokens, const box_content &content, bool ignore) {
    tokens.next();
    if (tokens.current().type != TOK_SYS_FUNCTION) {
        throw parsing_error("Previsto '&'", tokens.getLocation(tokens.current()));
    }

    tokens.next();
    if (tokens.current().type != TOK_IDENTIFIER) {
        throw parsing_error("Previsto identificatore", tokens.getLocation(tokens.current()));
    }

    token fun_opening = tokens.current();
    std::string fun_name = std::string(tokens.current().value);

    if (fun_name == "if") {
        tokens.next();
        if (tokens.current().type != TOK_PAREN_BEGIN) {
            throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
        }

        auto condition = evaluate(tokens, content, ignore);
        
        tokens.next();
        if (tokens.current().type != TOK_PAREN_END) {
            throw parsing_error("Previsto ')'", tokens.getLocation(tokens.current()));
        }
        
        exec_line(tokens, content, ignore || !condition);
    } else if (fun_name == "while") {
        tokens.next();
        if (tokens.current().type != TOK_PAREN_BEGIN) {
            throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
        }

        token tok_condition = tokens.current();
        auto condition = evaluate(tokens, content, true);
        
        tokens.next();
        if (tokens.current().type != TOK_PAREN_END) {
            throw parsing_error("Previsto ')'", tokens.getLocation(tokens.current()));
        }

        tokens.next(true);
        token tok_begin = tokens.current();
        while (true) {
            tokens.gotoTok(tok_condition);
            if (evaluate(tokens, content, ignore)) {
                tokens.gotoTok(tok_begin);
                exec_line(tokens, content, ignore);
            } else {
                tokens.gotoTok(tok_begin);
                exec_line(tokens, content, true);
                break;
            }
        }
    } else if (fun_name == "lines") {
        tokens.next();
        if (tokens.current().type != TOK_PAREN_BEGIN) {
            throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
        }

        auto str = evaluate(tokens, content, ignore);

        tokens.next();
        if (tokens.current().type != TOK_PAREN_END) {
            throw parsing_error("Previsto ')'", tokens.getLocation(tokens.current()));
        }

        auto lines = read_lines(str.str());

        tokens.next(true);
        token tok_begin = tokens.current();

        box_content content_line = content;
        for (auto &line : lines) {
            tokens.gotoTok(tok_begin);
            content_line.text = line;
            exec_line(tokens, content_line, ignore);
        }
    } else if (fun_name == "tokens") {
        tokens.next();
        if (tokens.current().type != TOK_PAREN_BEGIN) {
            throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
        }

        auto str = evaluate(tokens, content, ignore);

        tokens.next();
        if (tokens.current().type != TOK_PAREN_END) {
            throw parsing_error("Previsto ')'", tokens.getLocation(tokens.current()));
        }

        auto strtoks = tokenize(str.str());

        tokens.next();
        if (tokens.current().type != TOK_BRACE_BEGIN) {
            throw parsing_error("Previsto '{'", tokens.getLocation(tokens.current()));
        }

        box_content content_strtok = content;
        size_t i = 0;
        while (true) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            content_strtok.text = i < strtoks.size() ? strtoks[i] : variable();
            exec_line(tokens, content_strtok, ignore);
            ++i;
        }
    } else if (fun_name == "for") {
        tokens.next();
        if (tokens.current().type != TOK_PAREN_BEGIN) {
            throw parsing_error("Previsto '('", tokens.getLocation(tokens.current()));
        }

        auto idx = get_variable(tokens, content);

        tokens.next();
        if (tokens.current().type != TOK_COMMA) {
            throw parsing_error("Previsto ','", tokens.getLocation(tokens.current()));
        }

        size_t num = evaluate(tokens, content).asInt();

        tokens.next();
        if (tokens.current().type != TOK_PAREN_END) {
            throw parsing_error("Previsto ')'", tokens.getLocation(tokens.current()));
        }

        tokens.next(true);
        token tok_begin = tokens.current();

        for (size_t i=0; i<num; ++i) {
            *idx = i;
            tokens.gotoTok(tok_begin);
            exec_line(tokens, content, ignore);
        }
        tokens.gotoTok(tok_begin);
        exec_line(tokens, content, true);
        idx.clear();
    } else {
        throw parsing_error(fmt::format("Funzione sistema {0} non riconosciuta", fun_name), tokens.getLocation(fun_opening));
    }
}

variable_ref reader::get_variable(tokenizer &tokens, const box_content &content) {
    variable_ref ref(*this);
    while (tokens.next()) {
        switch (tokens.current().type) {
        case TOK_GLOBAL:
            ref.flags |= variable_ref::FLAGS_GLOBAL;
            break;
        case TOK_DEBUG:
            ref.flags |= variable_ref::FLAGS_DEBUG;
            break;
        case TOK_PERCENT:
            ref.flags |= variable_ref::FLAGS_NUMBER;
            break;
        default:
            goto out_of_loop;
        }
    }
out_of_loop:
    if (tokens.current().type != TOK_IDENTIFIER) {
        throw parsing_error("Richiesto identificatore", tokens.getLocation(tokens.current()));
    }
    ref.name = tokens.current().value;
    if (ref.flags & variable_ref::FLAGS_GLOBAL) {
        return ref;
    }

    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_BRACKET_BEGIN:
        tokens.advance();
        ref.index = evaluate(tokens, content).asInt();
        tokens.next();
        if (tokens.current().type != TOK_BRACKET_END) {
            throw parsing_error("Richiesto ']'", tokens.getLocation(tokens.current()));
        }
        break;
    case TOK_APPEND:
        ref.flags |= variable_ref::FLAGS_APPEND;
        tokens.advance();
        break;
    case TOK_CLEAR:
        ref.flags |= variable_ref::FLAGS_CLEAR;
        tokens.advance();
        break;
    default:
        break;
    }

    return ref;
}

const variable &reader::get_global(const std::string &name) const {
    static const variable VAR_EMPTY;

    auto it = m_globals.find(name);
    if (it == m_globals.end()) {
        return VAR_EMPTY;
    } else {
        return it->second;
    }
}

std::ostream &reader::print_output(std::ostream &out, bool debug) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::arrayValue;

    for (auto &page : m_values) {
        auto &page_values = values.append(Json::objectValue);
        for (auto &pair : page) {
            if(!debug && pair.second.front().debug) continue;
            auto &json_arr = page_values[pair.first] = Json::arrayValue;
            for (auto &val : pair.second) {
                json_arr.append(val.str());
            }
        }
    }

    Json::Value &globals = root["globals"] = Json::objectValue;
    for (auto &pair : m_globals) {
        if (!debug && pair.second.debug) continue;
        globals[pair.first] = pair.second.str();
    }

    return out << root;
}