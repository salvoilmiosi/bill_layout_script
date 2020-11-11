#include "assembler.h"

#include <fmt/core.h>
#include <vector>
#include <utility>
#include <map>

#include "../shared/utils.h"
#include "../shared/layout.h"

assembler::assembler(const std::vector<std::string> &lines) {
    std::map<std::string, int> labels;

    for (size_t i=0; i<lines.size(); ++i) {
        if (lines[i].back() == ':') {
            labels[lines[i].substr(0, lines[i].size()-1)] = i - labels.size();
        }
    }

    auto getgotoindex = [&](const std::string &label) {
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
            auto box = std::make_shared<layout_box>();
            box->type = BOX_RECTANGLE;
            box->mode = static_cast<read_mode>(std::stoi(args[0]));
            box->page = std::stoi(args[1]);
            box->spacers = args[2];
            box->x = std::stof(args[3]);
            box->y = std::stof(args[4]);
            box->w = std::stof(args[5]);
            box->h = std::stof(args[6]);
            out_lines.emplace_back(RDBOX, box, sizeof(int) * 7);
            break;
        }
        case hash("RDPAGE"):
        {
            auto box = std::make_shared<layout_box>();
            box->type = BOX_PAGE;
            box->mode = static_cast<read_mode>(std::stoi(args[0]));
            box->page = std::stoi(args[1]);
            box->spacers = args[2];
            out_lines.emplace_back(RDPAGE, box, sizeof(int) * 3);
            break;
        }
        case hash("RDFILE"):
        {
            auto box = std::make_shared<layout_box>();
            box->type = BOX_WHOLE_FILE;
            box->mode = static_cast<read_mode>(std::stoi(args[0]));
            out_lines.emplace_back(RDFILE, box, sizeof(int));
            break;
        }
        case hash("CALL"):
        {
            auto call = std::make_shared<command_call>();
            call->name = args[0];
            call->numargs = std::stoi(args[1]);
            out_lines.emplace_back(CALL, call);
            break;
        }
        case hash("SPACER"):
        {
            auto spacer = std::make_shared<command_spacer>();
            spacer->name = args[0];
            spacer->w = std::stof(args[1]);
            spacer->h = std::stof(args[2]);
            out_lines.emplace_back(SPACER, spacer);
            break;
        }
        case hash("ERROR"):         out_lines.emplace_back(ERROR,           std::make_shared<std::string>(parse_string(arg_str))); break;
        case hash("PARSENUM"):      out_lines.emplace_back(PARSENUM); break;
        case hash("PARSEINT"):      out_lines.emplace_back(PARSEINT); break;
        case hash("EQ"):            out_lines.emplace_back(EQ); break;
        case hash("NEQ"):           out_lines.emplace_back(NEQ); break;
        case hash("AND"):           out_lines.emplace_back(AND); break;
        case hash("OR"):            out_lines.emplace_back(OR); break;
        case hash("NOT"):           out_lines.emplace_back(NOT); break;
        case hash("ADD"):           out_lines.emplace_back(ADD); break;
        case hash("SUB"):           out_lines.emplace_back(SUB); break;
        case hash("MUL"):           out_lines.emplace_back(MUL); break;
        case hash("DIV"):           out_lines.emplace_back(DIV); break;
        case hash("GT"):            out_lines.emplace_back(GT); break;
        case hash("LT"):            out_lines.emplace_back(LT); break;
        case hash("GEQ"):           out_lines.emplace_back(GEQ); break;
        case hash("LEQ"):           out_lines.emplace_back(LEQ); break;
        case hash("MAX"):           out_lines.emplace_back(MAX); break;
        case hash("MIN"):           out_lines.emplace_back(MIN); break;
        case hash("SETGLOBAL"):     out_lines.emplace_back(SETGLOBAL,       std::make_shared<std::string>(args[0])); break;
        case hash("SETDEBUG"):      out_lines.emplace_back(SETDEBUG); break;
        case hash("CLEAR"):         out_lines.emplace_back(CLEAR,           std::make_shared<std::string>(args[0])); break;
        case hash("APPEND"):        out_lines.emplace_back(APPEND,          std::make_shared<std::string>(args[0])); break;
        case hash("SETVAR"):        out_lines.emplace_back(SETVAR,          std::make_shared<std::string>(args[0])); break;
        case hash("RESETVAR"):      out_lines.emplace_back(RESETVAR,        std::make_shared<std::string>(args[0])); break;
        case hash("COPYCONTENT"):   out_lines.emplace_back(COPYCONTENT); break;
        case hash("PUSHNUM"):       out_lines.emplace_back(PUSHNUM,         std::make_shared<float>(std::stof(args[0]))); break;
        case hash("PUSHSTR"):       out_lines.emplace_back(PUSHSTR,         std::make_shared<std::string>(parse_string(arg_str))); break;
        case hash("PUSHGLOBAL"):    out_lines.emplace_back(PUSHGLOBAL,      std::make_shared<std::string>(args[0])); break;
        case hash("PUSHVAR"):       out_lines.emplace_back(PUSHVAR,         std::make_shared<std::string>(args[0])); break;
        case hash("SETINDEX"):      out_lines.emplace_back(SETINDEX,        std::make_shared<int>(std::stoi(args[0]))); break;
        case hash("SETINDEXTOP"):   out_lines.emplace_back(SETINDEXTOP); break;
        case hash("JMP"):           out_lines.emplace_back(JMP,             std::make_shared<int>(getgotoindex(args[0]))); break;
        case hash("JZ"):            out_lines.emplace_back(JZ,              std::make_shared<int>(getgotoindex(args[0]))); break;
        case hash("JTE"):           out_lines.emplace_back(JTE,             std::make_shared<int>(getgotoindex(args[0]))); break;
        case hash("INCTOP"):        out_lines.emplace_back(INCTOP,          std::make_shared<std::string>(args[0])); break;
        case hash("INC"):           out_lines.emplace_back(INC,             std::make_shared<std::string>(args[0])); break;
        case hash("INCGTOP"):       out_lines.emplace_back(INCGTOP,         std::make_shared<std::string>(args[0])); break;
        case hash("INCG"):          out_lines.emplace_back(INCG,            std::make_shared<std::string>(args[0])); break;
        case hash("DECTOP"):        out_lines.emplace_back(DECTOP,          std::make_shared<std::string>(args[0])); break;
        case hash("DEC"):           out_lines.emplace_back(DEC,             std::make_shared<std::string>(args[0])); break;
        case hash("DECGTOP"):       out_lines.emplace_back(DECGTOP,         std::make_shared<std::string>(args[0])); break;
        case hash("DECG"):          out_lines.emplace_back(DECG,            std::make_shared<std::string>(args[0])); break;
        case hash("ISSET"):         out_lines.emplace_back(ISSET,           std::make_shared<std::string>(args[0])); break;
        case hash("SIZE"):          out_lines.emplace_back(SIZE,            std::make_shared<std::string>(args[0])); break;
        case hash("PUSHCONTENT"):   out_lines.emplace_back(PUSHCONTENT); break;
        case hash("NEXTLINE"):      out_lines.emplace_back(NEXTLINE); break;
        case hash("NEXTTOKEN"):     out_lines.emplace_back(NEXTTOKEN); break;
        case hash("POPCONTENT"):    out_lines.emplace_back(POPCONTENT); break;
        case hash("HLT"):           out_lines.emplace_back(HLT); break;
        default:
            throw assembly_error{fmt::format("Comando sconosciuto: {0}", cmd)};
        }
    }
}

