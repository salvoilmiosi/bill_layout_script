
#if 0

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