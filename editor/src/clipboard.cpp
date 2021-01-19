#include "clipboard.h"
#include <wx/clipbrd.h>

class BoxDataObject : public wxCustomDataObject {
public:
    BoxDataObject() : wxCustomDataObject("layout_box") {}
    
    BoxDataObject(const layout_box &box);

    layout_box GetLayoutBox() const;
};

BoxDataObject::BoxDataObject(const layout_box &box) : BoxDataObject() {
    wxMemoryBuffer buffer;

    auto add_data = [&](const auto &data) { buffer.AppendData(&data, sizeof(data)); };
    auto add_string = [&](const std::string &data) { add_data(data.size()); buffer.AppendData(data.data(), data.size()); };

    add_data(box.x);
    add_data(box.y);
    add_data(box.w);
    add_data(box.h);
    add_data(box.page);
    add_data(box.mode);
    add_data(box.type);
    add_data(box.selected);
    add_string(box.name);
    add_string(box.script);
    add_string(box.spacers);
    add_string(box.goto_label);
    
    SetData(buffer.GetDataLen(), buffer.GetData());
}

layout_box BoxDataObject::GetLayoutBox() const {
    layout_box box;

    char *ptr = (char *) GetData();
    size_t datalen = GetDataSize();
    size_t offset = 0;

    auto get_data = [&](auto &data) {
        if (offset + sizeof(data) > datalen) throw std::out_of_range("Invalid data");
        memcpy(&data, ptr + offset, sizeof(data));
        offset += sizeof(data);
    };
    auto get_string = [&](std::string &data) {
        size_t len;
        get_data(len);
        if (offset + len > datalen) throw std::out_of_range("Invalid data");
        data.resize(len);
        memcpy(data.data(), ptr + offset, len);
        offset += len;
    };

    get_data(box.x);
    get_data(box.y);
    get_data(box.w);
    get_data(box.h);
    get_data(box.page);
    get_data(box.mode);
    get_data(box.type);
    get_data(box.selected);
    get_string(box.name);
    get_string(box.script);
    get_string(box.spacers);
    get_string(box.goto_label);

    return box;
}

bool SetClipboard(const layout_box &box) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new BoxDataObject(box));
        wxTheClipboard->Close();
        return true;
    }
    return false;
}

bool GetClipboard(layout_box &box) {
    if (wxTheClipboard->Open()) {
        if (BoxDataObject clip_data; wxTheClipboard->GetData(clip_data)) {
            box = clip_data.GetLayoutBox();
            wxTheClipboard->Close();
            return true;
        }
    }
    wxTheClipboard->Close();
    return false;
}