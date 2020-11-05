#include "reader.h"

#include <json/json.h>

#include <functional>

#include <fmt/format.h>

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

variable reader::add_value(variable_ref ref, variable value) {
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

variable reader::exec_line(tokenizer &tokens, const box_content &content) {
    token next;
    tokens.nextToken(next, false);
    if (next.type == TOK_FUNCTION) {
        auto var = exec_function(tokens, content);
        tokens.nextToken(next);
        if (next.type != TOK_END_OF_FILE && next.type != TOK_END_EXPR) {
            throw parsing_error("Token imprevisto", tokens.getLocation(next));
        }
        return var;
    } else {
        variable_ref ref = get_variable(tokens, content);
        tokens.nextToken(next);
        switch (next.type) {
        case TOK_EQUALS:
        {
            auto var = evaluate(tokens, content);
            tokens.nextToken(next);
            if (next.type != TOK_END_OF_FILE && next.type != TOK_END_EXPR) {
                throw parsing_error("Token imprevisto", tokens.getLocation(next));
            }
            add_value(ref, var);
            return var;
        }
        case TOK_END_EXPR:
        case TOK_END_OF_FILE:
            return add_value(ref, content.text);
        default:
            throw parsing_error("Token imprevisto", tokens.getLocation(next));
        }
    }
    return variable();
}

variable reader::evaluate(tokenizer &tokens, const box_content &content) {
    token next;
    tokens.nextToken(next, false);
    switch (next.type) {
    case TOK_FUNCTION:
        return exec_function(tokens, content);
    case TOK_NUMBER:
        tokens.advance(next);
        return variable(std::string(next.value), VALUE_NUMBER);
    case TOK_STRING:
        tokens.advance(next);
        return parse_string(next.value);
    case TOK_CONTENT:
        tokens.advance(next);
        return content.text;
    default:
        throw parsing_error("Token imprevisto", tokens.getLocation(next));
    }
    return variable();
}

using arg_list = std::vector<variable>;

struct function_handler {
    size_t min_args;
    size_t max_args;
    std::function<variable(const arg_list &)> handler;
};

variable reader::exec_function(tokenizer &tokens, const box_content &content) {
    token next;
    tokens.nextToken(next);
    if (next.type != TOK_FUNCTION) {
        throw parsing_error("Previsto '$'", tokens.getLocation(next));
    }

    tokens.nextToken(next);
    if (next.type != TOK_IDENTIFIER) {
        throw parsing_error("Previsto identificatore", tokens.getLocation(next));
    }

    token fun_opening = next;
    std::string fun_name = std::string(next.value);
    arg_list args;

    tokens.nextToken(next);
    if (next.type != TOK_BRACE_BEGIN) {
        throw parsing_error("Previsto '('", tokens.getLocation(next));
    }

    bool in_fun_loop = true;
    while (in_fun_loop) {
        args.push_back(evaluate(tokens, content));
        tokens.nextToken(next);
        switch (next.type) {
        case TOK_COMMA:
            break;
        case TOK_BRACE_END:
            in_fun_loop=false;
            break;
        default:
            throw parsing_error("Token imprevisto", tokens.getLocation(next));
        }
    }

    static const std::map<std::string, function_handler> dispatcher = {
        {"search", {1, 3, [&](const arg_list &args) {
            return search_regex(args[0].str(),
                args.size() >= 2 ? content.text : args[1].str(),
                args.size() >= 3 ? args[2].asInt() : 0);
        }}},
        {"if", {2, 3, [&](const arg_list &args) {
            if (args[0]) return args[1];
            else if (args.size() >= 3) return args[2];
            return variable();
        }}},
        {"contains", {2, 2, [&](const arg_list &args) {
            return args[0].str().find(args[1].str()) != std::string::npos;
        }}},
        {"error", {1, 1, [&](const arg_list &args) {
            throw parsing_error(args[0].str(), tokens.getLocation(fun_opening));
            return variable();
        }}}
    };

    auto it = dispatcher.find(fun_name);
    if (it != dispatcher.end()) {
        if (args.size() >= it->second.min_args && args.size() <= it->second.max_args) {
            return it->second.handler(args);
        } else if (it->second.min_args == it->second.max_args) {
            throw parsing_error(fmt::format("La funzione {0} richiede {1} argomenti", fun_name, it->second.min_args), tokens.getLocation(fun_opening));
        } else if (it->second.max_args != (size_t) -1) {
            throw parsing_error(fmt::format("La funzione {0} richiede tra {1} e {2} argomenti", fun_name, it->second.min_args, it->second.max_args), tokens.getLocation(fun_opening));
        } else {
            throw parsing_error(fmt::format("La funzione {0} richiede minimo {1} argomenti", fun_name, it->second.min_args), tokens.getLocation(fun_opening));
        }
    } else {
        throw parsing_error(fmt::format("Funzione non riconosciuta: {0}", fun_name), tokens.getLocation(fun_opening));
    }

    return variable();
}

variable_ref reader::get_variable(tokenizer &tokens, const box_content &content) {
    variable_ref ref(*this);
    token next;
    while (tokens.nextToken(next)) {
        switch (next.type) {
        case TOK_GLOBAL:
            ref.flags |= variable_ref::FLAGS_GLOBAL;
            break;
        case TOK_DEBUG:
            ref.flags |= variable_ref::FLAGS_DEBUG;
            break;
        case TOK_NUMBER:
            ref.flags |= variable_ref::FLAGS_NUMBER;
            break;
        default:
            goto out_of_loop;
        }
    }
out_of_loop:
    if (next.type != TOK_IDENTIFIER) {
        throw parsing_error("Richiesto identificatore", tokens.getLocation(next));
    }
    ref.name = next.value;
    if (ref.flags & variable_ref::FLAGS_GLOBAL) {
        return ref;
    }

    tokens.nextToken(next, false);
    switch (next.type) {
    case TOK_BRACKET_BEGIN:
        ref.index = evaluate(tokens, content).asInt();
        tokens.advance(next);
        tokens.nextToken(next);
        if (next.type != TOK_BRACKET_END) {
            throw parsing_error("Richiesto ']'", tokens.getLocation(next));
        }
        break;
    case TOK_APPEND:
        ref.flags |= variable_ref::FLAGS_APPEND;
        tokens.advance(next);
        break;
    case TOK_CLEAR:
        ref.flags |= variable_ref::FLAGS_CLEAR;
        tokens.advance(next);
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