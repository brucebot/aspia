//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/save_report_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__SAVE_REPORT_DIALOG_H
#define _ASPIA_UI__SYSTEM_INFO__SAVE_REPORT_DIALOG_H

#include "base/macros.h"
#include "protocol/category.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class SaveReportDialog :
    public CDialogImpl<SaveReportDialog>,
    public CDialogResize<SaveReportDialog>
{
public:
    enum { IDD = IDD_SAVE_REPORT };

    SaveReportDialog() = default;
    ~SaveReportDialog() = default;

    CategoryGuidList& GetSelectedGuidList();

private:
    BEGIN_MSG_MAP(SaveReportDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_SELECT_ALL, OnSelectAllButton)
        COMMAND_ID_HANDLER(IDC_UNSELECT_ALL, OnUnselectAllButton)
        COMMAND_ID_HANDLER(IDOK, OnSaveButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        NOTIFY_CODE_HANDLER(TVN_ITEMCHANGED, OnTreeItemChanged)

        CHAIN_MSG_MAP(CDialogResize<SaveReportDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(SaveReportDialog)
        DLGRESIZE_CONTROL(IDC_CATEGORY_TREE, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDC_SELECT_ALL, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDC_UNSELECT_ALL, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSelectAllButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnUnselectAllButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnSaveButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);

    LRESULT OnTreeItemChanged(int control_id, LPNMHDR hdr, BOOL& handled);

    void BuildGuidList(CTreeViewCtrl& treeview, HTREEITEM parent_item);

    void AddChildItems(CTreeViewCtrl& treeview,
                       const CSize& icon_size,
                       const CategoryList& tree,
                       HTREEITEM parent_tree_item);
    static void SetCheckStateForChildItems(CTreeViewCtrl& treeview,
                                           HTREEITEM parent_item,
                                           BOOL state);

    CIcon small_icon_;
    CIcon big_icon_;
    CIcon select_all_icon_;
    CIcon unselect_all_icon_;

    CImageListManaged imagelist_;

    CategoryGuidList selected_list_;
    CategoryList category_tree_;
    bool checkbox_rebuild_ = false;

    DISALLOW_COPY_AND_ASSIGN(SaveReportDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__SAVE_REPORT_DIALOG_H
