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
            throw assembly_error(fmt::format("Etichetta sconosciuta: {0}", label));
        } else {
            return it->second - size();
        }
    };

    try {
        for (auto &line : lines) {
            size_t space_pos = line.find_first_of(' ');
            auto cmd = line.substr(0, space_pos);
            auto arg_str = line.substr(space_pos + 1);
            auto args = string_split(arg_str, ',');
            switch (hash(cmd)) {
            case hash("LABEL"):
                break;
            case hash("COMMENT"):
                emplace_back(opcode::COMMENT, arg_str);
                break;
            case hash("RDBOX"):
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
            case hash("RDPAGE"):
            {
                pdf_rect box;
                box.type = box_type::BOX_PAGE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                box.page = cstoi(args[1]);
                emplace_back(opcode::RDPAGE, std::move(box));
                break;
            }
            case hash("RDFILE"):
            {
                pdf_rect box;
                box.type = box_type::BOX_FILE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                emplace_back(opcode::RDFILE, std::move(box));
                break;
            }
            case hash("SETPAGE"):
            {
                emplace_back(opcode::SETPAGE, static_cast<small_int>(cstoi(args[0])));
                break;
            }
            case hash("CALL"):
            {
                command_call call;
                call.name = args[0];
                call.numargs = cstoi(args[1]);
                emplace_back(opcode::CALL, std::move(call));
                break;
            }
            case hash("MVBOX"):         emplace_back(opcode::MVBOX, static_cast<spacer_index>(cstoi(arg_str))); break;
            case hash("THROWERR"):      emplace_back(opcode::THROWERR); break;
            case hash("ADDWARNING"):    emplace_back(opcode::ADDWARNING); break;
            case hash("PARSENUM"):      emplace_back(opcode::PARSENUM); break;
            case hash("PARSEINT"):      emplace_back(opcode::PARSEINT); break;
            case hash("EQ"):            emplace_back(opcode::EQ); break;
            case hash("NEQ"):           emplace_back(opcode::NEQ); break;
            case hash("AND"):           emplace_back(opcode::AND); break;
            case hash("OR"):            emplace_back(opcode::OR); break;
            case hash("NOT"):           emplace_back(opcode::NOT); break;
            case hash("NEG"):           emplace_back(opcode::NEG); break;
            case hash("ADD"):           emplace_back(opcode::ADD); break;
            case hash("SUB"):           emplace_back(opcode::SUB); break;
            case hash("MUL"):           emplace_back(opcode::MUL); break;
            case hash("DIV"):           emplace_back(opcode::DIV); break;
            case hash("GT"):            emplace_back(opcode::GT); break;
            case hash("LT"):            emplace_back(opcode::LT); break;
            case hash("GEQ"):           emplace_back(opcode::GEQ); break;
            case hash("LEQ"):           emplace_back(opcode::LEQ); break;
            case hash("SELVAR"):        emplace_back(opcode::SELVAR, variable_idx(args[0], cstoi(args[1]))); break;
            case hash("SELVARTOP"):     emplace_back(opcode::SELVARTOP, args[0]); break;
            case hash("SELRANGE"):      emplace_back(opcode::SELRANGE, variable_idx(args[0], cstoi(args[1]), cstoi(args[2]))); break;
            case hash("SELRANGETOP"):   emplace_back(opcode::SELRANGETOP, args[0]); break;
            case hash("SELRANGEALL"):   emplace_back(opcode::SELRANGEALL, args[0]); break;
            case hash("CLEAR"):         emplace_back(opcode::CLEAR); break;
            case hash("SETVAR"):        emplace_back(opcode::SETVAR); break;
            case hash("RESETVAR"):      emplace_back(opcode::RESETVAR); break;
            case hash("PUSHVIEW"):      emplace_back(opcode::PUSHVIEW); break;
            case hash("PUSHNUM"): {
                if (args[0].find('.') == std::string::npos) {
                    int32_t num = cstoi(args[0]);
                    if (static_cast<int8_t>(num) == num) {
                        emplace_back(opcode::PUSHBYTE, static_cast<int8_t>(num));
                    } else if (static_cast<int16_t>(num) == num) {
                        emplace_back(opcode::PUSHSHORT, static_cast<int16_t>(num));
                    } else {
                        emplace_back(opcode::PUSHINT, num);
                    }
                } else {
                    emplace_back(opcode::PUSHDECIMAL, fixed_point(args[0]));
                }
                break;
            }
            case hash("PUSHSTR"):
            {
                std::string str;
                if (!(arg_str.front() == '"' ? parse_string : parse_string_regexp)(str, arg_str)) {
                    throw assembly_error(fmt::format("Stringa non valida: {0}", arg_str));
                }
                emplace_back(opcode::PUSHSTR, str);
                break;
            }
            case hash("PUSHVAR"):       emplace_back(opcode::PUSHVAR); break;
            case hash("MOVEVAR"):       emplace_back(opcode::MOVEVAR); break;
            case hash("JMP"):           emplace_back(opcode::JMP, getgotoindex(args[0])); break;
            case hash("JSR"):           emplace_back(opcode::JMP, getgotoindex(args[0])); break;
            case hash("JZ"):            emplace_back(opcode::JZ,  getgotoindex(args[0])); break;
            case hash("JNZ"):           emplace_back(opcode::JNZ, getgotoindex(args[0])); break;
            case hash("JTE"):           emplace_back(opcode::JTE, getgotoindex(args[0])); break;
            case hash("INC"):           emplace_back(opcode::INC); break;
            case hash("DEC"):           emplace_back(opcode::DEC); break;
            case hash("ISSET"):         emplace_back(opcode::ISSET); break;
            case hash("GETSIZE"):       emplace_back(opcode::GETSIZE); break;
            case hash("MOVCONTENT"):    emplace_back(opcode::MOVCONTENT); break;
            case hash("SETBEGIN"):      emplace_back(opcode::SETBEGIN); break;
            case hash("SETEND"):        emplace_back(opcode::SETEND); break;
            case hash("NEWVIEW"):       emplace_back(opcode::NEWVIEW); break;
            case hash("NEWTOKENS"):     emplace_back(opcode::NEWTOKENS); break;
            case hash("RESETVIEW"):     emplace_back(opcode::RESETVIEW); break;
            case hash("NEXTLINE"):      emplace_back(opcode::NEXTLINE); break;
            case hash("NEXTTOKEN"):     emplace_back(opcode::NEXTTOKEN); break;
            case hash("POPCONTENT"):    emplace_back(opcode::POPCONTENT); break;
            case hash("NEXTTABLE"):     emplace_back(opcode::NEXTTABLE); break;
            case hash("ATE"):           emplace_back(opcode::ATE); break;
            case hash("RET"):           emplace_back(opcode::RET); break;
            case hash("IMPORT"):        emplace_back(opcode::IMPORT, arg_str); break;
            default:
                throw assembly_error(fmt::format("Comando sconosciuto: {0}", cmd));
            }
        }
    } catch (const std::invalid_argument &error) {
        throw assembly_error(error.what());
    }
}