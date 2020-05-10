#include "layout.h"

#include "binary_io.h"

constexpr uint32_t MAGIC = 0xb011e77a;
constexpr uint32_t VERSION = 0x00000000;

layout_bolletta::layout_bolletta() {

}

std::vector<layout_box>::iterator layout_bolletta::getBoxAt(float x, float y, int page) {
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (x > it->x && x < it->x + it->w && y > it->y && y < it->y + it->h && it->page == page) {
            return it;
        }
    }
    return boxes.end();
}

void layout_bolletta::newFile() {
    boxes.clear();
}

void layout_bolletta::saveFile(const std::string &filename) const {
    std::ofstream ofs(filename, std::ios::binary | std::ios::out);

    if (ofs.bad()) throw layout_error("Impossibile aprire il file");

    ofs << *this;

    ofs.close();
}

void layout_bolletta::saveRwops(rwops &ops) const {
    binary_io ofs(ops);

    ofs.writeInt(MAGIC);
    ofs.writeInt(VERSION);
    ofs.writeShort(boxes.size());

    for (auto &box : boxes) {
        ofs.writeString(box.name);
        ofs.writeByte(box.type);
        ofs.writeString(box.parse_string);
        ofs.writeFloat(box.x);
        ofs.writeFloat(box.y);
        ofs.writeFloat(box.w);
        ofs.writeFloat(box.h);
        ofs.writeByte(box.page);
    }
}

void layout_bolletta::openFile(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary | std::ios::in);

    if (ifs.bad()) throw layout_error("Impossibile aprire il file");

    ifs >> *this;

    ifs.close();
}

void layout_bolletta::openRwops(rwops &ops) {
    binary_io ifs(ops);

    uint32_t magic = ifs.readInt();
    if (magic != MAGIC) throw layout_error("Tipo di file invalido");
    uint32_t version = ifs.readInt();
    switch (version) {
    case 0x00000000:
    {
        uint16_t num_boxes = ifs.readShort();
        boxes.clear();
        for (size_t i=0; i<num_boxes; ++i) {
            layout_box box;
            box.name = ifs.readString();
            box.type = static_cast<box_type>(ifs.readByte());
            box.parse_string = ifs.readString();
            box.x = ifs.readFloat();
            box.y = ifs.readFloat();
            box.w = ifs.readFloat();
            box.h = ifs.readFloat();
            box.page = ifs.readByte();
            boxes.push_back(box);
        }
        break;
    }
    default:
        throw layout_error("Versione non supportata");
    }
}

std::ostream &operator << (std::ostream &out, const layout_bolletta &obj) {
    ostream_rwops ops(out);
    obj.saveRwops(ops);
    return out;
}

std::istream &operator >> (std::istream &in, layout_bolletta &obj) {
    istream_rwops ops(in);
    obj.openRwops(ops);
    return in;
}