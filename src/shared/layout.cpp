#include "layout.h"

#include <json/json.h>

constexpr uint32_t VERSION = 0x00000001;

std::ostream &operator << (std::ostream &out, const bill_layout_script &boxes) {
    Json::Value root = Json::objectValue;
    root["version"] = VERSION;
    
    Json::Value &layout = root["layout"] = Json::objectValue;

    Json::Value &json_boxes = layout["boxes"] = Json::arrayValue;

    for (auto &box : boxes) {
        Json::Value json_box = Json::objectValue;

        json_box["name"] = box.name;
        json_box["spacers"] = box.spacers;
        json_box["script"] = box.script;
        json_box["goto_label"] = box.goto_label;
        json_box["x"] = box.x;
        json_box["y"] = box.y;
        json_box["w"] = box.w;
        json_box["h"] = box.h;
        json_box["page"] = box.page;
        json_box["mode"] = box.mode;
        json_box["type"] = box.type;

        json_boxes.append(json_box);
    }

    return out << root;
}

std::istream &operator >> (std::istream &in, bill_layout_script &boxes) {
    Json::Value root;

    try {
        in >> root;
        int version = root["version"].asInt();

        switch(version) {
        case 0x00000001:
        {
            Json::Value &layout = root["layout"];
            Json::Value &json_boxes = layout["boxes"];
            for (Json::Value::iterator it=json_boxes.begin(); it!=json_boxes.end(); ++it) {
                Json::Value &json_box = *it;
                layout_box box;
                box.name = json_box["name"].asString();
                box.spacers = json_box["spacers"].asString();
                box.script = json_box["script"].asString();
                box.goto_label = json_box["goto_label"].asString();
                box.x = json_box["x"].asFloat();
                box.y = json_box["y"].asFloat();
                box.w = json_box["w"].asFloat();
                box.h = json_box["h"].asFloat();
                box.page = json_box["page"].asInt();
                box.mode = static_cast<read_mode>(json_box["mode"].asInt());
                box.type = static_cast<box_type>(json_box["type"].asInt());

                boxes.push_back(box);
            }
            break;
        }
        default:
            throw layout_error("Versione non supportata");
        }
    } catch (const std::exception &error) {
        throw layout_error("Impossibile leggere questo file");
    }

    return in;
}