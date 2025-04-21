#include <wx/wx.h>
#include <Rest.hpp>
#include <json.hpp>
#include <wx/event.h>
#include <wx/clipbrd.h>
#include <fstream>

class Translator;
using json = nlohmann::json;

class MyApp : public wxApp
{
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(MyApp);

class MyFrame : public wxFrame
{
public:
    MyFrame();

private:
    void OnTranslate(wxCommandEvent &event);
    void ShowAndFocus();
    void OnHotkey(wxKeyEvent &event);
    void OnActivate(wxActivateEvent &event);
    void OnApi(wxCommandEvent &event);
    void OnShortcut(wxCommandEvent &event);
    void LoadConfig();
    wxTextCtrl *m_outputCtrl;
    wxTextCtrl *m_inputCtrl;
    wxButton *m_translateBtn;
    bool m_firstShow = true;
    Translator m_Translator;

    wxDECLARE_EVENT_TABLE();
};

enum
{
    ID_Translate = 2,
    ID_Hotkey = wxID_HIGHEST + 1,
    ID_Append = 3,
    ID_ShortCut = 4,
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_HOTKEY(ID_Hotkey, MyFrame::OnHotkey)
        EVT_ACTIVATE(MyFrame::OnActivate)
            wxEND_EVENT_TABLE()

                bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame();
    frame->Hide();
    frame->RegisterHotKey(ID_Hotkey, wxMOD_CONTROL | wxMOD_SHIFT, 'T');
    return true;
}

MyFrame::MyFrame()
    : wxFrame(nullptr, wxID_ANY, "Translator")
{
    wxMenuBar *menuBar = new wxMenuBar();
    wxMenu *api = new wxMenu();
    wxMenu *shortcut = new wxMenu();
    api->Append(ID_Append, "API KEY", "Enter your API KEY");
    shortcut->Append(ID_ShortCut, "ShortCut", "Enter your Shortcut");
    menuBar->Append(api, "&API");
    menuBar->Append(shortcut, "&ShortCut");

    SetMenuBar(menuBar);

    wxPanel *panel = new wxPanel(this);

    m_inputCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(400, 60));
    m_translateBtn = new wxButton(panel, ID_Translate, "Translate");
    m_outputCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RIGHT);

    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
    vbox->Add(m_inputCtrl, 0, wxEXPAND | wxALL, 5);
    vbox->Add(m_translateBtn, 0, wxEXPAND | wxALL, 5);
    vbox->Add(m_outputCtrl, 1, wxEXPAND | wxALL, 5);

    panel->SetSizer(vbox);

    Bind(wxEVT_BUTTON, &MyFrame::OnTranslate, this, ID_Translate);
    Bind(wxEVT_MENU, &MyFrame::OnApi, this, ID_Append);
    Bind(wxEVT_MENU, &MyFrame::OnShortcut, this, ID_ShortCut);
    m_inputCtrl->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnTranslate, this);
    m_translateBtn->SetDefault();
    LoadConfig();
}

void MyFrame::LoadConfig()
{
    std::ifstream in("config.json");
    if (!in)
        return;
    json j;
    in >> j;
    if (j.contains("api_key"))
    {
        m_Translator.setApiKey(j["api_key"].get<std::string>());
    }
    if (j.contains("shortcut"))
    {
        int modifiers = 0;
        auto shortcut = j["shortcut"];
        if (shortcut.value("ctrl", false))
            modifiers |= wxMOD_CONTROL;
        if (shortcut.value("alt", false))
            modifiers |= wxMOD_ALT;
        if (shortcut.value("shift", false))
            modifiers |= wxMOD_SHIFT;
        std::string keyStr = shortcut.value("key", "T");
        if (!keyStr.empty())
        {
            int keycode = toupper(keyStr[0]);
            UnregisterHotKey(ID_Hotkey);
            RegisterHotKey(ID_Hotkey, modifiers, keycode);
        }
    }
}

void MyFrame::ShowAndFocus()
{
    if (wxTheClipboard->Open())
    {
        if (wxTheClipboard->IsSupported(wxDF_TEXT))
        {
            wxTextDataObject data;
            if (wxTheClipboard->GetData(data))
            {
                m_inputCtrl->SetValue(data.GetText());
            }
        }
        wxTheClipboard->Close();
    }

    Show(true);
    Raise();
    m_inputCtrl->SetFocus();
}

