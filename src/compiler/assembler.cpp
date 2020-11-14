#include "assembler.h"

#include <fmt/core.h>
#include <vector>
#include <utility>
#include <map>

#include "../shared/utils.h"

void assembler::read_lines(const std::vector<std::string> &lines) {
    std::map<std::string, jump_address> labels;

    for (size_t i=0; i<lines.size(); ++i) {
        if (lines[i].back() == ':') {
            labels[lines[i].substr(0, lines[i].size()-1)] = i - labels.size();
        }
    }

    auto getgotoindex = [&](const std::string &label) -> jump_address {
        try {
            return std::stoi(label);
        } catch (std::invalid_argument &) {
            auto it = labels.find(label);
            if (it == labels.end()) {
                throw assembly_error{fmt::format("Etichetta sconosciuta: {0}", label)};
            } else {
                return it->second;
            }
        }
    };

    for (auto &line : lines) {
        if (line.back() == ':') continue;
        
        size_t space_pos = line.find_first_of(' ');
        std::string cmd = line.substr(0, space_pos);
        auto arg_str = line.substr(space_pos + 1);
        auto args = string_split(arg_str, ',');
        switch (hash(cmd)) {
        case hash("RDBOX"):
        {
            command_box box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(std::stoi(args[0]));
            box.page = std::stoi(args[1]);
            box.ref_spacers = add_string(args[2]);
            box.x = std::stof(args[3]);
            box.y = std::stof(args[4]);
            box.w = std::stof(args[5]);
            box.h = std::stof(args[6]);
            add_command(RDBOX, std::move(box));
            break;
        }
        case hash("RDPAGE"):
        {
            command_box box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(std::stoi(args[0]));
            box.page = std::stoi(args[1]);
            box.ref_spacers = add_string(args[2]);
            add_command(RDPAGE, std::move(box));
            break;
        }
        case hash("RDFILE"):
        {
            layout_box box;
            box.type = BOX_WHOLE_FILE;
            box.mode = static_cast<read_mode>(std::stoi(args[0]));
            add_command(RDFILE, std::move(box));
            break;
        }
        case hash("CALL"):
        {
            command_call call;
            call.name = add_string(args[0]);
            call.numargs = std::stoi(args[1]);
            add_command(CALL, std::move(call));
            break;
        }
        case hash("SPACER"):
        {
            command_spacer spacer;
            spacer.name = add_string(args[0]);
            spacer.w = std::stof(args[1]);
            spacer.h = std::stof(args[2]);
            add_command(SPACER, std::move(spacer));
            break;
        }
        case hash("ERROR"):         add_command(ERROR, add_string(parse_string(arg_str))); break;
        case hash("PARSENUM"):      add_command(PARSENUM); break;
        case hash("PARSEINT"):      add_command(PARSEINT); break;
        case hash("EQ"):            add_command(EQ); break;
        case hash("NEQ"):           add_command(NEQ); break;
        case hash("AND"):           add_command(AND); break;
        case hash("OR"):            add_command(OR); break;
        case hash("NOT"):           add_command(NOT); break;
        case hash("ADD"):           add_command(ADD); break;
        case hash("SUB"):           add_command(SUB); break;
        case hash("MUL"):           add_command(MUL); break;
        case hash("DIV"):           add_command(DIV); break;
        case hash("GT"):            add_command(GT); break;
        case hash("LT"):            add_command(LT); break;
        case hash("GEQ"):           add_command(GEQ); break;
        case hash("LEQ"):           add_command(LEQ); break;
        case hash("MAX"):           add_command(MAX); break;
        case hash("MIN"):           add_command(MIN); break;
        case hash("SELGLOBAL"):     add_command(SELGLOBAL, add_string(args[0])); break;
        case hash("SELVAR"):        add_command(SELVAR, add_string(args[0])); break;
        case hash("SELVARIDX"):     add_command(SELVARIDX, variable_idx(add_string(args[0]), std::stoi(args[1]))); break;
        case hash("SETDEBUG"):      add_command(SETDEBUG); break;
        case hash("CLEAR"):         add_command(CLEAR); break;
        case hash("APPEND"):        add_command(APPEND); break;
        case hash("SETVAR"):        add_command(SETVAR); break;
        case hash("RESETVAR"):      add_command(RESETVAR); break;
        case hash("COPYCONTENT"):   add_command(COPYCONTENT); break;
        case hash("PUSHINT"):       add_command(PUSHINT, std::stoi(args[0])); break;
        case hash("PUSHFLOAT"):     add_command(PUSHFLOAT, std::stof(args[0])); break;
        case hash("PUSHSTR"):       add_command(PUSHSTR, add_string(parse_string(arg_str))); break;
        case hash("PUSHVAR"):       add_command(PUSHVAR); break;
        case hash("JMP"):           add_command(JMP, getgotoindex(args[0])); break;
        case hash("JZ"):            add_command(JZ, getgotoindex(args[0])); break;
        case hash("JNZ"):            add_command(JNZ, getgotoindex(args[0])); break;
        case hash("JTE"):           add_command(JTE, getgotoindex(args[0])); break;
        case hash("INCTOP"):        add_command(INCTOP); break;
        case hash("INC"):           add_command(INC, static_cast<small_int>(std::stoi(args[0]))); break;
        case hash("DECTOP"):        add_command(DECTOP); break;
        case hash("DEC"):           add_command(DEC, static_cast<small_int>(std::stoi(args[0]))); break;
        case hash("ISSET"):         add_command(ISSET); break;
        case hash("SIZE"):          add_command(SIZE); break;
        case hash("PUSHCONTENT"):   add_command(PUSHCONTENT); break;
        case hash("NEXTLINE"):      add_command(NEXTLINE); break;
        case hash("NEXTTOKEN"):     add_command(NEXTTOKEN); break;
        case hash("POPCONTENT"):    add_command(POPCONTENT); break;
        case hash("NEXTPAGE"):      add_command(NEXTPAGE); break;
        case hash("HLT"):           add_command(HLT); break;
        default:
            throw assembly_error{fmt::format("Comando sconosciuto: {0}", cmd)};
        }
    }
}

