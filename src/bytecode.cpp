#include "bytecode.h"

#include <fmt/core.h>
#include <vector>
#include <map>

#include "utils.h"
#include "fixed_point.h"

bytecode::bytecode(const std::vector<std::string> &lines) {
    std::map<std::string, jump_address> labels;

    for (size_t i=0; i<lines.size(); ++i) {
        if (lines[i].starts_with("LABEL")) {
            labels[lines[i].substr(6)] = i - labels.size();
        }
    }

    auto getgotoindex = [&](const std::string &label) -> jump_address {
        auto it = labels.find(label);
        if (it == labels.end()) {
            throw assembly_error(fmt::format("Etichetta sconosciuta: {}", label));
        } else {
            return it->second - size();
        }
    };

    try {
        for (auto &line : lines) {
            size_t space_pos = line.find_first_of(' ');
            auto cmd_name = line.substr(0, space_pos);
            if (cmd_name == "LABEL") continue;

            auto arg_str = line.substr(space_pos + 1);
            auto args = string_split(arg_str, ',');

            auto it = std::find(std::begin(opcode_names), std::end(opcode_names), cmd_name);
            if (it == std::end(opcode_names)) {
                throw assembly_error(fmt::format("Comando sconosciuto: {}", cmd_name));
            }
            auto cmd = static_cast<opcode>(it - opcode_names);
            switch (cmd) {
            case opcode::COMMENT:
            case opcode::IMPORT:
                emplace_back(cmd, arg_str);
                break;
            case opcode::RDBOX:
            {
                pdf_rect box;
                box.type = box_type::BOX_RECTANGLE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                box.page = cstoi(args[1]);
                box.x = cstof(args[2]);
                box.y = cstof(args[3]);
                box.w = cstof(args[4]);
                box.h = cstof(args[5]);
                emplace_back(opcode::RDBOX, std::move(box));
                break;
            }
            case opcode::RDPAGE:
            {
                pdf_rect box;
                box.type = box_type::BOX_PAGE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                box.page = cstoi(args[1]);
                emplace_back(opcode::RDPAGE, std::move(box));
                break;
            }
            case opcode::RDFILE:
            {
                pdf_rect box;
                box.type = box_type::BOX_FILE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                emplace_back(opcode::RDFILE, std::move(box));
                break;
            }
            case opcode::SETPAGE:
                emplace_back(opcode::SETPAGE, static_cast<small_int>(cstoi(args[0])));
                break;
            case opcode::CALL:
            {
                command_call call;
                call.name = args[0];
                call.numargs = cstoi(args[1]);
                emplace_back(opcode::CALL, std::move(call));
                break;
            }
            case opcode::MVBOX:
                emplace_back(opcode::MVBOX, static_cast<spacer_index>(cstoi(arg_str)));
                break;
            case opcode::SELVAR:
                emplace_back(opcode::SELVAR, variable_idx(args[0], cstoi(args[1])));
                break;
            case opcode::SELRANGE:
                emplace_back(opcode::SELRANGE, variable_idx(args[0], cstoi(args[1]), cstoi(args[2])));
                break;
            case opcode::SELVARTOP:
            case opcode::SELRANGETOP:
            case opcode::SELRANGEALL:
                emplace_back(cmd, args[0]);
                break;
            case opcode::PUSHNUM:
                emplace_back(opcode::PUSHNUM, fixed_point(args[0]));
                break;
            case opcode::PUSHSTR:
            {
                std::string str;
                if (!(arg_str.front() == '"' ? parse_string_token : parse_regexp_token)(str, arg_str)) {
                    throw assembly_error(fmt::format("Stringa non valida: {}", arg_str));
                }
                emplace_back(opcode::PUSHSTR, str);
                break;
            }
            case opcode::JMP:
            case opcode::JSR:
            case opcode::JZ:
            case opcode::JNZ:
            case opcode::JTE:
                emplace_back(cmd, getgotoindex(args[0]));
                break;
            default:
                emplace_back(cmd);
                break;
            }
        }
    } catch (const std::invalid_argument &error) {
        throw assembly_error(error.what());
    }
}