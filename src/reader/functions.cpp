#include "reader.h"

#include <functional>
#include <fmt/format.h>
#include "../shared/utils.h"

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
    tokens.require(TOK_FUNCTION);
    token fun_opening = tokens.require(TOK_IDENTIFIER);

    std::string fun_name = std::string(tokens.current().value);
    arg_list args;

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
        {"ifl", create_function<2, 3>   ([](const variable &condition, const variable &var_if, const variable &var_else) { return condition ? var_if : var_else; })},
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
        tokens.require(TOK_PAREN_BEGIN);

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
        tokens.gotoTok(fun_opening);
        return exec_sys_function(tokens, content, ignore);
    }

    return variable();
}

variable reader::exec_sys_function(tokenizer &tokens, const box_content &content, bool ignore) {
    token fun_opening = tokens.require(TOK_IDENTIFIER);
    std::string fun_name = std::string(fun_opening.value);

    if (fun_name == "if") {
        tokens.require(TOK_PAREN_BEGIN);
        auto condition = evaluate(tokens, content, ignore);
        tokens.require(TOK_PAREN_END);
        exec_line(tokens, content, ignore || !condition);
    } else if (fun_name == "while") {
        tokens.require(TOK_PAREN_BEGIN);

        token tok_condition = tokens.current();
        auto condition = evaluate(tokens, content, true);

        tokens.require(TOK_PAREN_END);

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
    } else if (fun_name == "for") {
        tokens.require(TOK_PAREN_BEGIN);
        auto idx = get_variable(tokens, content);
        tokens.require(TOK_COMMA);
        size_t num = evaluate(tokens, content).asInt();
        tokens.require(TOK_PAREN_END);

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
    } else if (fun_name == "lines") {
        tokens.require(TOK_PAREN_BEGIN);
        auto str = evaluate(tokens, content, ignore);
        tokens.require(TOK_PAREN_END);

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
        tokens.require(TOK_PAREN_BEGIN);
        auto str = evaluate(tokens, content, ignore);
        tokens.require(TOK_PAREN_END);

        auto strtoks = tokenize(str.str());

        tokens.require(TOK_BRACE_BEGIN);

        box_content content_strtok = content;
        for (size_t i=0;; ++i) {
            tokens.next(true);
            if (tokens.current().type == TOK_BRACE_END) {
                tokens.advance();
                break;
            }
            content_strtok.text = i < strtoks.size() ? strtoks[i] : "";
            exec_line(tokens, content_strtok, ignore);
        }
    } else if (fun_name == "goto") {
        tokens.next();

        if (!ignore) {        
            switch (tokens.current().type) {
            case TOK_IDENTIFIER:
            {
                std::string label(tokens.current().value);
                auto it = goto_labels.find(label);
                if (it == goto_labels.end()) {
                    throw parsing_error(fmt::format("Impossibile trovare etichetta {0}", label), tokens.getLocation(tokens.current()));
                } else {
                    program_counter = it->second;
                    jumped = true;
                }
                break;
            }
            case TOK_NUMBER:
                program_counter = variable(tokens.current().value, VALUE_NUMBER).asInt();
                jumped = true;
                break;
            default:
                throw parsing_error("Indirizzo goto invalido", tokens.getLocation(tokens.current()));
            }
        }
    } else if (fun_name == "inc") {
        tokens.require(TOK_PAREN_BEGIN);
        auto var = get_variable(tokens, content);
        tokens.next();
        variable amt = 1;
        switch(tokens.current().type) {
        case TOK_COMMA:
            amt = evaluate(tokens, content, ignore);
            tokens.require(TOK_PAREN_END);
            break;
        case TOK_PAREN_END:
            break;
        default:
            throw parsing_error("Token inaspettato", tokens.getLocation(tokens.current()));
        }
        if (!ignore && amt) *var = *var + amt;
    } else if (fun_name == "dec") {
        tokens.require(TOK_PAREN_BEGIN);
        auto var = get_variable(tokens, content);
        tokens.next();
        variable amt = 1;
        switch(tokens.current().type) {
        case TOK_COMMA:
            amt = evaluate(tokens, content, ignore);
            tokens.require(TOK_PAREN_END);
            break;
        case TOK_PAREN_END:
            break;
        default:
            throw parsing_error("Token inaspettato", tokens.getLocation(tokens.current()));
        }
        if (!ignore && amt) *var = *var - amt;
    } else if (fun_name == "isset") {
        tokens.require(TOK_PAREN_BEGIN);
        auto var = get_variable(tokens, content);
        tokens.require(TOK_PAREN_END);
        return var.isset();
    } else if (fun_name == "size") {
        tokens.require(TOK_PAREN_BEGIN);
        auto var = get_variable(tokens, content);
        tokens.require(TOK_PAREN_END);
        return var.size();
    } else if (fun_name == "clear") {
        tokens.next();
        auto var = get_variable(tokens, content);
        var.clear();
    } else if (fun_name == "addspacer") {
        tokens.require(TOK_IDENTIFIER);
        if (!ignore) {
            m_spacers[std::string(tokens.current().value)] = spacer(content.w, content.h);
        }
    } else {
        throw parsing_error(fmt::format("Funzione {0} non riconosciuta", fun_name), tokens.getLocation(fun_opening));
    }
    return variable();
}