string_ref assembler::add_string(const std::string &str) {
    if (str.empty()) {
        return 0xffff;
    }
    auto it = std::find(out_strings.begin(), out_strings.end(), str);
    if (it != out_strings.end()) {
        return it - out_strings.begin();
    } else {
        out_strings.push_back(str);
        return out_strings.size() - 1;
    }
}

void assembler::write_bytecode(std::ostream &output) {
    auto write_data = [&output](const auto &data) {
        output.write(reinterpret_cast<const char *>(&data), sizeof(data));
    };

    for (const auto &line : out_lines) {
        write_data(static_cast<command_int>(line.command));
        switch (line.command) {
        case RDBOX:
        {
            const auto &box = line.get<command_box>();
            write_data(static_cast<small_int>(box.mode));
            write_data(static_cast<small_int>(box.page));
            write_data(box.ref_spacers);
            write_data(box.x);
            write_data(box.y);
            write_data(box.w);
            write_data(box.h);
            break;
        }
        case RDPAGE:
        {
            const auto &box = line.get<command_box>();
            write_data(static_cast<small_int>(box.mode));
            write_data(static_cast<small_int>(box.page));
            write_data(box.ref_spacers);
            break;
        }
        case RDFILE:
        {
            const auto &box = line.get<command_box>();
            write_data(static_cast<small_int>(box.mode));
            break;
        }
        case CALL:
        {
            const auto &call = line.get<command_call>();
            write_data(call.name);
            write_data(call.numargs);
            break;
        }
        case SPACER:
        {
            const auto &spacer = line.get<command_spacer>();
            write_data(spacer.name);
            write_data(spacer.w);
            write_data(spacer.h);
            break;
        }
        case SELVARIDX:
        {
            const auto &var_idx = line.get<variable_idx>();
            write_data(var_idx.name);
            write_data(var_idx.index);
            break;
        }
        case STRDATA:
        {
            const auto &str = line.get<std::string>();
            write_data(static_cast<string_size>(str.size()));
            output.write(str.c_str(), str.size());
            break;
        }
        case ERROR:
        case PUSHSTR:
        case SELVAR:
        case SELGLOBAL:
            write_data(line.get<string_ref>());
            break;
        case PUSHFLOAT:
            write_data(line.get<float>());
            break;
        case PUSHINT:
        case INC:
        case DEC:
            write_data(line.get<small_int>());
            break;
        case JMP:
        case JZ:
        case JNZ:
        case JTE:
            write_data(line.get<jump_address>());
            break;
        default:
            break;
        }
    }

    for (const auto &str : out_strings) {
        write_data(static_cast<command_int>(STRDATA));
        write_data(static_cast<string_size>(str.size()));
        output.write(str.c_str(), str.size());
    }
}