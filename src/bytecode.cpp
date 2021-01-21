#include "bytecode.h"
#include "binary_io.h"
#include "fixed_point.h"

#include <algorithm>
#include <fstream>

std::ostream &operator << (std::ostream &output, const bytecode &code) {
    writeData(output, MAGIC);

    for (const auto &line : code) {
        writeData<opcode>(output, line.command());
        switch (line.command()) {
        case opcode::RDBOX:
        {
            const auto &box = line.get<pdf_rect>();
            writeData(output, box.mode);
            writeData(output, box.page);
            writeData(output, box.x);
            writeData(output, box.y);
            writeData(output, box.w);
            writeData(output, box.h);
            break;
        }
        case opcode::RDPAGE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData(output, box.mode);
            writeData(output, box.page);
            break;
        }
        case opcode::RDFILE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData(output, box.mode);
            break;
        }
        case opcode::CALL:
        {
            const auto &call = line.get<command_call>();
            writeData(output, call.name);
            writeData(output, call.numargs);
            break;
        }
        case opcode::SELVAR:
        {
            const auto &var_idx = line.get<variable_idx>();
            writeData(output, var_idx.name);
            writeData(output, var_idx.index);
            break;
        }
        case opcode::SELRANGE:
        {
            const auto &var_idx = line.get<variable_idx>();
            writeData(output, var_idx.name);
            writeData(output, var_idx.index);
            writeData(output, var_idx.range_len);
            break;
        }
        case opcode::IMPORT:
        case opcode::COMMENT:
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELRANGETOP:
            writeData(output, line.get<std::string>());
            break;
        case opcode::PUSHBYTE:
            writeData(output, line.get<int8_t>());
            break;
        case opcode::PUSHSHORT:
            writeData(output, line.get<int16_t>());
            break;
        case opcode::PUSHINT:
            writeData(output, line.get<int32_t>());
            break;
        case opcode::PUSHDECIMAL:
            writeData(output, line.get<fixed_point>());
            break;
        case opcode::SETPAGE:
            writeData(output, line.get<small_int>());
            break;
        case opcode::MVBOX:
            writeData(output, line.get<spacer_index>());
            break;
        case opcode::JMP:
        case opcode::JSR:
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            writeData(output, line.get<jump_address>());
            break;
        default:
            break;
        }
    }

    return output;
}

std::istream &operator >> (std::istream &input, bytecode &code) {
    auto check = readData<int32_t>(input);
    if (check != MAGIC) {
        input.setstate(std::ios::failbit);
        return input;
    }
    code.clear();

    while (input.peek() != EOF) {
        opcode cmd = readData<opcode>(input);
        switch(cmd) {
        case opcode::RDBOX:
        {
            pdf_rect box;
            box.type = box_type::BOX_RECTANGLE;
            readData(input, box.mode);
            readData(input, box.page);
            readData(input, box.x);
            readData(input, box.y);
            readData(input, box.w);
            readData(input, box.h);
            code.add_command(cmd, std::move(box));
            break;
        }
        case opcode::RDPAGE:
        {
            pdf_rect box;
            box.type = box_type::BOX_PAGE;
            readData(input, box.mode);
            readData(input, box.page);
            code.add_command(cmd, std::move(box));
            break;
        }
        case opcode::RDFILE:
        {
            pdf_rect box;
            box.type = box_type::BOX_FILE;
            readData(input, box.mode);
            code.add_command(cmd, std::move(box));
            break;
        }
        case opcode::CALL:
        {
            command_call call;
            readData(input, call.name);
            readData(input, call.numargs);
            code.add_command(cmd, std::move(call));
            break;
        }
        case opcode::SELVAR:
        {
            variable_idx var_idx;
            readData(input, var_idx.name);
            readData(input, var_idx.index);
            var_idx.range_len = 1;
            code.add_command(cmd, std::move(var_idx));
            break;
        }
        case opcode::SELRANGE:
        {
            variable_idx var_idx;
            readData(input, var_idx.name);
            readData(input, var_idx.index);
            readData(input, var_idx.range_len);
            code.add_command(cmd, std::move(var_idx));
            break;
        }
        case opcode::IMPORT:
        case opcode::COMMENT:
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELRANGETOP:
            code.add_command(cmd, readData<std::string>(input));
            break;
        case opcode::PUSHBYTE:
            code.add_command(cmd, readData<int8_t>(input));
            break;
        case opcode::PUSHSHORT:
            code.add_command(cmd, readData<int16_t>(input));
            break;
        case opcode::PUSHINT:
            code.add_command(cmd, readData<int32_t>(input));
            break;
        case opcode::PUSHDECIMAL:
            code.add_command(cmd, readData<fixed_point>(input));
            break;
        case opcode::SETPAGE:
            code.add_command(cmd, readData<small_int>(input));
            break;
        case opcode::MVBOX:
            code.add_command(cmd, readData<spacer_index>(input));
            break;
        case opcode::JMP:
        case opcode::JSR:
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            code.add_command(cmd, readData<jump_address>(input));
            break;
        default:
            code.add_command(cmd);
        }
    }
    return input;
}

bytecode bytecode::from_file(const std::filesystem::path &filename) {
    bytecode ret;
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    ifs >> ret;
    ret.filename = filename;
    return ret;
}