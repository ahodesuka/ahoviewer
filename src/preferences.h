#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <gtkmm.h>

#include "siteeditor.h"

namespace AhoViewer
{
    class PreferencesDialog : public Gtk::Dialog
    {
    public:
        PreferencesDialog(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~PreferencesDialog() = default;

        SiteEditor* get_site_editor() const { return m_SiteEditor; }
        sigc::signal<void> signal_bg_color_set() const { return m_SignalBGColorSet; }
        sigc::signal<void> signal_cache_size_changed() const { return m_SignalCacheSizeChanged; }
        sigc::signal<void> signal_slideshow_delay_changed() const { return m_SignalSlideshowDelayChanged; }
    private:
        struct BooruMaxRatingModelColumns : public Gtk::TreeModelColumnRecord
        {
            BooruMaxRatingModelColumns() { add(text_column); }
            Gtk::TreeModelColumn<std::string> text_column;
        };

        SiteEditor *m_SiteEditor;

        sigc::signal<void> m_SignalBGColorSet,
                           m_SignalCacheSizeChanged,
                           m_SignalSlideshowDelayChanged;
    };
}

#endif /* _PREFERENCES_H_ */
