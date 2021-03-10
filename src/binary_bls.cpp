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
void writeData(std::ostream &output, const T &data) {
    if (is_big_endian()) {
        output.write(reinterpret_cast<const char *>(&data), sizeof(data));
    } else {
        T swapped = swap_endian(data);
        output.write(reinterpret_cast<const char *>(&swapped), sizeof(data));
    }
}

template<typename T>
T readData(std::istream &input) {
    T buffer;
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
    if (!is_big_endian()) {
        return swap_endian(buffer);
    }
    return buffer;
}

template<> inline void writeData<std::string>(std::ostream &output, const std::string &str) {
    writeData<string_size>(output, str.size());
    output.write(str.c_str(), str.size());
}

template<> inline std::string readData<std::string>(std::istream &input) {
    auto len = readData<string_size>(input);
    std::string buffer(len, '\0');
    input.read(buffer.data(), len);
    return buffer;
}

template<> inline void writeData<fixed_point>(std::ostream &output, const fixed_point &num) {
    writeData<dec::int64>(output, num.getUnbiased());
}

template<> inline fixed_point readData<fixed_point>(std::istream &input) {
    fixed_point ret;
    ret.setUnbiased(readData<dec::int64>(input));
    return ret;
}

template<> inline void writeData<pdf_rect>(std::ostream &output, const pdf_rect &box) {
    writeData<uint8_t>(output, box.page);
    writeData(output, box.x);
    writeData(output, box.y);
    writeData(output, box.w);
    writeData(output, box.h);
    writeData(output, box.mode);
    writeData(output, box.type);
}

template<> inline pdf_rect readData<pdf_rect>(std::istream &input) {
    pdf_rect box;
    box.page = readData<uint8_t>(input);
    box.x = readData<float>(input);
    box.y = readData<float>(input);
    box.w = readData<float>(input);
    box.h = readData<float>(input);
    box.mode = readData<read_mode>(input);
    box.type = readData<box_type>(input);
    return box;
}

template<> inline void writeData<command_call>(std::ostream &output, const command_call &args) {
    writeData(output, args.name);
    writeData(output, args.numargs);
}

template<> inline command_call readData<command_call>(std::istream &input) {
    command_call args;
    args.name = readData<std::string>(input);
    args.numargs = readData<small_int>(input);
    return args;
}

template<> inline void writeData<variable_selector>(std::ostream &output, const variable_selector &args) {
    writeData(output, args.name);
    writeData(output, args.index);
    writeData(output, args.length);
    writeData(output, args.flags);
}

template<> inline variable_selector readData<variable_selector>(std::istream &input) {
    variable_selector sel;
    sel.name = readData<std::string>(input);
    sel.index = readData<small_int>(input);
    sel.length = readData<small_int>(input);
    sel.flags = readData<uint8_t>(input);
    return sel;
}

template<> inline void writeData<jump_uneval>(std::ostream &output, const jump_uneval &args) {
    writeData(output, args.cmd);
    writeData(output, args.label);
}

template<> inline jump_uneval readData<jump_uneval>(std::istream &input) {
    jump_uneval jmp;
    jmp.cmd = readData<opcode>(input);
    jmp.label = readData<std::string>(input);
    return jmp;
}

template<> inline void writeData<import_options>(std::ostream &output, const import_options &opts) {
    writeData(output, opts.filename.string());
    writeData(output, opts.flags);
}

template<> inline import_options readData<import_options>(std::istream &input) {
    import_options opts;
    opts.filename = readData<std::string>(input);
    opts.flags = readData<uint8_t>(input);
    return opts;
}

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
#define O_IMPL(x, t) case OP_##x: ret.push_back(createCommandArgs<OP_##x>(ifs)); break;
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
#define O_IMPL(x, t) case OP_##x: writeCommandData<OP_##x>(ofs, line); break;
            switch (line.command()) {
                OPCODES
            }
#undef O_IMPL
        }
    }
}