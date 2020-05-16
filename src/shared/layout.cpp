#include "layout.h"

#include <json/json.h>

constexpr uint32_t MAGIC = 0xb011e77a;
constexpr uint32_t VERSION = 0x00000001;
constexpr float RESIZE_TOLERANCE = 10.f;

bill_layout_script::bill_layout_script() {

}

box_reference bill_layout_script::getBoxAt(float x, float y, int page) {
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (x > it->x && x < it->x + it->w && y > it->y && y < it->y + it->h && it->page == page) {
            return it;
        }
    }
    return boxes.end();
}

std::pair<box_reference, int> bill_layout_script::getBoxResizeNode(float x, float y, int page, std::pair<float, float> scale) {
    float nw = RESIZE_TOLERANCE / scale.first;
    float nh = RESIZE_TOLERANCE / scale.second;
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (x > it->x - nw && x < it->x + it->w + nw && y > it->y - nh && y < it->y + it->h + nh && it->page == page) {
            int node = 0;
            if (x > it->x - nw && x < it->x + nw) {
                node |= RESIZE_LEFT;
            }
            if (x > it->x + it->w - nw && x < it->x + it->w + nw) {
                node |= RESIZE_RIGHT;
            }
            if (y > it->y - nh && y < it->y + nh) {
                node |= RESIZE_TOP;
            }
            if (y > it->y + it->h - nh && y < it->y + it->h + nh) {
                node |= RESIZE_BOTTOM;
            }
            return std::make_pair(it, node);
        }
    }
    return std::make_pair(boxes.end(), 0);
}

std::ostream &operator << (std::ostream &out, const bill_layout_script &obj) {
    Json::Value root = Json::objectValue;
    root["version"] = VERSION;
    
    Json::Value &layout = root["layout"]  = Json::objectValue;
    layout["pdf_filename"] = obj.pdf_filename;

    Json::Value &json_boxes = layout["boxes"] = Json::arrayValue;

    for (auto &box : obj.boxes) {
        Json::Value json_box = Json::objectValue;

        json_box["name"] = box.name;
        json_box["type"] = box.type;
        json_box["spacers"] = box.spacers;
        json_box["script"] = box.script;
        json_box["x"] = box.x;
        json_box["y"] = box.y;
        json_box["w"] = box.w;
        json_box["h"] = box.h;
        json_box["page"] = box.page;
        json_box["mode"] = box.mode;

        json_boxes.append(json_box);
    }

    return out << root;
}

std::istream &operator >> (std::istream &in, bill_layout_script &obj) {
    Json::Value root;

    try {
        in >> root;
        int version = root["version"].asInt();

        switch(version) {
        case 0x00000001:
        {
            Json::Value &layout = root["layout"];
            obj.pdf_filename = layout["pdf_filename"].asString();
            Json::Value &json_boxes = layout["boxes"];
            for (Json::Value::iterator it=json_boxes.begin(); it!=json_boxes.end(); ++it) {
                Json::Value &json_box = *it;
                layout_box box;
                box.name = json_box["name"].asString();
                box.type = static_cast<box_type>(json_box["type"].asInt());
                box.spacers = json_box["spacers"].asString();
                box.script = json_box["script"].asString();
                box.x = json_box["x"].asFloat();
                box.y = json_box["y"].asFloat();
                box.w = json_box["w"].asFloat();
                box.h = json_box["h"].asFloat();
                box.page = json_box["page"].asInt();
                box.mode = static_cast<read_mode>(json_box["mode"].asInt());

                obj.boxes.push_back(box);
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