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

template<> struct binary_io<pdf_rect> {
    static void write(std::ostream &output, const pdf_rect &box) {
        writeData<uint8_t>(output, box.page);
        writeData(output, box.x);
        writeData(output, box.y);
        writeData(output, box.w);
        writeData(output, box.h);
        writeData(output, box.mode);
        writeData(output, box.type);
        writeData(output, box.flags);
    }

    static pdf_rect read(std::istream &input) {
        pdf_rect box;
        box.page = readData<uint8_t>(input);
        box.x = readData<double>(input);
        box.y = readData<double>(input);
        box.w = readData<double>(input);
        box.h = readData<double>(input);
        box.mode = readData<read_mode>(input);
        box.type = readData<box_type>(input);
        box.flags = readData<flags_t>(input);
        return box;
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
        sel.name = readData<std::string>(input);
        sel.index = readData<small_int>(input);
        sel.length = readData<small_int>(input);
        sel.flags = readData<uint8_t>(input);
        return sel;
    }
};

template<> struct binary_io<import_options> {
    static void write(std::ostream &output, const import_options &opts) {
        writeData(output, opts.filename.string());
        writeData(output, opts.flags);
    }

    static import_options read(std::istream &input) {
        import_options opts;
        opts.filename = readData<std::string>(input);
        opts.flags = readData<uint8_t>(input);
        return opts;
    }
};

template<> struct binary_io<jump_address> {
    static void write(std::ostream &output, const jump_address &addr) {
        writeData(output, addr.relative_addr);
    }

    static jump_address read(std::istream &input) {
        jump_address addr;
        addr.relative_addr = readData<int16_t>(input);
        return addr;
    }
};

template<> struct binary_io<jsr_address> {
    static void write(std::ostream &output, const jsr_address &addr) {
        writeData<jump_address>(output, addr);
        writeData(output, addr.numargs);
    }

    static jsr_address read(std::istream &input) {
        jsr_address addr;
        addr.relative_addr = readData<int16_t>(input);
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

template<opcode Cmd> static void addCommand(bytecode &ret, opcode check, std::istream &input) {
    if (Cmd == check) {
        if constexpr (std::is_void_v<EnumType<Cmd>>) {
            ret.push_back(make_command<Cmd>());
        } else {
            ret.push_back(make_command<Cmd>(readData<EnumType<Cmd>>(input)));
        }
    }
}

namespace binary_bls {
    bytecode read(const std::filesystem::path &filename) {
        std::ifstream ifs(filename, std::ios::binary | std::ios::in);

        bytecode ret;

        while (ifs.peek() != EOF) {
            auto cmd = readData<opcode>(ifs);
            [&] <size_t ... Is> (std::index_sequence<Is...>) {
                (addCommand<static_cast<opcode>(Is)>(ret, cmd, ifs), ...);
            } (std::make_index_sequence<EnumSize<opcode>>{});
        }

        return ret;
    }

    void write(const bytecode &code, const std::filesystem::path &filename) {
        std::ofstream ofs(filename, std::ios::binary | std::ios::out);

        for (const auto &line : code) {
            writeData(ofs, line.command());
            line.visit([&](auto && args) {
                writeData(ofs, args);
            });
        }
    }
}