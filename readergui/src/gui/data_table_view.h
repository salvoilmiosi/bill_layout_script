#ifndef __DATA_TABLE_VIEW_H__
#define __DATA_TABLE_VIEW_H__

#include <wx/dataview.h>

#include <concepts>
#include <functional>
#include <tuple>

#include "data_table_types.h"

class DataTableModel : public wxDataViewListStore {
protected:
    virtual int DoCompareValues(const wxVariant &obj1, const wxVariant &obj2) const {
        if (obj1.GetType() == ColumnValueVariantData::CLASS_NAME) {
            auto value1 = dynamic_cast<const ColumnValueVariantData*>(obj1.GetData());
            auto value2 = dynamic_cast<const ColumnValueVariantData*>(obj2.GetData());
            return value1->CompareTo(*value2);
        } else if (obj1.GetType() == "longlong") {
            if (obj1.GetLongLong() < obj2.GetLongLong()) return -1;
            if (obj2.GetLongLong() < obj1.GetLongLong()) return 1;
        }
        return 0;
    }
};

template<typename T> struct VariantType{};

template<> struct VariantType<wxString> { static constexpr const char *value = "string"; };
template<> struct VariantType<int> { static constexpr const char *value = "long"; };
template<> struct VariantType<wxLongLong> { static constexpr const char *value = "longlong"; };

template<std::derived_from<ColumnValueVariantData> T>
struct VariantType<T> {
    static constexpr const char *value = ColumnValueVariantData::CLASS_NAME;
};

template<typename T>
class DataTableColumnBase : public wxDataViewColumn {
public:
    DataTableColumnBase(const wxString &title, const char *variant_type, int col, int width) :
        wxDataViewColumn(title, new wxDataViewTextRenderer(variant_type), col, width,
        wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_REORDERABLE) {}
    
    virtual wxVariant constructVariant(const T &obj) = 0;
};

template<typename T, typename Value, typename ... Ts>
class DataTableColumn : public DataTableColumnBase<T> {
private:
    std::tuple<Ts ...> member_ptrs;

public:
    DataTableColumn(const wxString &title, int col, int width, Ts ... member_ptrs) :
        DataTableColumnBase<T>(title, VariantType<Value>::value, col, width), member_ptrs(member_ptrs ...) {}
    
    virtual wxVariant constructVariant(const T &obj) {
        return constructVariantImpl(obj, std::make_index_sequence<sizeof...(Ts)>{});
    }

private:
    template<typename TParam>
    constexpr auto getParameter(const T &obj, const TParam &param) {
        if constexpr (std::is_member_pointer_v<TParam>) {
            return obj.*param;
        } else if constexpr (std::is_invocable_v<TParam, const T &>) {
            return std::invoke(param, obj);
        } else {
            return param;
        }
    }

    template<size_t ... I>
    wxVariant constructVariantImpl(const T &obj, std::index_sequence<I ...>) {
        if constexpr(std::is_base_of_v<wxVariantData, Value>) {
            return new Value(getParameter(obj, std::get<I>(member_ptrs)) ...);
        } else {
            return Value(getParameter(obj, std::get<I>(member_ptrs)) ...);
        }
    }
};

template<typename T>
class DataTableView : public wxDataViewListCtrl {
public:
    DataTableView(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize,
        const wxValidator &validator = wxDefaultValidator) :
        wxDataViewListCtrl(parent, id, position, size, wxDV_ROW_LINES, validator)
    {
        DataTableModel *model = new DataTableModel;
        AssociateModel(model);
        model->DecRef();
    }

    template<typename ColumnValue, typename ... Ts>
    void AddColumn(const wxString &label, int width, Ts ... objs) {
        AppendColumn(new DataTableColumn<T, ColumnValue, Ts...>(label, GetColumnCount(), width, objs...));
    }

    size_t ResetItems(const wxVector<T> &items) {
        DeleteAllItems();

        m_data = items;

        for (const T &obj : m_data) {
            wxVector<wxVariant> row;
            for (size_t i=0; i<GetColumnCount(); ++i) {
                auto *col = dynamic_cast<DataTableColumnBase<T>*>(GetColumn(i));
                if (col) {
                    row.push_back(col->constructVariant(obj));
                } else {
                    row.push_back(wxVariant());
                }
            }
            AppendItem(row, reinterpret_cast<wxUIntPtr>(&obj));
        }

        return m_data.size();
    }

    virtual T *GetDataAt(unsigned int n) const {
        return reinterpret_cast<T *>(GetItemData(RowToItem(n)));
    }

    virtual T *GetSelectedData() {
        wxDataViewItem selected = GetSelection();
        if (selected.IsOk()) {
            return reinterpret_cast<T *>(GetItemData(selected));
        }
        return nullptr;
    }

    virtual unsigned int GetCount() const {
        return wxDataViewListCtrl::GetItemCount();
    }

    virtual void SetSelection(int n) {
        return wxDataViewListCtrl::SelectRow(n);
    }

    const wxVector<T> &GetDataVector() {
        return m_data;
    }

private:
    wxVector<T> m_data;
};

#endif