void MyFrame::OnHotkey(wxKeyEvent &event)
{
    if (IsShown())
        Hide();
    else
        ShowAndFocus();
}

void MyFrame::OnActivate(wxActivateEvent &event)
{
    if (!event.GetActive())
    {
        wxWindow *focusWin = wxWindow::FindFocus();
        if (!focusWin || focusWin->GetParent() != this)
            Hide();
    }
    event.Skip();
}

void MyFrame::OnTranslate(wxCommandEvent &event)
{
    wxString word = m_inputCtrl->GetValue();
    if (word.IsEmpty())
    {
        m_outputCtrl->SetValue("Please enter a word.");
        return;
    }
    std::string translate_word = word.ToStdString();
    std::string response = m_Translator.Translate(translate_word);
    json j = json::parse(response);
    std::string persian_translation = j["persian_definition"].get<std::string>();
    wxString rtlText = wxString::FromUTF8(persian_translation);
    m_outputCtrl->SetValue(rtlText);
}

void MyFrame::OnApi(wxCommandEvent &event)
{
    ShowAndFocus();
    wxTextEntryDialog dlg(this, "Enter your API KEY:", "API Key", "");
    dlg.SetTextValidator(wxFILTER_NONE);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString apiKey = dlg.GetValue();
        m_Translator.setApiKey(apiKey.ToStdString());
        json j;
        std::ifstream in("config.json");
        if(in) in >> j;
        in.close();
        j["api_key"] = apiKey.ToStdString();
        std::ofstream out("config.json");
        out << j.dump(4);
        out.close();

        wxMessageBox("API Key set!", "Info", wxOK | wxICON_INFORMATION, this);
    }
}

void MyFrame::OnShortcut(wxCommandEvent &event)
{
    ShowAndFocus();
    wxDialog dlg(this, wxID_ANY, "Change Shortcut", wxDefaultPosition, wxDefaultSize);
    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

    wxCheckBox *ctrlBox = new wxCheckBox(&dlg, wxID_ANY, "Ctrl");
    wxCheckBox *altBox = new wxCheckBox(&dlg, wxID_ANY, "Alt");
    wxCheckBox *shiftBox = new wxCheckBox(&dlg, wxID_ANY, "Shift");
    wxTextCtrl *keyCtrl = new wxTextCtrl(&dlg, wxID_ANY, "", wxDefaultPosition, wxSize(50, -1));

    vbox->Add(ctrlBox, 0, wxALL, 5);
    vbox->Add(altBox, 0, wxALL, 5);
    vbox->Add(shiftBox, 0, wxALL, 5);
    vbox->Add(new wxStaticText(&dlg, wxID_ANY, "Key (A-Z):"), 0, wxALL, 5);
    vbox->Add(keyCtrl, 0, wxALL, 5);

    wxStdDialogButtonSizer *btnSizer = dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    vbox->Add(btnSizer, 0, wxALL | wxALIGN_CENTER, 5);

    dlg.SetSizerAndFit(vbox);

    if (dlg.ShowModal() == wxID_OK)
    {
        int modifiers = 0;
        if (ctrlBox->GetValue())
            modifiers |= wxMOD_CONTROL;
        if (altBox->GetValue())
            modifiers |= wxMOD_ALT;
        if (shiftBox->GetValue())
            modifiers |= wxMOD_SHIFT;

        wxString keyStr = keyCtrl->GetValue().Upper();
        if (keyStr.Length() == 1 && keyStr[0] >= 'A' && keyStr[0] <= 'Z')
        {
            int keycode = keyStr[0];
            UnregisterHotKey(ID_Hotkey);
            if (RegisterHotKey(ID_Hotkey, modifiers, keycode))
            {
                wxMessageBox("Shortcut changed!", "Shortcut", wxOK | wxICON_INFORMATION, this);
            }
            else
            {
                wxMessageBox("Failed to register new shortcut.", "Error", wxOK | wxICON_ERROR, this);
            }
        }
        else
        {
            wxMessageBox("Please enter a single letter (A-Z).", "Invalid Input", wxOK | wxICON_WARNING, this);
        }
    }
}