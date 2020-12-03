#include "bytecode.h"
#include "binary_io.h"

#include <algorithm>

std::ostream &bytecode::write_bytecode(std::ostream &output) {
    writeData(output, MAGIC);

    for (const auto &line : m_commands) {
        writeData<opcode>(output, line.command);
        switch (line.command) {
        case opcode::RDBOX:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<byte_int>(output, box.mode);
            writeData<byte_int>(output, box.page);
            writeData(output, box.x);
            writeData(output, box.y);
            writeData(output, box.w);
            writeData(output, box.h);
            break;
        }
        case opcode::RDPAGE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<byte_int>(output, box.mode);
            writeData<byte_int>(output, box.page);
            break;
        }
        case opcode::RDFILE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<byte_int>(output, box.mode);
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
            writeData(output, var_idx.index_first);
            break;
        }
        case opcode::SELRANGE:
        {
            const auto &var_idx = line.get<variable_idx>();
            writeData(output, var_idx.name);
            writeData(output, var_idx.index_first);
            writeData(output, var_idx.index_last);
            break;
        }
        case opcode::STRDATA:
            writeData(output, line.get<std::string>());
            break;
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELRANGETOP:
        case opcode::SELGLOBAL:
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
        case opcode::PUSHFLOAT:
            writeData(output, line.get<float>());
            break;
        case opcode::SETPAGE:
        case opcode::MVBOX:
            writeData(output, line.get<byte_int>());
            break;
        case opcode::INC:
        case opcode::DEC:
            writeData(output, line.get<small_int>());
            break;
        case opcode::JMP:
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            writeData(output, line.get<jump_address>());
            break;
        default:
            break;
        }
    }

    for (const auto &str : m_strings) {
        writeData<opcode>(output, opcode::STRDATA);
        writeData<string_size>(output, str.size());
        output.write(str.c_str(), str.size());
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
        opcode cmd = static_cast<opcode>(readData<opcode>(input));
        switch(cmd) {
        case opcode::RDBOX:
        {
            pdf_rect box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(readData<byte_int>(input));
            box.page = readData<byte_int>(input);
            box.x = readData<float>(input);
            box.y = readData<float>(input);
            box.w = readData<float>(input);
            box.h = readData<float>(input);
            m_commands.emplace_back(cmd, std::move(box));
            break;
        }
        case opcode::RDPAGE:
        {
            pdf_rect box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(readData<byte_int>(input));
            box.page = readData<byte_int>(input);
            m_commands.emplace_back(cmd, std::move(box));
            break;
        }
        case opcode::RDFILE:
        {
            pdf_rect box;
            box.type = BOX_FILE;
            box.mode = static_cast<read_mode>(readData<byte_int>(input));
            m_commands.emplace_back(cmd, std::move(box));
            break;
        }
        case opcode::CALL:
        {
            command_call call;
            call.name = readData<string_ref>(input);
            call.numargs = readData<byte_int>(input);
            m_commands.emplace_back(cmd, std::move(call));
            break;
        }
        case opcode::SELVAR:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index_first = readData<byte_int>(input);
            var_idx.index_last = var_idx.index_first;
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case opcode::SELRANGE:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index_first = readData<byte_int>(input);
            var_idx.index_last = readData<byte_int>(input);
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case opcode::STRDATA:
            m_strings.push_back(readData<std::string>(input));
            break;
        case opcode::PUSHSTR:
        case opcode::SELVARTOP:
        case opcode::SELRANGEALL:
        case opcode::SELGLOBAL:
        case opcode::SELRANGETOP:
            m_commands.emplace_back(cmd, readData<string_ref>(input));
            break;
        case opcode::PUSHBYTE:
            m_commands.emplace_back(cmd, readData<int8_t>(input));
            break;
        case opcode::PUSHSHORT:
            m_commands.emplace_back(cmd, readData<int16_t>(input));
            break;
        case opcode::PUSHINT:
            m_commands.emplace_back(cmd, readData<int32_t>(input));
            break;
        case opcode::PUSHFLOAT:
            m_commands.emplace_back(cmd, readData<float>(input));
            break;
        case opcode::SETPAGE:
        case opcode::MVBOX:
            m_commands.emplace_back(cmd, readData<byte_int>(input));
            break;
        case opcode::INC:
        case opcode::DEC:
            m_commands.emplace_back(cmd, readData<small_int>(input));
            break;
        case opcode::JMP:
        case opcode::JZ:
        case opcode::JNZ:
        case opcode::JTE:
            m_commands.emplace_back(cmd, readData<jump_address>(input));
            break;
        default:
            m_commands.emplace_back(cmd);
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