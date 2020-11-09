#include "assembler.h"

#include "../shared/utils.h"

#include <fmt/core.h>
#include <map>

template<typename T> constexpr size_t get_size(const T &) {
    return sizeof(T);
}

template<>
size_t get_size<> (const std::string &str) {
    return sizeof(short) + str.size();
}

constexpr size_t get_size_all() {
    return 0;
}

template<typename T, typename ... Ts>
constexpr size_t get_size_all(const T &first, const Ts & ... others) {
    return get_size(first) + get_size_all(others ...);
}

template<typename T>
size_t add_arg(char *out, const T &data) {
    memcpy(out, &data, sizeof(data));
    return sizeof(data);
}

template<> size_t add_arg(char *out, const std::string &data) {
    size_t bytes = add_arg(out, (short) data.size());
    memcpy(out + bytes, data.data(), data.size());
    return get_size(data);
}

constexpr void add_args(char *) {}

template<typename T, typename ... Ts>
void add_args(char *out, const T &first, const Ts & ... others) {
    size_t bytes = add_arg(out, first);
    add_args(out + bytes, others ...);
}

template<typename ... Ts>
asm_line create_cmd(asm_command cmd, const Ts &... args) {
    asm_line ret(cmd);
    ret.arg_num = sizeof...(args);
    if (ret.arg_num > 0) {
        ret.args_size = get_size_all(args ...);
        ret.args_data = new char[ret.args_size];
        add_args(ret.args_data, args ...);
    }
    return ret;
}

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
        std::string args = line.substr(space_pos + 1);
        switch (hash(cmd)) {
        case hash("RDBOX"):
        {
            size_t comma = args.find_first_of(',');
            float x = std::stof(args.substr(0, comma));
            size_t start = comma + 1;
            comma = args.find_first_of(',', start);
            float y = std::stof(args.substr(start, comma - start));
            start = comma + 1;
            comma = args.find_first_of(',', start);
            float w = std::stof(args.substr(start, comma - start));
            start = comma + 1;
            comma = args.find_first_of(',', start);
            float h = std::stof(args.substr(start, comma - start));
            start = comma + 1;
            comma = args.find_first_of(',', start);
            int page = std::stoi(args.substr(start, comma - start));
            start = comma + 1;
            comma = args.find_first_of(',', start);
            int mode = std::stoi(args.substr(start, comma - start));
            start = comma + 1;
            comma = args.find_first_of(',', start);
            int type = std::stoi(args.substr(start, comma - start));
            start = comma + 1;
            std::string spacers = args.substr(start);
            out_lines.push_back(create_cmd(RDBOX, x, y, w, h, page, mode, type, spacers));
            break;
        }
        case hash("CALL"):
        {
            size_t comma = args.find_first_of(',');
            std::string name = args.substr(0, comma);
            int numargs = std::stoi(args.substr(comma + 1));
            out_lines.push_back(create_cmd(CALL, args, numargs));
            break;
        }
        case hash("SPACER"):
        {
            size_t comma = args.find_first_of(',');
            std::string name = args.substr(0, comma);
            size_t start = comma + 1;
            comma = args.find_first_of(',', start);
            float w = std::stof(args.substr(start, comma - start));
            start = comma + 1;
            float h = std::stof(args.substr(start));
            out_lines.push_back(create_cmd(SPACER, name, w, h));
            break;
        }
        case hash("NOP"):           out_lines.emplace_back(NOP); break;
        case hash("SETGLOBAL"):     out_lines.push_back(create_cmd(SETGLOBAL, args)); break;
        case hash("SETDEBUG"):      out_lines.emplace_back(SETDEBUG); break;
        case hash("CLEAR"):         out_lines.push_back(create_cmd(CLEAR, args)); break;
        case hash("APPEND"):        out_lines.push_back(create_cmd(APPEND, args)); break;
        case hash("SETVAR"):        out_lines.push_back(create_cmd(SETVAR, args)); break;
        case hash("PUSHCONTENT"):   out_lines.emplace_back(PUSHCONTENT); break;
        case hash("PUSHNUM"):       out_lines.push_back(create_cmd(PUSHNUM, std::stof(args))); break;
        case hash("PUSHSTR"):       out_lines.push_back(create_cmd(PUSHSTR, parse_string(args))); break;
        case hash("PUSHGLOBAL"):    out_lines.push_back(create_cmd(PUSHGLOBAL, args)); break;
        case hash("PUSHVAR"):       out_lines.push_back(create_cmd(PUSHVAR, args)); break;
        case hash("SETINDEX"):      out_lines.emplace_back(SETINDEX); break;
        case hash("JMP"):           out_lines.push_back(create_cmd(JMP, getgotoindex(args))); break;
        case hash("JZ"):            out_lines.push_back(create_cmd(JZ, getgotoindex(args))); break;
        case hash("JTE"):           out_lines.push_back(create_cmd(JTE, getgotoindex(args))); break;
        case hash("INCTOP"):        out_lines.push_back(create_cmd(INCTOP, args)); break;
        case hash("INC"):           out_lines.push_back(create_cmd(INC, args)); break;
        case hash("DECTOP"):        out_lines.push_back(create_cmd(DECTOP, args)); break;
        case hash("DEC"):           out_lines.push_back(create_cmd(DEC, args)); break;
        case hash("ISSET"):         out_lines.push_back(create_cmd(ISSET, args)); break;
        case hash("SIZE"):          out_lines.push_back(create_cmd(SIZE, args)); break;
        case hash("NEWCONTENT"):    out_lines.emplace_back(NEWCONTENT); break;
        case hash("NEXTLINE"):      out_lines.emplace_back(NEXTLINE); break;
        case hash("NEXTTOKEN"):     out_lines.emplace_back(NEXTTOKEN); break;
        case hash("POPCONTENT"):    out_lines.emplace_back(POPCONTENT); break;
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