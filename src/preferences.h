#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include "keybindingeditor.h"
#include "siteeditor.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class PreferencesDialog : public Gtk::Dialog
    {
    public:
        PreferencesDialog(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        virtual ~PreferencesDialog() override = default;

        SiteEditor* get_site_editor() const { return m_SiteEditor; }
        KeybindingEditor* get_keybinding_editor() const { return m_KeybindingEditor; }

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

    private:
        struct BooruMaxRatingModelColumns : public Gtk::TreeModelColumnRecord
        {
            BooruMaxRatingModelColumns() { add(text_column); }
            Gtk::TreeModelColumn<std::string> text_column;
        };

        SiteEditor* m_SiteEditor;
        KeybindingEditor* m_KeybindingEditor;

        const std::map<std::string, sigc::signal<void>> m_SpinSignals;
        sigc::signal<void> m_SignalBGColorSet, m_SignalTitleFormatChanged;
    };
}

#endif /* _PREFERENCES_H_ */
