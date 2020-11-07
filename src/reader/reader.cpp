#include "reader.h"

#include <json/json.h>
#include <fmt/format.h>
#include <iostream>
#include "../shared/utils.h"

void reader::read_layout(const pdf_info &info, const bill_layout_script &layout) {
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        if (!layout.boxes[i].goto_label.empty()) {
            goto_labels[layout.boxes[i].goto_label] = i;
        }
    }
    try {
        program_counter = 0;
        while (program_counter < layout.boxes.size()) {
            read_box(info, layout.boxes[program_counter]);
            if (jumped) {
                jumped = false;
            } else {
                ++program_counter;
            }
        }
    } catch (const parsing_error &error) {
        throw layout_error(fmt::format("In {0}: {1}\n{2}", layout.boxes[program_counter].name, error.message, error.line));
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
    default:
    {
        variable_ref ref = get_variable(tokens, content);
        tokens.next(true);
        switch (tokens.current().type) {
        case TOK_EQUALS:
            tokens.advance();
            return add_value(ref, evaluate(tokens, content, ignore), ignore);
        case TOK_END_OF_FILE:
            break;
        default:
            return add_value(ref, content.text, ignore);
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
    default:
    {
        auto ref = get_variable(tokens, content);
        return ref.isset() ? *ref : variable();
    }
    }
    return variable();
}

variable_ref reader::get_variable(tokenizer &tokens, const box_content &content) {
    variable_ref ref(*this);
    bool in_loop = true;
    while (in_loop && tokens.next()) {
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
            in_loop = false;
        }
    }

    if (tokens.current().type != TOK_IDENTIFIER) {
        throw parsing_error("Richiesto identificatore", tokens.getLocation(tokens.current()));
    }
    ref.name = tokens.current().value;
    ref.pageidx = get_global("PAGE_NUM").asInt();
    if (ref.flags & variable_ref::FLAGS_GLOBAL) {
        return ref;
    }

    tokens.next(true);
    switch (tokens.current().type) {
    case TOK_BRACKET_BEGIN:
        tokens.advance();
        ref.index = evaluate(tokens, content).asInt();
        tokens.require(TOK_BRACKET_END);
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