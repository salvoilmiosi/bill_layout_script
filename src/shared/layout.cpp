#include "layout.h"

#include <json/json.h>

constexpr uint32_t MAGIC = 0xb011e77a;
constexpr uint32_t VERSION = 0x00000001;

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

std::ostream &operator << (std::ostream &out, const layout_bolletta &obj) {
    Json::Value root = Json::objectValue;
    Json::Value &layout = root["layout"]  = Json::objectValue;
    layout["version"] = VERSION;
    layout["pdf_filename"] = obj.pdf_filename;

    Json::Value &json_boxes = layout["boxes"] = Json::arrayValue;

    for (auto &box : obj.boxes) {
        Json::Value json_box = Json::objectValue;

        json_box["name"] = box.name;
        json_box["type"] = box.type;
        json_box["parse_string"] = box.parse_string;
        json_box["x"] = box.x;
        json_box["y"] = box.y;
        json_box["w"] = box.w;
        json_box["h"] = box.h;
        json_box["page"] = box.page;

        json_boxes.append(json_box);
    }

    return out << root;
}

std::istream &operator >> (std::istream &in, layout_bolletta &obj) {
    Json::Value root;

    in >> root;

    Json::Value &layout = root["layout"];

    int version = layout["version"].asInt();

    switch(version) {
    case 0x00000001:
    {
        obj.pdf_filename = layout["pdf_filename"].asString();
        Json::Value &json_boxes = layout["boxes"];
        for (Json::Value::iterator it=json_boxes.begin(); it!=json_boxes.end(); ++it) {
            Json::Value &json_box = *it;
            layout_box box;
            box.name = json_box["name"].asString();
            box.type = static_cast<box_type>(json_box["type"].asInt());
            box.parse_string = json_box["parse_string"].asString();
            box.x = json_box["x"].asFloat();
            box.y = json_box["y"].asFloat();
            box.w = json_box["w"].asFloat();
            box.h = json_box["h"].asFloat();
            box.page = json_box["page"].asInt();

            obj.boxes.push_back(box);
        }
        break;
    }
    default:
        throw layout_error("Versione non supportata");
    }

    return in;
}