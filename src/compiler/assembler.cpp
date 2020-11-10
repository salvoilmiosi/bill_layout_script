#include "assembler.h"

#include <fmt/core.h>
#include <map>

#include "../shared/utils.h"

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
            out_lines.emplace_back(RDBOX,
                std::stof(args[0]), std::stof(args[1]),
                std::stof(args[2]), std::stof(args[3]),
                std::stoi(args[4]),
                std::stoi(args[5]), std::stoi(args[6]),
                args[7]);
            break;
        case hash("CALL"):          out_lines.emplace_back(CALL, args[0], std::stoi(args[1])); break;
        case hash("ERROR"):         out_lines.emplace_back(ERROR); break;
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
        case hash("SETGLOBAL"):     out_lines.emplace_back(SETGLOBAL, arg_str); break;
        case hash("SETDEBUG"):      out_lines.emplace_back(SETDEBUG); break;
        case hash("CLEAR"):         out_lines.emplace_back(CLEAR, arg_str); break;
        case hash("APPEND"):        out_lines.emplace_back(APPEND, arg_str); break;
        case hash("SETVAR"):        out_lines.emplace_back(SETVAR, arg_str); break;
        case hash("PUSHCONTENT"):   out_lines.emplace_back(PUSHCONTENT); break;
        case hash("PUSHNUM"):       out_lines.emplace_back(PUSHNUM, std::stof(arg_str)); break;
        case hash("PUSHSTR"):       out_lines.emplace_back(PUSHSTR, parse_string(arg_str)); break;
        case hash("PUSHGLOBAL"):    out_lines.emplace_back(PUSHGLOBAL, arg_str); break;
        case hash("PUSHVAR"):       out_lines.emplace_back(PUSHVAR, arg_str); break;
        case hash("SETINDEX"):      out_lines.emplace_back(SETINDEX); break;
        case hash("JMP"):           out_lines.emplace_back(JMP, getgotoindex(arg_str)); break;
        case hash("JZ"):            out_lines.emplace_back(JZ, getgotoindex(arg_str)); break;
        case hash("JTE"):           out_lines.emplace_back(JTE, getgotoindex(arg_str)); break;
        case hash("INCTOP"):        out_lines.emplace_back(INCTOP, arg_str); break;
        case hash("INC"):           out_lines.emplace_back(INC, arg_str); break;
        case hash("INCGTOP"):       out_lines.emplace_back(INCGTOP, arg_str); break;
        case hash("INCG"):          out_lines.emplace_back(INCG, arg_str); break;
        case hash("DECTOP"):        out_lines.emplace_back(DECTOP, arg_str); break;
        case hash("DEC"):           out_lines.emplace_back(DEC, arg_str); break;
        case hash("DECGTOP"):       out_lines.emplace_back(DECGTOP, arg_str); break;
        case hash("DECG"):          out_lines.emplace_back(DECG, arg_str); break;
        case hash("ISSET"):         out_lines.emplace_back(ISSET, arg_str); break;
        case hash("SIZE"):          out_lines.emplace_back(SIZE, arg_str); break;
        case hash("CONTENTVIEW"):   out_lines.emplace_back(CONTENTVIEW); break;
        case hash("NEXTLINE"):      out_lines.emplace_back(NEXTLINE); break;
        case hash("NEXTTOKEN"):     out_lines.emplace_back(NEXTTOKEN); break;
        case hash("POPCONTENT"):    out_lines.emplace_back(POPCONTENT); break;
        case hash("SPACER"):        out_lines.emplace_back(SPACER, args[0], std::stof(args[1]), std::stof(args[2])); break;
        default:
            throw assembly_error{fmt::format("Comando sconosciuto: {0}", cmd)};
        }
    }
}

void assembler::save_output(std::ostream &output) {
    for (auto &line : out_lines) {
        output.write(reinterpret_cast<const char *>(&line.command), sizeof(line.command));
        if (line.args_size > 0) {
            output.write(line.args_data, line.args_size);
        }
    }
}