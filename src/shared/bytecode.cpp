#include "bytecode.h"
#include "binary_io.h"
#include "decimal.h"

#include <algorithm>

std::ostream &bytecode::write_bytecode(std::ostream &output) {
    writeData(output, MAGIC);

    for (const auto &line : m_commands) {
        writeData<opcode>(output, line.command);
        switch (line.command) {
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
        case opcode::STRDATA:
            writeData(output, line.get<std::string>());
            break;
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELRANGETOP:
            writeData(output, line.get<string_ref>());
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
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            writeData(output, line.get<jump_address>());
            break;
        case opcode::COMMENT:
            writeData<std::string>(output, line.get<std::string>());
            break;
        default:
            break;
        }
    }

    for (const auto &str : m_strings) {
        writeData<opcode>(output, opcode::STRDATA);
        writeData<std::string>(output, str);
    }
    return output;
}

std::istream &bytecode::read_bytecode(std::istream &input) {
    auto check = readData<int32_t>(input);
    if (check != MAGIC) {
        input.setstate(std::ios::failbit);
        return input;
    }
    m_commands.clear();
    m_strings.clear();

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
            add_command(cmd, std::move(box));
            break;
        }
        case opcode::RDPAGE:
        {
            pdf_rect box;
            box.type = box_type::BOX_PAGE;
            readData(input, box.mode);
            readData(input, box.page);
            add_command(cmd, std::move(box));
            break;
        }
        case opcode::RDFILE:
        {
            pdf_rect box;
            box.type = box_type::BOX_FILE;
            readData(input, box.mode);
            add_command(cmd, std::move(box));
            break;
        }
        case opcode::CALL:
        {
            command_call call;
            readData(input, call.name);
            readData(input, call.numargs);
            add_command(cmd, std::move(call));
            break;
        }
        case opcode::SELVAR:
        {
            variable_idx var_idx;
            readData(input, var_idx.name);
            readData(input, var_idx.index);
            var_idx.range_len = 1;
            add_command(cmd, std::move(var_idx));
            break;
        }
        case opcode::SELRANGE:
        {
            variable_idx var_idx;
            readData(input, var_idx.name);
            readData(input, var_idx.index);
            readData(input, var_idx.range_len);
            add_command(cmd, std::move(var_idx));
            break;
        }
        case opcode::COMMENT:
            add_command(cmd, readData<std::string>(input));
            break;
        case opcode::STRDATA:
            m_strings.push_back(readData<std::string>(input));
            break;
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELRANGETOP:
            add_command(cmd, readData<string_ref>(input));
            break;
        case opcode::PUSHBYTE:
            add_command(cmd, readData<int8_t>(input));
            break;
        case opcode::PUSHSHORT:
            add_command(cmd, readData<int16_t>(input));
            break;
        case opcode::PUSHINT:
            add_command(cmd, readData<int32_t>(input));
            break;
        case opcode::PUSHDECIMAL:
            add_command(cmd, readData<fixed_point>(input));
            break;
        case opcode::SETPAGE:
            add_command(cmd, readData<small_int>(input));
            break;
        case opcode::MVBOX:
            add_command(cmd, readData<spacer_index>(input));
            break;
        case opcode::JMP:
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            add_command(cmd, readData<jump_address>(input));
            break;
        default:
            add_command(cmd);
        }
    }
    return input;
}

string_ref bytecode::add_string(const std::string &str) {
    if (str.empty()) {
        return 0xffff;
    }
    auto it = std::find(m_strings.begin(), m_strings.end(), str);
    if (it != m_strings.end()) {
        return it - m_strings.begin();
    } else {
        m_strings.push_back(str);
        return m_strings.size() - 1;
    }
}

const std::string &bytecode::get_string(string_ref ref) {
    static const std::string STR_EMPTY;
    if (ref == 0xffff) {
        return STR_EMPTY;
    } else {
        return m_strings[ref];
    }
}