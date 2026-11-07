#include "ui/FileDialog.h"

#include "model/Language.h"
#include "ui/Key.h"
#include "ui/Text.h"
#include "ui/Theme.h"

#include <wx/dialog.h>
#include <wx/filectrl.h>
#include <wx/intl.h>
#include <wx/filename.h>
#include <wx/generic/filectrlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <vector>

namespace {
std::string text_of(char const* key) {
    return lookup(current_language(), key);
}
}  // namespace

namespace {

/// "Images (*.jpg *.png);;All (*)" as the wx wildcard "Images (*.jpg *.png)|*.jpg;*.png|All (*)|*";
/// a string that is a wx wildcard already (it has a '|') is kept.
wxString wildcard_of(wxString const& filter) {
    if (filter.empty() || filter.Contains("|"))
        return filter.empty() ? wxString(wxFileSelectorDefaultWildcardStr) : filter;
    wxString out;
    wxString rest = filter;
    while (!rest.empty()) {
        wxString entry = rest.BeforeFirst(';');
        if (rest.Contains(";;")) {
            entry = rest.BeforeFirst(';');
            rest = rest.AfterFirst(';').AfterFirst(';');
        } else {
            entry = rest;
            rest.clear();
        }
        wxString patterns = entry.AfterLast('(').BeforeLast(')');
        if (patterns.empty())
            patterns = "*";
        patterns.Trim().Trim(false);
        patterns.Replace(" ", ";");
        if (!out.empty())
            out += "|";
        out += entry + "|" + patterns;
    }
    return out;
}

/// A directory name is inserted as *wxBLUE (wxFileData::MakeItem). The generic
/// list draws that colour, and on the black field it is too dark to read, so
/// the row takes the field's text (white). A file keeps the list's foreground.
void colour_directory(wxListCtrl* list, long row) {
    if (light_theme())
        return;
    auto* data = static_cast<wxFileData*>(wxUIntToPtr(list->GetItemData(row)));
    if (data && data->IsDir())
        list->SetItemTextColour(row, palette(false).field_text);
}

/// The file dialog: the toolbar of keys over a generic file control, the
/// themed Open or Save and Cancel keys below.
class FileDialog : public wxDialog {
public:
    FileDialog(wxWindow* parent, wxString const& title, wxString const& folder,
               wxString const& name, wxString const& filter, bool saving, wxString const& suffix)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          saving_(saving),
          suffix_(suffix) {
        SetName("contorno-file-dialog");
        SetLayoutDirection(layout_direction());
        files_ = new wxGenericFileCtrl(this, wxID_ANY, folder, name, wildcard_of(filter),
                                       (saving ? wxFC_SAVE : wxFC_OPEN) | wxFC_NOSHOWHIDDEN);
        auto* list = files_->GetFileList();
        list->Bind(wxEVT_LIST_INSERT_ITEM, [list](wxListEvent& event) {
            colour_directory(list, event.GetIndex());
            event.Skip();
        });
        for (long row = 0; row < list->GetItemCount(); ++row)
            colour_directory(list, row);
        history_.push_back(files_->GetShownDirectory());

        auto* tools = new wxBoxSizer(wxHORIZONTAL);
        tools->Add(new wxStaticText(this, wxID_ANY, wx(text_of("file.lookIn"))), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
        back_ = tool("back", wxGetTranslation("Back"), [this] { go(history_index_ - 1); }, tools);
        forward_ =
            tool("forward", wxGetTranslation("Forward"), [this] { go(history_index_ + 1); }, tools);
        tool(
            "up", wxGetTranslation("Go to parent directory"), [this] { files_->GoToParentDir(); },
            tools);
        tool(
            "new folder", wxGetTranslation("Create new directory"), [this] { new_folder(); },
            tools);
        tools->AddStretchSpacer();
        tool(
            "list", wxGetTranslation("View files as a list view"),
            [this] { files_->ChangeToListMode(); }, tools);
        tool(
            "details", wxGetTranslation("View files as a detailed view"),
            [this] { files_->ChangeToReportMode(); }, tools);
        update_history_keys();

        // the file control's own field and choice carry no captions
        auto* captions = new wxBoxSizer(wxHORIZONTAL);
        captions->Add(new wxStaticText(this, wxID_ANY, wx(text_of("file.fileName"))), 0, wxRIGHT,
                      24);
        captions->Add(new wxStaticText(this, wxID_ANY, wx(text_of("file.fileType"))));

        auto* keys = new wxBoxSizer(wxHORIZONTAL);
        keys->AddStretchSpacer();
        auto* accept =
            new Key(this, wxID_OK, wx(text_of(saving ? "file.save" : "file.open")), {}, 6, 18);
        auto* cancel = new Key(this, wxID_CANCEL, wx(text_of("common.cancel")), {}, 6, 18);
        keys->Add(accept, 0, wxRIGHT, 10);
        keys->Add(cancel);

        auto* column = new wxBoxSizer(wxVERTICAL);
        column->Add(tools, 0, wxEXPAND | wxALL, 10);
        column->Add(files_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
        column->Add(captions, 0, wxLEFT | wxRIGHT | wxTOP, 10);
        column->Add(keys, 0, wxEXPAND | wxALL, 10);
        SetSizerAndFit(column);
        SetSize(std::max(GetSize().x, FromDIP(680)), std::max(GetSize().y, FromDIP(460)));
        SetEscapeId(wxID_CANCEL);

        Bind(wxEVT_BUTTON, &FileDialog::on_accept, this, wxID_OK);
        Bind(wxEVT_FILECTRL_FOLDERCHANGED,
             [this](wxFileCtrlEvent&) { entered(files_->GetShownDirectory()); });
        Bind(wxEVT_FILECTRL_FILEACTIVATED, [this](wxFileCtrlEvent&) {
            wxCommandEvent accept(wxEVT_BUTTON, wxID_OK);
            GetEventHandler()->ProcessEvent(accept);
        });
        apply_theme(this, light_theme());
        CentreOnParent();
    }

    wxString path() const { return path_; }

private:
    template <class F>
    Key* tool(wxString const& icon, wxString const& name, F action, wxBoxSizer* row) {
        auto* key = new Key(this, wxID_ANY, "", tinted_icon(icon), 4, 8);
        key->SetToolTip(name);
        key->SetName(name);
        key->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
        row->Add(key, 0, wxRIGHT, 6);
        return key;
    }

    void entered(wxString const& dir) {
        if (navigating_ || dir == history_[history_index_])
            return;
        history_.resize(history_index_ + 1);
        history_.push_back(dir);
        ++history_index_;
        update_history_keys();
    }
    void go(int index) {
        if (index < 0 || index >= static_cast<int>(history_.size()))
            return;
        navigating_ = true;
        history_index_ = index;
        files_->SetDirectory(history_[index]);
        navigating_ = false;
        update_history_keys();
    }
    void update_history_keys() {
        back_->Enable(history_index_ > 0);
        forward_->Enable(history_index_ + 1 < static_cast<int>(history_.size()));
    }
    void new_folder() {
        wxTextEntryDialog ask(this, wxGetTranslation("Create new directory"),
                              wxGetTranslation("Create new directory"), "");
        apply_theme(&ask, light_theme());
        if (ask.ShowModal() != wxID_OK || ask.GetValue().empty())
            return;
        wxFileName const made = wxFileName::DirName(
            files_->GetShownDirectory() + wxFileName::GetPathSeparator() + ask.GetValue());
        if (made.Mkdir(wxS_DIR_DEFAULT, 0))
            files_->SetDirectory(made.GetFullPath());
    }

    /// Open: an existing file (a folder is entered). Save: the typed name, with the suffix when it
    /// has none.
    void on_accept(wxCommandEvent& event) {
        wxString const typed = files_->GetPath();
        if (typed.empty())
            return;
        if (wxFileName::DirExists(typed)) {
            files_->SetDirectory(typed);
            return;
        }
        wxFileName name(typed);
        if (saving_) {
            if (!suffix_.empty() && name.GetExt().empty())
                name.SetExt(suffix_);
        } else if (!name.FileExists()) {
            return;
        }
        // wx's own question, so wx's catalogues translate it
        if (saving_ && name.FileExists() &&
            !confirm(this, wxGetTranslation("Confirm"),
                     wxString::Format(
                         wxGetTranslation(
                             "File '%s' already exists, do you really want to overwrite it?"),
                         name.GetFullName())))
            return;
        path_ = name.GetFullPath();
        event.Skip();
    }

    bool saving_;
    wxString suffix_, path_;
    wxGenericFileCtrl* files_ = nullptr;
    Key *back_ = nullptr, *forward_ = nullptr;
    std::vector<wxString> history_;
    int history_index_ = 0;
    bool navigating_ = false;
};

wxString choose_file(wxWindow* parent, wxString const& title, wxString const& folder,
                     wxString const& name, wxString const& filter, bool saving,
                     wxString const& suffix) {
    FileDialog dialog(parent, title, folder, name, filter, saving, suffix);
    return dialog.ShowModal() == wxID_OK ? dialog.path() : wxString();
}

}  // namespace

wxString choose_open_file(wxWindow* parent, wxString const& title, wxString const& folder,
                          wxString const& filter) {
    return choose_file(parent, title, folder, "", filter, false, {});
}

wxString choose_save_file(wxWindow* parent, wxString const& title, wxString const& suggested,
                          wxString const& filter, wxString const& suffix) {
    wxFileName const info(suggested);
    return choose_file(parent, title, info.GetPath(), info.GetFullName(), filter, true, suffix);
}
