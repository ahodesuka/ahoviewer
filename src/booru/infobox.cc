#include "infobox.h"
using namespace AhoViewer::Booru;

#include "settings.h"

InfoBox::InfoBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::EventBox{ cobj }
{
    auto model{ Glib::RefPtr<Gio::Menu>::cast_dynamic(bldr->get_object("InfoBoxPopoverMenu")) };
    m_PopupMenu = Gtk::make_managed<Gtk::Menu>(model);
    m_PopupMenu->attach_to_widget(*this);

    bldr->get_widget("Booru::Browser::InfoRevealer", m_Revealer);
    bldr->get_widget("Booru::Browser::InfoBox::DateLabel", m_DateLabel);
    bldr->get_widget("Booru::Browser::InfoBox::SourceLabel", m_SourceLabel);
    bldr->get_widget("Booru::Browser::InfoBox::RatingLabel", m_RatingLabel);
    bldr->get_widget("Booru::Browser::InfoBox::ScoreLabel", m_ScoreLabel);

    auto css = Gtk::CssProvider::create();
    css->load_from_resource("/ui/css/booru-info-box.css");
    get_style_context()->add_provider_for_screen(
        Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void InfoBox::set_info(const PostInfo& post_info)
{
    m_InfoSet = true;

    m_DateLabel->set_label(post_info.date);
    if (post_info.source.rfind("http", 0) == 0)
    {
        m_SourceLabel->set_markup(Glib::ustring::compose(
            "<a href=\"%1\">%1</a>", Glib::Markup::escape_text(post_info.source)));
    }
    else
    {
        m_SourceLabel->set_use_markup(false);
        m_SourceLabel->set_label(post_info.source);
    }

    m_RatingLabel->set_label(post_info.rating);
    m_ScoreLabel->set_label(post_info.score);

    if (m_IsVisible || !Settings.get_bool("AutoHideInfoBox"))
        show();
}

void InfoBox::clear()
{
    if (m_InfoSet)
        hide();

    m_InfoSet = false;

    m_DateLabel->set_label("");
    m_SourceLabel->set_label("");
    m_RatingLabel->set_label("");
    m_ScoreLabel->set_label("");
}

void InfoBox::show()
{
    if (m_InfoSet)
    {
        m_HideConn.disconnect();
        m_Revealer->set_reveal_child(true);
    }

    m_IsVisible = true;
}

void InfoBox::hide()
{
    if (Settings.get_bool("AutoHideInfoBox"))
    {
        m_HideConn =
            Glib::signal_timeout().connect(sigc::mem_fun(*this, &InfoBox::timeout_hide), 500);
    }
    else
    {
        m_Revealer->set_reveal_child(false);
    }
    m_IsVisible = false;
}

bool InfoBox::timeout_hide()
{
    if (!m_IsVisible)
        m_Revealer->set_reveal_child(false);
    return false;
}

bool InfoBox::on_button_press_event(GdkEventButton* e)
{
    if (e->button == 3)
    {
        m_PopupMenu->popup_at_pointer(reinterpret_cast<GdkEvent*>(e));
        return false;
    }

    return Gtk::EventBox::on_button_press_event(e);
}
