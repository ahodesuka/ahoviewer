#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <gtkmm.h>

#include "siteeditor.h"

namespace AhoViewer
{
    class PreferencesDialog : public Gtk::Dialog
    {
    public:
        typedef sigc::signal<void> SignalBGColorSetType;

        PreferencesDialog(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~PreferencesDialog();

        SiteEditor* get_site_editor() const { return m_SiteEditor; }
        SignalBGColorSetType signal_bg_color_set() const { return m_SignalBGColorSet; }
    private:
        struct BooruMaxRatingModelColumns : public Gtk::TreeModelColumnRecord
        {
            BooruMaxRatingModelColumns() { add(text_column); }
            Gtk::TreeModelColumn<std::string> text_column;
        };

        SiteEditor *m_SiteEditor;

        SignalBGColorSetType m_SignalBGColorSet;
    };
}

#endif /* _PREFERENCES_H_ */
