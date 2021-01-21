#include "bytecode.h"

#include <fmt/core.h>
#include <vector>
#include <map>

#include "utils.h"
#include "binary_io.h"
#include "parsestr.h"
#include "fixed_point.h"

bytecode bytecode::from_lines(const std::vector<std::string> &lines) {
    bytecode ret;
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
            return it->second - ret.size();
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
                ret.add_command(opcode::COMMENT, arg_str);
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
                ret.add_command(opcode::RDBOX, std::move(box));
                break;
            }
            case hash("RDPAGE"):
            {
                pdf_rect box;
                box.type = box_type::BOX_PAGE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                box.page = cstoi(args[1]);
                ret.add_command(opcode::RDPAGE, std::move(box));
                break;
            }
            case hash("RDFILE"):
            {
                pdf_rect box;
                box.type = box_type::BOX_FILE;
                box.mode = static_cast<read_mode>(cstoi(args[0]));
                ret.add_command(opcode::RDFILE, std::move(box));
                break;
            }
            case hash("SETPAGE"):
            {
                ret.add_command(opcode::SETPAGE, static_cast<small_int>(cstoi(args[0])));
                break;
            }
            case hash("CALL"):
            {
                command_call call;
                call.name = args[0];
                call.numargs = cstoi(args[1]);
                ret.add_command(opcode::CALL, std::move(call));
                break;
            }
            case hash("MVBOX"):         ret.add_command(opcode::MVBOX, static_cast<spacer_index>(cstoi(arg_str))); break;
            case hash("THROWERR"):      ret.add_command(opcode::THROWERR); break;
            case hash("ADDWARNING"):    ret.add_command(opcode::ADDWARNING); break;
            case hash("PARSENUM"):      ret.add_command(opcode::PARSENUM); break;
            case hash("PARSEINT"):      ret.add_command(opcode::PARSEINT); break;
            case hash("EQ"):            ret.add_command(opcode::EQ); break;
            case hash("NEQ"):           ret.add_command(opcode::NEQ); break;
            case hash("AND"):           ret.add_command(opcode::AND); break;
            case hash("OR"):            ret.add_command(opcode::OR); break;
            case hash("NOT"):           ret.add_command(opcode::NOT); break;
            case hash("NEG"):           ret.add_command(opcode::NEG); break;
            case hash("ADD"):           ret.add_command(opcode::ADD); break;
            case hash("SUB"):           ret.add_command(opcode::SUB); break;
            case hash("MUL"):           ret.add_command(opcode::MUL); break;
            case hash("DIV"):           ret.add_command(opcode::DIV); break;
            case hash("GT"):            ret.add_command(opcode::GT); break;
            case hash("LT"):            ret.add_command(opcode::LT); break;
            case hash("GEQ"):           ret.add_command(opcode::GEQ); break;
            case hash("LEQ"):           ret.add_command(opcode::LEQ); break;
            case hash("SELVAR"):        ret.add_command(opcode::SELVAR, variable_idx(args[0], cstoi(args[1]))); break;
            case hash("SELVARTOP"):     ret.add_command(opcode::SELVARTOP, args[0]); break;
            case hash("SELRANGE"):      ret.add_command(opcode::SELRANGE, variable_idx(args[0], cstoi(args[1]), cstoi(args[2]))); break;
            case hash("SELRANGETOP"):   ret.add_command(opcode::SELRANGETOP, args[0]); break;
            case hash("SELRANGEALL"):   ret.add_command(opcode::SELRANGEALL, args[0]); break;
            case hash("CLEAR"):         ret.add_command(opcode::CLEAR); break;
            case hash("SETVAR"):        ret.add_command(opcode::SETVAR); break;
            case hash("RESETVAR"):      ret.add_command(opcode::RESETVAR); break;
            case hash("PUSHVIEW"):      ret.add_command(opcode::PUSHVIEW); break;
            case hash("PUSHNUM"): {
                if (args[0].find('.') == std::string::npos) {
                    int32_t num = cstoi(args[0]);
                    if (static_cast<int8_t>(num) == num) {
                        ret.add_command(opcode::PUSHBYTE, static_cast<int8_t>(num));
                    } else if (static_cast<int16_t>(num) == num) {
                        ret.add_command(opcode::PUSHSHORT, static_cast<int16_t>(num));
                    } else {
                        ret.add_command(opcode::PUSHINT, num);
                    }
                } else {
                    ret.add_command(opcode::PUSHDECIMAL, fixed_point(args[0]));
                }
                break;
            }
            case hash("PUSHSTR"):
            {
                std::string str;
                if (!(arg_str.front() == '"' ? parse_string : parse_string_regexp)(str, arg_str)) {
                    throw assembly_error(fmt::format("Stringa non valida: {0}", arg_str));
                }
                ret.add_command(opcode::PUSHSTR, str);
                break;
            }
            case hash("PUSHVAR"):       ret.add_command(opcode::PUSHVAR); break;
            case hash("MOVEVAR"):       ret.add_command(opcode::MOVEVAR); break;
            case hash("JMP"):           ret.add_command(opcode::JMP, getgotoindex(args[0])); break;
            case hash("JSR"):           ret.add_command(opcode::JMP, getgotoindex(args[0])); break;
            case hash("JZ"):            ret.add_command(opcode::JZ,  getgotoindex(args[0])); break;
            case hash("JNZ"):           ret.add_command(opcode::JNZ, getgotoindex(args[0])); break;
            case hash("JTE"):           ret.add_command(opcode::JTE, getgotoindex(args[0])); break;
            case hash("INC"):           ret.add_command(opcode::INC); break;
            case hash("DEC"):           ret.add_command(opcode::DEC); break;
            case hash("ISSET"):         ret.add_command(opcode::ISSET); break;
            case hash("GETSIZE"):       ret.add_command(opcode::GETSIZE); break;
            case hash("MOVCONTENT"):    ret.add_command(opcode::MOVCONTENT); break;
            case hash("SETBEGIN"):      ret.add_command(opcode::SETBEGIN); break;
            case hash("SETEND"):        ret.add_command(opcode::SETEND); break;
            case hash("NEWVIEW"):       ret.add_command(opcode::NEWVIEW); break;
            case hash("NEWTOKENS"):     ret.add_command(opcode::NEWTOKENS); break;
            case hash("RESETVIEW"):     ret.add_command(opcode::RESETVIEW); break;
            case hash("NEXTLINE"):      ret.add_command(opcode::NEXTLINE); break;
            case hash("NEXTTOKEN"):     ret.add_command(opcode::NEXTTOKEN); break;
            case hash("POPCONTENT"):    ret.add_command(opcode::POPCONTENT); break;
            case hash("NEXTTABLE"):     ret.add_command(opcode::NEXTTABLE); break;
            case hash("ATE"):           ret.add_command(opcode::ATE); break;
            case hash("RET"):           ret.add_command(opcode::RET); break;
            case hash("IMPORT"):        ret.add_command(opcode::IMPORT, arg_str); break;
            default:
                throw assembly_error(fmt::format("Comando sconosciuto: {0}", cmd));
            }
        }
    } catch (const std::invalid_argument &error) {
        throw assembly_error(error.what());
    }
    return ret;
}