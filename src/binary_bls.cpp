#include "binary_bls.h"

#include <fstream>

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

template<> void writeData<std::monostate>(std::ostream &, const std::monostate &) {}

template<typename T> T readData(std::istream &input) {
    return binary_io<T>::read(input);
}

template<> struct binary_io<std::string> {
    static void write(std::ostream &output, const std::string &str) {
        writeData<string_size>(output, str.size());
        output.write(str.c_str(), str.size());
    }

    static std::string read(std::istream &input) {
        auto len = readData<string_size>(input);
        std::string buffer(len, '\0');
        input.read(buffer.data(), len);
        return buffer;
    }
};

template<> struct binary_io<string_ptr> {
    static void write(std::ostream &output, const string_ptr &str) {
        writeData(output, *str);
    }

    static string_ptr read(std::istream &input) {
        return readData<std::string>(input);
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
        writeData(output, box.type);
        writeData(output, box.flags);
    }

    static readbox_options read(std::istream &input) {
        readbox_options options;
        options.mode = readData<read_mode>(input);
        options.type = readData<box_type>(input);
        options.flags = readData<flags_t>(input);
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

template<> struct binary_io<variable_selector> {
    static void write(std::ostream &output, const variable_selector &args) {
        writeData(output, args.name);
        writeData(output, args.index);
        writeData(output, args.length);
        writeData(output, args.flags);
    }

    static variable_selector read(std::istream &input) {
        variable_selector sel;
        sel.name = readData<string_ptr>(input);
        sel.index = readData<small_int>(input);
        sel.length = readData<small_int>(input);
        sel.flags = readData<uint8_t>(input);
        return sel;
    }
};

template<> struct binary_io<import_options> {
    static void write(std::ostream &output, const import_options &opts) {
        writeData(output, opts.filename);
        writeData(output, opts.flags);
    }

    static import_options read(std::istream &input) {
        import_options opts;
        opts.filename = readData<string_ptr>(input);
        opts.flags = readData<uint8_t>(input);
        return opts;
    }
};

template<> struct binary_io<jsr_address> {
    static void write(std::ostream &output, const jsr_address &addr) {
        writeData(output, addr.label);
        writeData(output, addr.numargs);
    }

    static jsr_address read(std::istream &input) {
        jsr_address addr;
        addr.label = readData<jump_label>(input);
        addr.numargs = readData<small_int>(input);
        return addr;
    }
};

template<typename T> struct binary_io<bitset<T>> {
    static void write(std::ostream &output, const bitset<T> &data) {
        writeData<flags_t>(output, data);
    }

    static bitset<T> read(std::istream &input) {
        return readData<flags_t>(input);
    }
};

template<> struct binary_io<command_args> {
    static void write(std::ostream &output, const command_args &cmd) {
        writeData(output, cmd.command());
        cmd.visit([&](auto && data) {
            writeData(output, data);
        });
    }

    static command_args read(std::istream &input) {
        static const auto make_command_lookup = []<size_t ... Is> (std::index_sequence<Is...>) {
            return std::array<std::function<command_args(std::istream &)>, EnumSize<opcode>> {
                [](std::istream &input) {
                    constexpr auto Cmd = static_cast<opcode>(Is);
                    if constexpr (std::is_void_v<EnumType<Cmd>>) {
                        return make_command<Cmd>();
                    } else {
                        return make_command<Cmd>(readData<EnumType<Cmd>>(input));
                    }
                } ...
            };
        } (std::make_index_sequence<EnumSize<opcode>>{});

        return make_command_lookup[static_cast<size_t>(readData<opcode>(input))](input);
    }
};

namespace binary_bls {
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