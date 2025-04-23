#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/sysopt.h>
#include <wx/textdlg.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/log.h>
#include <wx/settings.h>

#include <fstream>
#include <algorithm>
#include <cctype>

#include <Rest.hpp>
#include <json.hpp>

using json = nlohmann::json;

enum class Theme
{
    Light,
    Dark
};

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
    ~MyFrame();

private:
    void OnTranslate(wxCommandEvent &event);
    void ShowAndFocus();
    void OnHotkey(wxKeyEvent &event);
    void OnActivate(wxActivateEvent &event);
    void OnApi(wxCommandEvent &event);
    void OnShortcut(wxCommandEvent &event);
    void OnThemeSelect(wxCommandEvent &event);
    void ApplyTheme(Theme theme);
    void OnProxy(wxCommandEvent &event);
    void LoadConfig();
    void SaveConfig();

    wxTextCtrl *m_inputCtrl = nullptr;
    wxTextCtrl *m_outputCtrl = nullptr;
    wxButton *m_translateBtn = nullptr;
    wxPanel *m_panel = nullptr;

    Translator m_Translator;
    json m_config;
    Theme m_currentTheme = Theme::Light;

    wxDECLARE_EVENT_TABLE();
};

enum
{
    ID_Translate = wxID_HIGHEST + 1,
    ID_Hotkey,
    ID_Append_API,
    ID_Menu_Shortcut,
    ID_Proxy,
    ID_Theme_Light,
    ID_Theme_Dark
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_HOTKEY(ID_Hotkey, MyFrame::OnHotkey)
        wxEND_EVENT_TABLE()

