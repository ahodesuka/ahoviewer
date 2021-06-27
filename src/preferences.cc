#include "preferences.h"
using namespace AhoViewer;

#include "settings.h"

#include <glibmm/i18n.h>

PreferencesDialog::PreferencesDialog(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::Dialog(cobj),
      m_SpinSignals({
          { "CursorHideDelay", sigc::signal<void>() },
          { "CacheSize", sigc::signal<void>() },
          { "SlideshowDelay", sigc::signal<void>() },
      })
{
    bldr->get_widget_derived("BooruSiteEditor", m_SiteEditor);
    bldr->get_widget_derived("KeybindingEditor", m_KeybindingEditor);
    bldr->get_widget("AskDeleteConfirm", m_AskDeleteConfirm);

    m_AskDeleteConfirm->signal_toggled().connect(
        [&] { m_SignalAskDeleteConfirmChanged(m_AskDeleteConfirm->get_active()); });

    Gtk::Button* close_button{ nullptr };
    bldr->get_widget("PreferencesDialog::CloseButton", close_button);
    Gtk::StackSidebar* stack_sidebar{ nullptr };
    bldr->get_widget("PreferencesDialog::StackSidebar", stack_sidebar);
    stack_sidebar->get_style_context()->remove_class("sidebar");
    auto css{ Gtk::CssProvider::create() };
    css->load_from_data(
        "#PreferencesDialogStackSidebar list{border-right-width:1px;border-right-style:solid;}");
    stack_sidebar->get_style_context()->add_provider_for_screen(
        Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    close_button->signal_clicked().connect([&] { hide(); });

    Gtk::CheckButton* check_button{ nullptr };
    bldr->get_widget("SaveThumbnails", check_button);
#ifdef __linux__
    check_button->show();
#else  // !__linux__
    check_button->hide();
#endif // !__linux__

    bldr->get_widget("BackgroundColor", m_BGColor);
    // This needs to be done on realize because the default color isn't known until the imagebox is
    // realized
    signal_realize().connect([&]() { m_BGColor->set_rgba(Settings.get_background_color()); });
    m_BGColor->signal_color_set().connect([&]() {
        Settings.set_background_color(m_BGColor->get_rgba());
        m_SignalBGColorSet();
    });

    Gtk::Button* reset_bgolor_button{ nullptr };
    bldr->get_widget("ResetBGColorButton", reset_bgolor_button);
    reset_bgolor_button->signal_clicked().connect([&]() {
        Settings.remove("BackgroundColor");
        m_BGColor->set_rgba(Settings.get_background_color());
        m_SignalBGColorSet();
    });

    Gtk::Entry* fmt_entry{ nullptr };
    bldr->get_widget("TitleFormat", fmt_entry);
    fmt_entry->set_text(Settings.get_string("TitleFormat"));
    fmt_entry->signal_changed().connect([&, fmt_entry]() {
        Settings.set("TitleFormat", fmt_entry->get_text());
        m_SignalTitleFormatChanged();
    });

    // {{{ Check Buttons
    std::vector<std::string> check_settings = {
        "StartFullscreen", "HideAllFullscreen", "RememberWindowSize",   "RememberWindowPos",
        "SmartNavigation", "AutoOpenArchive",   "RememberLastFile",     "StoreRecentFiles",
        "SaveThumbnails",  "AskDeleteConfirm",  "RememberLastSavePath", "SaveImageTags",
    };

    for (const std::string& s : check_settings)
    {
        bldr->get_widget(s, check_button);
        check_button->set_active(Settings.get_bool(s));
        check_button->signal_toggled().connect(
            [s, check_button]() { Settings.set(s, check_button->get_active()); });
    }
    // }}}

    // {{{ Spin Buttons
    std::vector<std::string> spin_settings = {
        "CursorHideDelay",
        "CacheSize",
        "SlideshowDelay",
        "BooruLimit",
    };
    Gtk::SpinButton* spin_button{ nullptr };

    for (const std::string& s : spin_settings)
    {
        bldr->get_widget(s, spin_button);
        spin_button->set_value(Settings.get_int(s));
        spin_button->signal_value_changed().connect([&, s, spin_button]() {
            Settings.set(s, spin_button->get_value_as_int());

            if (m_SpinSignals.find(s) != m_SpinSignals.end())
                m_SpinSignals.at(s).emit();
        });
    }
    // }}}

    Gtk::ComboBox* combo_box{ nullptr };
    bldr->get_widget("BooruMaxRating", combo_box);
    BooruMaxRatingModelColumns columns;
    Glib::RefPtr<Gtk::ListStore> combo_model = Gtk::ListStore::create(columns);

    std::vector<std::string> ratings = {
        _("Safe"),
        _("Questionable"),
        _("Explicit"),
    };

    for (const std::string& rating : ratings)
        combo_model->append()->set_value(0, rating);

    combo_box->pack_start(columns.text_column);
    combo_box->set_model(combo_model);
    combo_box->set_active(static_cast<int>(Settings.get_booru_max_rating()));
    combo_box->signal_changed().connect([combo_box]() {
        Settings.set_booru_max_rating(
            static_cast<Booru::Rating>(combo_box->get_active_row_number()));
    });
}
