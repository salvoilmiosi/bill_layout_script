#include "binary_bls.h"

#include <fstream>

using namespace bls;

typedef uint16_t string_size;

inline bool is_big_endian() {
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

template <typename T>
T swap_endian(T u) {
    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

template<typename T>
struct binary_io {
    static void write(std::ostream &output, const T &data) {
        if (is_big_endian()) {
            output.write(reinterpret_cast<const char *>(&data), sizeof(data));
        } else {
            T swapped = swap_endian(data);
            output.write(reinterpret_cast<const char *>(&swapped), sizeof(data));
        }
    }

    static T read(std::istream &input) {
        T buffer;
        input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
        if (!is_big_endian()) {
            return swap_endian(buffer);
        }
        return buffer;
    }
};

template<typename T> void writeData(std::ostream &output, const T &data) {
    binary_io<T>::write(output, data);
}

template<typename T> T readData(std::istream &input) {
    return binary_io<T>::read(input);
}

template<typename T> void readData(std::istream &input, T &out) {
    out = binary_io<T>::read(input);
}

template<> struct binary_io<std::string> {
    static void write(std::ostream &output, const std::string &str) {
        writeData<string_size>(output, str.size());
        output.write(str.data(), str.size());
    }

    static std::string read(std::istream &input) {
        auto len = readData<string_size>(input);
        std::string buffer(len, '\0');
        input.read(buffer.data(), len);
        return buffer;
    }
};

template<> struct binary_io<comment_line> {
    static void write(std::ostream &output, const comment_line &line) {
        writeData(output, line.comment);
        writeData(output, line.line);
    }

    static comment_line read(std::istream &input) {
        comment_line ret;
        readData(input, ret.comment);
        readData(input, ret.line);
        return ret;
    }
};

template<> struct binary_io<fixed_point> {
    static void write(std::ostream &output, const fixed_point &num) {
        writeData<dec::int64>(output, num.getUnbiased());
    }

    static fixed_point read(std::istream &input) {
        fixed_point ret;
        ret.setUnbiased(readData<dec::int64>(input));
        return ret;
    }
};

template<> struct binary_io<readbox_options> {
    static void write(std::ostream &output, const readbox_options &box) {
        writeData(output, box.mode);
        writeData(output, box.flags);
    }

    static readbox_options read(std::istream &input) {
        readbox_options options;
        readData(input, options.mode);
        readData(input, options.flags);
        return options;
    }
};

template<> struct binary_io<command_call> {
    static void write(std::ostream &output, const command_call &args) {
        writeData(output, args.fun->first);
        writeData(output, args.numargs);
    }

    static command_call read(std::istream &input) {
        auto name = readData<std::string>(input);
        auto numargs = readData<small_int>(input);
        return command_call(name, numargs);
    }
};

template<> struct binary_io<jump_label> {
    static void write(std::ostream &output, const jump_label &addr) {}

    static jump_label read(std::istream &input) {
        return {};
    }
};

template<> struct binary_io<jump_address> {
    static void write(std::ostream &output, const jump_address &addr) {
        writeData(output, addr.address);
    }

    static jump_address read(std::istream &input) {
        jump_address ret;
        readData(input, ret.address);
        return ret;
    }
};

template<enums::flags_enum T> struct binary_io<enums::bitset<T>> {
    static void write(std::ostream &output, const enums::bitset<T> &data) {
        writeData(output, data.data());
    }

    static enums::bitset<T> read(std::istream &input) {
        return readData<std::underlying_type_t<T>>(input);
    }
};

template<> struct binary_io<command_args> {
    static void write(std::ostream &output, const command_args &cmd) {
        writeData(output, cmd.command());
        visit_command(util::overloaded{
            []<opcode Cmd>(command_tag<Cmd>) {},
            [&]<opcode Cmd>(command_tag<Cmd>, const auto &data) {
                writeData(output, data);
            }
        }, cmd);
    }

    template<opcode Cmd> static constexpr command_args read_command(std::istream &input) {
        if constexpr (std::is_void_v<enums::get_type_t<Cmd>>) {
            return make_command<Cmd>();
        } else {
            return make_command<Cmd>(readData<enums::get_type_t<Cmd>>(input));
        }
    }

    static command_args read(std::istream &input) {
        static constexpr auto command_lookup = []<size_t ... Is> (std::index_sequence<Is...>) {
            return std::array { read_command<static_cast<opcode>(Is)> ... };
        } (std::make_index_sequence<enums::size<opcode>()>{});

        opcode command = readData<opcode>(input);
        return command_lookup[static_cast<size_t>(command)](input);
    }
};

namespace bls::binary {
    bytecode read(const std::filesystem::path &filename) {
        std::ifstream ifs(filename, std::ios::binary | std::ios::in);

        bytecode ret;
        while (ifs.peek() != EOF) {
            ret.push_back(readData<command_args>(ifs));
        }
        return ret;
    }

    void write(const bytecode &code, const std::filesystem::path &filename) {
        std::ofstream ofs(filename, std::ios::binary | std::ios::out);

        for (const auto &line : code) {
            writeData(ofs, line);
        }
    }
}