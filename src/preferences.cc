#include <glibmm/i18n.h>

#include "preferences.h"
using namespace AhoViewer;

#include "settings.h"

PreferencesDialog::PreferencesDialog(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::Dialog(cobj),
    m_SpinSignals(
    {
        { "CursorHideDelay",  sigc::signal<void>() },
        { "CacheSize",        sigc::signal<void>() },
        { "SlideshowDelay",   sigc::signal<void>() },
    })
{
    bldr->get_widget_derived("BooruSiteEditor",  m_SiteEditor);
    bldr->get_widget_derived("KeybindingEditor", m_KeybindingEditor);

    Gtk::Button *closeButton = nullptr;
    bldr->get_widget("PreferencesDialog::CloseButton", closeButton);

    closeButton->signal_clicked().connect([ this ]() { hide(); });

    Gtk::CheckButton *checkButton = nullptr;
    bldr->get_widget("SaveThumbnails", checkButton);
#ifdef __linux__
    checkButton->show();
#else
    checkButton->hide();
#endif // __linux__

    Gtk::ColorButton *bgColor = nullptr;
    bldr->get_widget("BackgroundColor", bgColor);
    bgColor->set_color(Settings.get_background_color());
    bgColor->signal_color_set().connect([ this, bgColor ]()
    {
        Settings.set_background_color(bgColor->get_color());
        m_SignalBGColorSet();
    });

    Gtk::Entry *fmtEntry = nullptr;
    bldr->get_widget("TitleFormat", fmtEntry);
    fmtEntry->set_text(Settings.get_string("TitleFormat"));
    fmtEntry->signal_changed().connect([=]()
    {
        Settings.set("TitleFormat", fmtEntry->get_text());
        m_SignalTitleFormatChanged();
    });

    // {{{ Check Buttons
    std::vector<std::string> checkSettings =
    {
        "StartFullscreen",
        "HideAllFullscreen",
        "RememberWindowSize",
        "RememberWindowPos",
        "SmartNavigation",
        "AutoOpenArchive",
        "RememberLastFile",
        "StoreRecentFiles",
        "SaveThumbnails",
        "RememberLastSavePath",
    };

    for (const std::string &s : checkSettings)
    {
        bldr->get_widget(s, checkButton);
        checkButton->set_active(Settings.get_bool(s));
        checkButton->signal_toggled().connect([ s, checkButton ]()
                { Settings.set(s, checkButton->get_active()); });
    }
    // }}}

    // {{{ Spin Buttons
    std::vector<std::string> spinSettings =
    {
        "CursorHideDelay",
        "CacheSize",
        "SlideshowDelay",
        "BooruLimit",
    };
    Gtk::SpinButton *spinButton = nullptr;

    for (const std::string &s : spinSettings)
    {
        bldr->get_widget(s, spinButton);
        spinButton->set_value(Settings.get_int(s));
        spinButton->signal_value_changed().connect([ this, s, spinButton ]()
        {
            Settings.set(s, spinButton->get_value_as_int());

            if (m_SpinSignals.find(s) != m_SpinSignals.end())
                m_SpinSignals.at(s).emit();
        });

    }
    // }}}

    Gtk::ComboBox *comboBox = nullptr;
    bldr->get_widget("BooruMaxRating", comboBox);
    BooruMaxRatingModelColumns columns;
    Glib::RefPtr<Gtk::ListStore> comboModel = Gtk::ListStore::create(columns);

    std::vector<std::string> ratings =
    {
        _("Safe"),
        _("Questionable"),
        _("Explicit"),
    };

    for (const std::string &rating : ratings)
        comboModel->append()->set_value(0, rating);

    comboBox->pack_start(columns.text_column);
    comboBox->set_model(comboModel);
    comboBox->set_active(static_cast<int>(Settings.get_booru_max_rating()));
    comboBox->signal_changed().connect([ comboBox ]()
            { Settings.set_booru_max_rating(static_cast<Booru::Site::Rating>(comboBox->get_active_row_number())); });
}