            bool MyApp::OnInit()
{

    MyFrame *frame = new MyFrame();
    frame->Hide();
    wxTheApp->Yield();
    return true;
}

MyFrame::MyFrame()
    : wxFrame(nullptr, wxID_ANY, "Translator", wxDefaultPosition, wxSize(450, 300))
{
    wxMenuBar *menuBar = new wxMenuBar();
    wxMenu *optionsMenu = new wxMenu();

    optionsMenu->Append(ID_Append_API, "API KEY...\tCtrl+Shift+A", "Enter your API KEY");
    optionsMenu->Append(ID_Menu_Shortcut, "Shortcut...\tCtrl+Shift+S", "Change the global shortcut");
    optionsMenu->Append(ID_Proxy, "Proxy...\tCtrl+Shift+D", "Set Proxy");
    optionsMenu->AppendSeparator();

    optionsMenu->AppendRadioItem(ID_Theme_Light, "Light Theme", "Use the light theme");
    optionsMenu->AppendRadioItem(ID_Theme_Dark, "Dark Theme", "Use the dark theme");

    menuBar->Append(optionsMenu, "&Options");

    SetMenuBar(menuBar);

    m_panel = new wxPanel(this);

    m_inputCtrl = new wxTextCtrl(m_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_translateBtn = new wxButton(m_panel, ID_Translate, "Translate");
    m_outputCtrl = new wxTextCtrl(m_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RIGHT);

    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
    vbox->Add(m_inputCtrl, 0, wxEXPAND | wxALL, 5);
    vbox->Add(m_translateBtn, 0, wxEXPAND | wxALL, 5);
    vbox->Add(m_outputCtrl, 1, wxEXPAND | wxALL, 5);

    m_panel->SetSizer(vbox);

    vbox->SetSizeHints(this);

    Layout();

    SetClientSize(wxSize(400, 300)); // Explicitly set client size after layout

    Bind(wxEVT_BUTTON, &MyFrame::OnTranslate, this, ID_Translate);
    Bind(wxEVT_MENU, &MyFrame::OnApi, this, ID_Append_API);
    Bind(wxEVT_MENU, &MyFrame::OnShortcut, this, ID_Menu_Shortcut);
    Bind(wxEVT_MENU, &MyFrame::OnThemeSelect, this, ID_Theme_Light, ID_Theme_Dark);
    Bind(wxEVT_TEXT_ENTER, &MyFrame::OnTranslate, this, m_inputCtrl->GetId());
    Bind(wxEVT_ACTIVATE, &MyFrame::OnActivate, this);
    Bind(wxEVT_MENU,&MyFrame::OnProxy,this,ID_Proxy);

    m_translateBtn->SetDefault();

    LoadConfig();

    ApplyTheme(m_currentTheme);
}

MyFrame::~MyFrame()
{
    UnregisterHotKey(ID_Hotkey);
}

void MyFrame::LoadConfig()
{
    std::ifstream in("config.json");
    if (in.is_open())
    {
        try
        {
            in >> m_config;
        }
        catch (const json::parse_error &e)
        {
            wxLogError("Failed to parse config.json: %s", e.what());
            m_config = json({});
        }
        catch (const std::exception &e)
        {
            wxLogError("Failed to read config.json: %s", e.what());
            m_config = json({});
        }
    }
    else
    {
        wxLogVerbose("config.json not found or could not be opened. Starting with default settings.");
    }

    if (m_config.contains("api_key") && m_config["api_key"].is_string())
    {
        m_Translator.setApiKey(m_config["api_key"].get<std::string>());
        wxLogVerbose("API key loaded from config.");
    }
    else
    {
        m_Translator.setApiKey("");
        wxLogWarning("API key not found or is invalid in config.json. Please set it via the menu.");
    }

    UnregisterHotKey(ID_Hotkey);

    if (m_config.contains("proxy_ip") && m_config.contains("proxy_port"))
    {
        m_Translator.setProxy(m_config["proxy_ip"].get<std::string>(), m_config["proxy_port"].get<std::string>());
    }
    if (m_config.contains("shortcut") && m_config["shortcut"].is_object())
    {
        try
        {
            auto shortcut = m_config["shortcut"];
            bool ctrl = shortcut.value("ctrl", false);
            bool alt = shortcut.value("alt", false);
            bool shift = shortcut.value("shift", false);
            std::string keyStr = shortcut.value("key", "");

            int modifiers = 0;
            if (ctrl)
                modifiers |= wxMOD_CONTROL;
            if (alt)
                modifiers |= wxMOD_ALT;
            if (shift)
                modifiers |= wxMOD_SHIFT;

            int keycode = 0;
            if (!keyStr.empty())
            {
                keycode = static_cast<int>(std::toupper(static_cast<unsigned char>(keyStr[0])));
                if (!((keycode >= 'A' && keycode <= 'Z') || (keycode >= '0' && keycode <= '9')))
                {
                    wxLogWarning("Invalid shortcut key '%s' specified in config.json. Must be A-Z or 0-9. Hotkey not registered.", keyStr);
                    keycode = 0;
                }
            }
            else
            {
                wxLogVerbose("Shortcut key not specified in config.json. Hotkey not registered.");
            }

            if (keycode != 0)
            {
                if (!RegisterHotKey(ID_Hotkey, modifiers, keycode))
                {
                    wxLogError("Failed to register hotkey: Modifier=%d, KeyCode=%d ('%c'). Another application might be using it.", modifiers, keycode, (char)keycode);
                }
                else
                {
                    wxLogVerbose("Hotkey registered: Modifier=%d, KeyCode=%d ('%c')", modifiers, keycode, (char)keycode);
                }
            }
        }
        catch (const json::exception &e)
        {
            wxLogError("Failed to read shortcut from config.json: %s. Hotkey not registered.", e.what());
        }
        catch (const std::exception &e)
        {
            wxLogError("An error occurred processing shortcut config: %s. Hotkey not registered.", e.what());
        }
    }
    else
    {
        wxLogVerbose("Shortcut configuration not found or is invalid in config.json. Hotkey not registered.");
    }

    if (m_config.contains("theme") && m_config["theme"].is_string())
    {
        std::string themeStr = m_config["theme"].get<std::string>();
        if (themeStr == "Dark")
        {
            m_currentTheme = Theme::Dark;
            wxLogVerbose("Loaded Dark theme from config.");
        }
        else
        {
            m_currentTheme = Theme::Light;
            wxLogVerbose("Loaded Light theme from config (or invalid theme specified).");
        }
    }
    else
    {
        wxLogVerbose("Theme not found in config. Using default Light theme.");
    }

    wxMenuBar *menuBar = GetMenuBar();
    if (menuBar)
    {
        wxMenuItem *lightItem = menuBar->FindItem(ID_Theme_Light);
        wxMenuItem *darkItem = menuBar->FindItem(ID_Theme_Dark);
        if (lightItem && darkItem)
        {
            lightItem->Check(m_currentTheme == Theme::Light);
            darkItem->Check(m_currentTheme == Theme::Dark);
        }
    }
}

void MyFrame::SaveConfig()
{
    m_config["theme"] = (m_currentTheme == Theme::Dark) ? "Dark" : "Light";

    std::ofstream out("config.json");
    if (out.is_open())
    {
        try
        {
            out << m_config.dump(4);
            wxLogVerbose("Config saved to config.json");
        }
        catch (const std::exception &e)
        {
            wxLogError("Failed to write config.json: %s", e.what());
        }
    }
    else
    {
        wxLogError("Failed to open config.json for writing.");
    }
}

void MyFrame::ApplyTheme(Theme theme)
{
    wxColour bgColor, textColor, textCtrlBgColor, textCtrlTextColor;

    if (theme == Theme::Dark)
    {
        bgColor = wxColor(45, 45, 48);
        textColor = wxColor(200, 200, 200);
        textCtrlBgColor = wxColor(60, 60, 60);
        textCtrlTextColor = wxColor(220, 220, 220);
    }
    else
    {
        bgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        textColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        textCtrlBgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        textCtrlTextColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    }

    if (m_panel)
    {
        m_panel->SetBackgroundColour(bgColor);
    }

    if (m_inputCtrl)
    {
        m_inputCtrl->SetBackgroundColour(textCtrlBgColor);
        m_inputCtrl->SetForegroundColour(textCtrlTextColor);
        m_inputCtrl->SetOwnBackgroundColour(textCtrlBgColor);
        m_inputCtrl->SetOwnForegroundColour(textCtrlTextColor);
    }

    if (m_outputCtrl)
    {
        m_outputCtrl->SetBackgroundColour(textCtrlBgColor);
        m_outputCtrl->SetForegroundColour(textCtrlTextColor);
        m_outputCtrl->SetOwnBackgroundColour(textCtrlBgColor);
        m_outputCtrl->SetOwnForegroundColour(textCtrlTextColor);
    }

    if (m_panel)
    {
        m_panel->Refresh();
        m_panel->Update();
    }
    Refresh();
    Update();

    wxLogVerbose("Applied theme: %s", (theme == Theme::Dark) ? "Dark" : "Light");
}

void MyFrame::OnThemeSelect(wxCommandEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning

    Theme selectedTheme;
    if (event.GetId() == ID_Theme_Light)
    {
        selectedTheme = Theme::Light;
    }
    else if (event.GetId() == ID_Theme_Dark)
    {
        selectedTheme = Theme::Dark;
    }
    else
    {
        event.Skip();
        return;
    }

    if (selectedTheme != m_currentTheme)
    {
        m_currentTheme = selectedTheme;
        ApplyTheme(m_currentTheme);
        SaveConfig();
    }

    event.Skip();
}

void MyFrame::ShowAndFocus()
{
    wxClipboardLocker lock;
    {
        if (wxTheClipboard->IsSupported(wxDF_TEXT))
        {
            wxTextDataObject data;
            if (wxTheClipboard->GetData(data))
            {
                m_inputCtrl->SetValue(data.GetText());
                m_inputCtrl->SetInsertionPointEnd();
                wxLogVerbose("Pasted text from clipboard.");
            }
            else
            {
                wxLogVerbose("Clipboard contained text data object but GetData failed.");
            }
        }
        else
        {
            wxLogVerbose("Clipboard does not contain text data.");
        }
    }

    Show(true);
    Raise();
    m_inputCtrl->SetFocus();
}

void MyFrame::OnHotkey(wxKeyEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning

    if (IsShown())
        Hide();
    else
        ShowAndFocus();

    event.Skip();
}

void MyFrame::OnActivate(wxActivateEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning

    if (!event.GetActive())
    {
        wxWindow *focusWin = wxWindow::FindFocus();
        if (!focusWin || !this->IsDescendant(focusWin))
        {
            Hide();
        }
    }
    event.Skip();
}

void MyFrame::OnTranslate(wxCommandEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning (if event ID or object wasn't used)
    // NOTE: This handler actually uses event.GetId() implicitly via Bind(),
    // so the warning might only appear if compiled with different flags
    // or if the event parameter was truly unused in the function body.
    // I'll keep the (void)event; for consistency with the requested cleanup.

    wxString word = m_inputCtrl->GetValue();
    if (word.IsEmpty())
    {
        m_outputCtrl->SetValue("Please enter a word.");
        return;
    }

    std::string response;
    std::string translate_word = word.ToStdString();

    translate_word.erase(
        std::remove_if(translate_word.begin(), translate_word.end(),
                       [](unsigned char c)
                       { return c < 32 || c > 126; }),
        translate_word.end());

    if (m_Translator.getApiKey().empty())
    {
        m_outputCtrl->SetValue("API Key is not set. Please go to Options > API KEY...");
        return;
    }

    try
    {
        response = m_Translator.Translate(translate_word);
        wxLogVerbose("Translation API call successful for word: '%s'", translate_word);
    }
    catch (const std::exception &e)
    {
        m_outputCtrl->SetValue(wxString::Format("Translation API call failed: %s", e.what()));
        wxLogError("Translation API call failed: %s", e.what());
        return;
    }

    try
    {
        json j = json::parse(response);
        wxLogVerbose("API response parsed successfully.");

        std::string persian_translation = j.value("persian_definition", "Translation definition not found in response.");

        wxString rtlText = wxString::FromUTF8(persian_translation.c_str());

        m_outputCtrl->SetValue(rtlText);
        wxLogVerbose("Displayed translation result.");
    }
    catch (const json::parse_error &e)
    {
        m_outputCtrl->SetValue(wxString::Format("Error parsing API response (invalid JSON): %s\nRaw Response: %s", e.what(), response.c_str()));
        wxLogError("JSON parse error: %s", e.what());
    }
    catch (const json::exception &e)
    {
        m_outputCtrl->SetValue(wxString::Format("Error reading data from API response: %s\nRaw Response: %s", e.what(), response.c_str()));
        wxLogError("JSON data access error: %s", e.what());
    }
    catch (const std::exception &e)
    {
        m_outputCtrl->SetValue(wxString::Format("An unexpected error occurred processing the translation: %s", e.what()));
        wxLogError("Unexpected error during processing: %s", e.what());
    }
}

void MyFrame::OnApi(wxCommandEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning

    wxTextEntryDialog dlg(this, "Enter your API KEY:", "API Key", m_Translator.getApiKey());

    if (dlg.ShowModal() == wxID_OK)
    {
        wxString apiKey = dlg.GetValue();
        std::string apiKeyStd = apiKey.ToStdString();

        m_Translator.setApiKey(apiKeyStd);

        m_config["api_key"] = apiKeyStd;
        SaveConfig();

        wxMessageBox("API Key set!", "Info", wxOK | wxICON_INFORMATION, this);
        wxLogVerbose("API Key updated via dialog.");
    }
}

void MyFrame::OnShortcut(wxCommandEvent &event)
{
    (void)event; // Avoid unreferenced parameter warning

    wxDialog dlg(this, wxID_ANY, "Change Shortcut", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

    auto current_shortcut = m_config.value("shortcut", json({}));
    bool current_ctrl = current_shortcut.value("ctrl", false);
    bool current_alt = current_shortcut.value("alt", false);
    bool current_shift = current_shortcut.value("shift", false);
    std::string current_key_str = current_shortcut.value("key", "");
    wxString current_key_wx = wxString(current_key_str).Upper();

    wxCheckBox *ctrlBox = new wxCheckBox(&dlg, wxID_ANY, "Ctrl");
    ctrlBox->SetValue(current_ctrl);

    wxCheckBox *altBox = new wxCheckBox(&dlg, wxID_ANY, "Alt");
    altBox->SetValue(current_alt);

    wxCheckBox *shiftBox = new wxCheckBox(&dlg, wxID_ANY, "Shift");
    shiftBox->SetValue(current_shift);

    wxTextCtrl *keyCtrl = new wxTextCtrl(&dlg, wxID_ANY, current_key_wx, wxDefaultPosition, wxSize(50, -1));
    keyCtrl->SetMaxLength(1);

    vbox->Add(new wxStaticText(&dlg, wxID_ANY, "Modifiers:"), 0, wxALL, 5);
    vbox->Add(ctrlBox, 0, wxALL | wxLEFT | wxRIGHT, 15);
    vbox->Add(altBox, 0, wxALL | wxLEFT | wxRIGHT, 15);
    vbox->Add(shiftBox, 0, wxALL | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    vbox->Add(new wxStaticText(&dlg, wxID_ANY, "Key (A-Z, 0-9):"), 0, wxALL | wxTOP, 5);
    vbox->Add(keyCtrl, 0, wxALL, 5);

    wxStdDialogButtonSizer *btnSizer = dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    vbox->Add(btnSizer, 0, wxALL | wxALIGN_CENTER, 10);

    dlg.SetSizerAndFit(vbox);
    dlg.Centre(wxCENTER_ON_SCREEN);

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
        int keycode = 0;
        std::string keyStrStd;

        if (keyStr.Length() == 1)
        {
            char c = keyStr[0];
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            {
                keycode = c;
                keyStrStd = keyStr.ToStdString();
            }
            else
            {
                wxMessageBox("Please enter a single letter (A-Z) or digit (0-9).", "Invalid Input", wxOK | wxICON_WARNING, this);
                return;
            }
        }
        else
        {
            wxMessageBox("Please enter a single letter (A-Z) or digit (0-9).", "Invalid Input", wxOK | wxICON_WARNING, this);
            return;
        }

        UnregisterHotKey(ID_Hotkey);

        if (keycode != 0)
        {
            if (RegisterHotKey(ID_Hotkey, modifiers, keycode))
            {
                m_config["shortcut"]["ctrl"] = ctrlBox->GetValue();
                m_config["shortcut"]["shift"] = shiftBox->GetValue();
                m_config["shortcut"]["alt"] = altBox->GetValue();
                m_config["shortcut"]["key"] = keyStrStd;

                SaveConfig();

                wxMessageBox("Shortcut changed successfully!\nNew shortcut: " + keyStr, "Shortcut", wxOK | wxICON_INFORMATION, this);
                wxLogVerbose("Shortcut updated to Modifier=%d, KeyCode=%d ('%c')", modifiers, keycode, (char)keycode);
            }
            else
            {
                wxMessageBox("Failed to register new shortcut.\nAnother application might be using it.", "Error", wxOK | wxICON_ERROR, this);
                wxLogError("Failed to register new hotkey: Modifier=%d, KeyCode=%d ('%c')", modifiers, keycode, (char)keycode);
            }
        }
        else
        {
            wxMessageBox("Invalid key specified for shortcut.", "Error", wxOK | wxICON_ERROR, this);
            wxLogError("Invalid keycode (0) after shortcut dialog processing.");
        }
    }
}

void MyFrame::OnProxy(wxCommandEvent &event)
{
    wxDialog dlg(this, wxID_ANY, "Set Proxy", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

    wxTextCtrl *ipCtrl = new wxTextCtrl(&dlg, wxID_ANY, m_config.value("proxy_ip", ""), wxDefaultPosition, wxSize(150, -1));
    wxTextCtrl *portCtrl = new wxTextCtrl(&dlg, wxID_ANY, m_config.value("proxy_port", ""), wxDefaultPosition, wxSize(80, -1));
    
    vbox->Add(new wxStaticText(&dlg, wxID_ANY, "Proxy IP:"), 0, wxALL, 5);
    vbox->Add(ipCtrl, 0, wxALL, 5);
    vbox->Add(new wxStaticText(&dlg, wxID_ANY, "Proxy Port:"), 0, wxALL, 5);
    vbox->Add(portCtrl, 0, wxALL, 5);

    wxStdDialogButtonSizer *btnSizer = dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    vbox->Add(btnSizer, 0, wxALL | wxALIGN_CENTER, 10);

    dlg.SetSizerAndFit(vbox);
    dlg.Centre(wxCENTER_ON_SCREEN);

    if (dlg.ShowModal() == wxID_OK)
    {
        wxString ip = ipCtrl->GetValue();
        wxString port = portCtrl->GetValue();

        m_config["proxy_ip"] = ip.ToStdString();
        m_config["proxy_port"] = port.ToStdString();
        SaveConfig();
        m_Translator.setProxy(ip.ToStdString(), port.ToStdString());

        wxMessageBox("Proxy settings saved.", "Proxy", wxOK | wxICON_INFORMATION, this);
    }
}