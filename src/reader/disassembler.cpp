#include "disassembler.h"

template<typename T> T readData(std::istream &input) {
    T buffer;
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
    return buffer;
}

void disassembler::read_bytecode(std::istream &input) {
    m_commands.clear();
    m_strings.clear();

    while (!input.eof()) {
        asm_command cmd = static_cast<asm_command>(readData<command_int>(input));
        switch(cmd) {
        case RDBOX:
        {
            command_box box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            box.ref_spacers = readData<string_ref>(input);
            box.x = readData<float>(input);
            box.y = readData<float>(input);
            box.w = readData<float>(input);
            box.h = readData<float>(input);
            m_commands.emplace_back(RDBOX, std::move(box));
            break;
        }
        case RDPAGE:
        {
            command_box box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            box.ref_spacers = readData<string_ref>(input);
            m_commands.emplace_back(RDPAGE, std::move(box));
            break;
        }
        case RDFILE:
        {
            command_box box;
            box.type = BOX_WHOLE_FILE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.ref_spacers = 0xffff;
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
        case SPACER:
        {
            command_spacer spacer;
            spacer.name = readData<string_ref>(input);
            spacer.w = readData<float>(input);
            spacer.h = readData<float>(input);
            m_commands.emplace_back(SPACER, std::move(spacer));
            break;
        }
        case SELVARIDX:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index = readData<small_int>(input);
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case STRDATA:
        {
            string_size len = readData<string_size>(input);
            std::string buffer(len, '\0');
            input.read(buffer.data(), len);
            m_strings.push_back(std::move(buffer));
            break;
        }
        case ERROR:
        case PUSHSTR:
        case SELVAR:
        case SELGLOBAL:
            m_commands.emplace_back(cmd, readData<string_ref>(input));
            break;
        case PUSHFLOAT:
            m_commands.emplace_back(cmd, readData<float>(input));
            break;
        case PUSHINT:
        case INC:
        case DEC:
            m_commands.emplace_back(cmd, readData<small_int>(input));
            break;
        case JMP:
        case JZ:
        case JTE:
            m_commands.emplace_back(cmd, readData<jump_address>(input));
            break;
        default:
            m_commands.emplace_back(cmd);
        }
    }
}

const std::string &disassembler::get_string(string_ref ref) {
    static const std::string STR_EMPTY;
    if (ref == 0xffff) {
        return STR_EMPTY;
    } else {
        return m_strings[ref];
    }
}