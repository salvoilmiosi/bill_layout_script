#include "bytecode.h"
#include "binary_io.h"

#include <algorithm>

std::ostream &bytecode::write_bytecode(std::ostream &output) {
    writeData(output, MAGIC);

    for (const auto &line : m_commands) {
        writeData<command_int>(output, line.command);
        switch (line.command) {
        case RDBOX:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<small_int>(output, box.mode);
            writeData<small_int>(output, box.page);
            writeData(output, box.x);
            writeData(output, box.y);
            writeData(output, box.w);
            writeData(output, box.h);
            break;
        }
        case RDPAGE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<small_int>(output, box.mode);
            writeData<small_int>(output, box.page);
            break;
        }
        case RDFILE:
        {
            const auto &box = line.get<pdf_rect>();
            writeData<small_int>(output, box.mode);
            break;
        }
        case CALL:
        {
            const auto &call = line.get<command_call>();
            writeData(output, call.name);
            writeData(output, call.numargs);
            break;
        }
        case SELVARIDX:
        {
            const auto &var_idx = line.get<variable_idx>();
            writeData(output, var_idx.name);
            writeData(output, var_idx.index_first);
            break;
        }
        case SELVARRANGE:
        {
            const auto &var_idx = line.get<variable_idx>();
            writeData(output, var_idx.name);
            writeData(output, var_idx.index_first);
            writeData(output, var_idx.index_last);
            break;
        }
        case STRDATA:
            writeData(output, line.get<std::string>());
            break;
        case PUSHSTR:
        case SELVAR:
        case SELVARALL:
        case SELVARRANGETOP:
        case SELGLOBAL:
            writeData(output, line.get<string_ref>());
            break;
        case PUSHFLOAT:
            writeData(output, line.get<float>());
            break;
        case PUSHINT:
        case INC:
        case DEC:
        case MVBOX:
            writeData(output, line.get<small_int>());
            break;
        case JMP:
        case JZ:
        case JNZ:
        case JTE:
            writeData(output, line.get<jump_address>());
            break;
        default:
            break;
        }
    }

    for (const auto &str : m_strings) {
        writeData<command_int>(output, STRDATA);
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
        opcode cmd = static_cast<opcode>(readData<command_int>(input));
        switch(cmd) {
        case RDBOX:
        {
            pdf_rect box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            box.x = readData<float>(input);
            box.y = readData<float>(input);
            box.w = readData<float>(input);
            box.h = readData<float>(input);
            m_commands.emplace_back(RDBOX, std::move(box));
            break;
        }
        case RDPAGE:
        {
            pdf_rect box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            m_commands.emplace_back(RDPAGE, std::move(box));
            break;
        }
        case RDFILE:
        {
            pdf_rect box;
            box.type = BOX_FILE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            m_commands.emplace_back(RDFILE, std::move(box));
            break;
        }
        case CALL:
        {
            command_call call;
            call.name = readData<string_ref>(input);
            call.numargs = readData<small_int>(input);
            m_commands.emplace_back(CALL, std::move(call));
            break;
        }
        case SELVARIDX:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index_first = readData<small_int>(input);
            var_idx.index_last = var_idx.index_first;
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case SELVARRANGE:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index_first = readData<small_int>(input);
            var_idx.index_last = readData<small_int>(input);
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case STRDATA:
            m_strings.push_back(readData<std::string>(input));
            break;
        case PUSHSTR:
        case SELVAR:
        case SELVARALL:
        case SELGLOBAL:
        case SELVARRANGETOP:
            m_commands.emplace_back(cmd, readData<string_ref>(input));
            break;
        case PUSHFLOAT:
            m_commands.emplace_back(cmd, readData<float>(input));
            break;
        case PUSHINT:
        case INC:
        case DEC:
        case MVBOX:
            m_commands.emplace_back(cmd, readData<small_int>(input));
            break;
        case JMP:
        case JZ:
        case JNZ:
        case JTE:
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