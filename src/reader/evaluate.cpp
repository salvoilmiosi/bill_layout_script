
#if 0

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
#endif