void assembler::save_output(std::ostream &output) {
    int out_size = 0;

    for (auto &line : out_lines) {
        out_size += sizeof(asm_command);
        out_size += line.datasize;
    }

    std::vector<std::pair<std::string, int>> out_strings;

    auto write_data = [&output](const auto &data) {
        output.write(reinterpret_cast<const char *>(&data), sizeof(data));
    };

    auto write_string = [&](const std::string &data) {
        if (data.empty()) {
            write_data(0);
        } else {
            auto it = std::find_if(out_strings.begin(), out_strings.end(), [&data](auto &obj){ return obj.first == data; });
            if (it == out_strings.end()) {
                write_data(out_size);
                out_strings.emplace_back(data, out_size);
                out_size += data.size() + sizeof(int) + sizeof(short);
            } else {
                write_data(it->second);
            }
        }
    };

    for (const auto &line : out_lines) {
        write_data(line.command);
        switch (line.command) {
        case RDBOX:
        {
            auto box = std::static_pointer_cast<layout_box>(line.data);
            write_data(box->mode);
            write_data(box->page);
            write_string(box->spacers);
            write_data(box->x);
            write_data(box->y);
            write_data(box->w);
            write_data(box->h);
            break;
        }
        case RDPAGE:
        {
            auto box = std::static_pointer_cast<layout_box>(line.data);
            write_data(box->mode);
            write_data(box->page);
            write_string(box->spacers);
            break;
        }
        case RDFILE:
        {
            auto box = std::static_pointer_cast<layout_box>(line.data);
            write_data(box->mode);
            break;
        }
        case CALL:
        {
            auto call = std::static_pointer_cast<command_call>(line.data);
            write_string(call->name);
            write_data(call->numargs);
            break;
        }
        case SPACER:
        {
            auto spacer = std::static_pointer_cast<command_spacer>(line.data);
            write_string(spacer->name);
            write_data(spacer->w);
            write_data(spacer->h);
            break;
        }
        case ERROR:
        case SETGLOBAL:
        case CLEAR:
        case APPEND:
        case SETVAR:
        case RESETVAR:
        case PUSHSTR:
        case PUSHGLOBAL:
        case PUSHVAR:
        case ISSET:
        case SIZE:
        case INC:
        case INCTOP:
        case INCG:
        case INCGTOP:
        case DEC:
        case DECTOP:
        case DECG:
        case DECGTOP:
            write_string(*std::static_pointer_cast<std::string>(line.data));
            break;
        case PUSHNUM:
            write_data(*std::static_pointer_cast<float>(line.data));
            break;
        case JMP:
        case JZ:
        case JTE:
        case SETINDEX:
            write_data(*std::static_pointer_cast<int>(line.data));
            break;
        default:
            break;
        }
    }

    for (auto &str : out_strings) {
        write_data(STRDATA);
        write_data((uint16_t) (str.first.size()));
        output.write(str.first.c_str(), str.first.size());
    }
}