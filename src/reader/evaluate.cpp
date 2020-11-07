
#if 0

case hash("clear"):
    if (fun_is(1, 1)) get_variable(function.args[0], content).clear();
    break;
case hash("size"):
    if (fun_is(1, 1)) return get_variable(function.args[0], content).size();
    break;
case hash("isset"):
    if (fun_is(1, 1)) return get_variable(function.args[0], content).isset();
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
#endif