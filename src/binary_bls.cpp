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

template<> struct binary_io<jump_uneval> {
    static void write(std::ostream &output, const jump_uneval &args) {
        assert("Unevaluated jump" == 0);
    }

    static jump_uneval read(std::istream &input) {
        assert("Unevaluated jump" == 0);
        return jump_uneval{};
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

template<typename T> struct binary_io<bitset<T>> {
    static void write(std::ostream &output, const bitset<T> &data) {
        writeData<flags_t>(output, data);
    }

    static bitset<T> read(std::istream &input) {
        return readData<flags_t>(input);
    }
};

template<opcode Cmd> inline void writeCommandData(std::ostream &output, const command_args &line) {
    using type = opcode_type<Cmd>;
    writeData(output, Cmd);
    if constexpr (! std::is_void_v<type>) {
        writeData<type>(output, line.get_args<Cmd>());
    }
}

template<opcode Cmd> inline command_args createCommandArgs(std::istream &input) {
    using type = opcode_type<Cmd>;
    if constexpr (std::is_void_v<type>) {
        return command_args(Cmd);
    } else {
        return command_args(Cmd, readData<type>(input));
    }
}

namespace binary_bls {
    bytecode read(const std::filesystem::path &filename) {
        std::ifstream ifs(filename, std::ios::binary | std::ios::in);

        bytecode ret;

        while (ifs.peek() != EOF) {
#define O_IMPL(x, t) case opcode::x: ret.push_back(createCommandArgs<opcode::x>(ifs)); break;
            switch (readData<opcode>(ifs)) {
                OPCODES
            }
#undef O_IMPL
        }

        return ret;
    }

    void write(const bytecode &code, const std::filesystem::path &filename) {
        std::ofstream ofs(filename, std::ios::binary | std::ios::out);

        for (const auto &line : code) {
#define O_IMPL(x, t) case opcode::x: writeCommandData<opcode::x>(ofs, line); break;
            switch (line.command()) {
                OPCODES
            }
#undef O_IMPL
        }
    }
}