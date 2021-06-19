#pragma once

#include "keybindingeditor.h"
#include "siteeditor.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class PreferencesDialog : public Gtk::Dialog
    {
    public:
        PreferencesDialog(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~PreferencesDialog() override = default;

        SiteEditor* get_site_editor() const { return m_SiteEditor; }
        KeybindingEditor* get_keybinding_editor() const { return m_KeybindingEditor; }

        // This can also be set from the main window via the checkbutton in the
        // message dialog (when the setting is true)
        void set_ask_delete_confirm(const bool val) { m_AskDeleteConfirm->set_active(val); }

        void set_has_rgba_visual(const bool val) { m_BGColor->set_use_alpha(val); }

        sigc::signal<void> signal_bg_color_set() const { return m_SignalBGColorSet; }
        sigc::signal<void> signal_cursor_hide_delay_changed() const
        {
            return m_SpinSignals.at("CursorHideDelay");
        }
        sigc::signal<void> signal_cache_size_changed() const
        {
            return m_SpinSignals.at("CacheSize");
        }
        sigc::signal<void> signal_slideshow_delay_changed() const
        {
            return m_SpinSignals.at("SlideshowDelay");
        }
        sigc::signal<void> signal_title_format_changed() const
        {
            return m_SignalTitleFormatChanged;
        }
        sigc::signal<void, bool> signal_ask_delete_confirm_changed() const
        {
            return m_SignalAskDeleteConfirmChanged;
        }

    private:
        struct BooruMaxRatingModelColumns : public Gtk::TreeModelColumnRecord
        {
            BooruMaxRatingModelColumns() { add(text_column); }
            Gtk::TreeModelColumn<std::string> text_column;
        };

        Gtk::ColorButton* m_BGColor;
        SiteEditor* m_SiteEditor;
        KeybindingEditor* m_KeybindingEditor;
        Gtk::CheckButton* m_AskDeleteConfirm;

        const std::map<std::string, sigc::signal<void>> m_SpinSignals;
        sigc::signal<void> m_SignalBGColorSet, m_SignalTitleFormatChanged;
        sigc::signal<void, bool> m_SignalAskDeleteConfirmChanged;
    };
}
