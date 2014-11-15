#include "page.h"
using namespace AhoViewer::Booru;

#include "browser.h"
#include "curler.h"
#include "image.h"
#include "settings.h"

#include <glibmm/i18n.h>
#include <iostream>

Page::Page(Browser *browser)
  : Gtk::ScrolledWindow(),
    m_Browser(browser),
    m_IconView(Gtk::manage(new Gtk::IconView())),
    m_Tab(Gtk::manage(new Gtk::HBox())),
    m_TabIcon(Gtk::manage(new Gtk::Image(Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU))),
    m_TabLabel(Gtk::manage(new Gtk::Label(_("New Tab")))),
    m_TabButton(Gtk::manage(new Gtk::Button())),
    m_GetPostsThread(nullptr),
    m_SignalClosed()
{
    set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    set_shadow_type(Gtk::SHADOW_ETCHED_IN);

    // Create page tab {{{
    GtkRcStyle *style = gtk_rc_style_new();
    style->xthickness = style->ythickness = 0;
    gtk_widget_modify_style((GtkWidget*)m_TabButton->gobj(), style);
    g_object_unref(G_OBJECT(style));

    m_TabButton->add(*(Gtk::manage(new Gtk::Image(Gtk::Stock::CLOSE, Gtk::ICON_SIZE_MENU))));
    m_TabButton->property_relief() = Gtk::RELIEF_NONE;
    m_TabButton->set_focus_on_click(false);
    m_TabButton->set_tooltip_text(_("Close Tab"));
    m_TabButton->signal_clicked().connect([ this ]() { m_SignalClosed(this); });

    m_TabLabel->set_alignment(0.0, 0.5);
    m_TabLabel->set_ellipsize(Pango::ELLIPSIZE_END);

    m_Tab->pack_start(*m_TabIcon, false, false);
    m_Tab->pack_start(*m_TabLabel, true, true, 2);
    m_Tab->pack_start(*m_TabButton, false, false);
    m_Tab->show_all();
    // }}}

    get_vadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &Page::on_value_changed));

    ModelColumns columns;
    m_ListStore = Gtk::ListStore::create(columns);
    m_IconView->set_model(m_ListStore);
    m_IconView->set_selection_mode(Gtk::SELECTION_BROWSE);
    m_IconView->signal_selection_changed().connect(sigc::mem_fun(*this, &Page::on_selection_changed));

    // Workaround to have fully centered pixbufs
    Gtk::CellRendererPixbuf *cell = Gtk::manage(new Gtk::CellRendererPixbuf());
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(m_IconView->gobj()),
            (GtkCellRenderer*)cell->gobj(), TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(m_IconView->gobj()),
            (GtkCellRenderer*)cell->gobj(), "pixbuf", 0);

    m_ImageList = std::make_shared<ImageList>(this);
    m_SignalPostsDownloaded.connect(sigc::mem_fun(*this, &Page::on_posts_downloaded));

    add(*m_IconView);
    show_all();
}

Page::~Page()
{
    if (m_GetPostsThread)
    {
        m_GetPostsThread->join();
        m_GetPostsThread = nullptr;
    }
}

void Page::set_selected(const size_t index)
{
    get_window()->freeze_updates();

    Gtk::TreePath path(std::to_string(index));
    m_IconView->select_path(path);
    m_IconView->scroll_to_path(path, false, 0, 0);
    gtk_icon_view_set_cursor(m_IconView->gobj(), path.gobj(), NULL, FALSE);

    get_window()->thaw_updates();
}

void Page::search(std::shared_ptr<Site> site, const std::string &tags)
{
    m_ImageList->clear();

    m_Site = site;
    m_Tags = tags;
    m_Page = 1;
    m_LastPage = false;

    m_TabLabel->set_text(site->get_name() + (!tags.empty() ? " - " + tags : ""));
    m_TabIcon->set(site->get_icon_pixbuf());

    get_posts();
}

ImageFetcher* Page::get_image_fetcher() const
{
    return m_Browser->get_image_fetcher();
}

void Page::get_posts()
{
    m_GetPostsThread = Glib::Threads::Thread::create([ this ]()
    {
        std::string tags(m_Tags);

        if (m_Tags.find("rating:") == std::string::npos)
        {
            if (Settings.get_booru_max_rating() == Site::Rating::SAFE)
            {
                tags += " rating:safe";
            }
            else if (Settings.get_booru_max_rating() == Site::Rating::QUESTIONABLE)
            {
                tags += " -rating:explicit";
            }
        }

        m_Posts = m_Site->download_posts(tags, m_Page);
        m_NumPosts = std::distance(m_Posts.begin(), m_Posts.end());
        m_SignalPostsDownloaded();
    });
}

/**
 * Adds the downloaded posts to the image list.
 **/
void Page::on_posts_downloaded()
{
    if (m_NumPosts > 0)
    {
        reserve(m_NumPosts);
        m_ImageList->load(m_Posts, this);
    }
    else if (m_Page == 1)
    {
        m_SignalNoResults();
    }

    if (m_NumPosts < (size_t)Settings.get_int("BooruLimit"))
        m_LastPage = true;

    m_GetPostsThread->join();
    m_GetPostsThread = nullptr;
}

void Page::on_selection_changed()
{
    std::vector<Gtk::TreePath> paths =
        static_cast<std::vector<Gtk::TreePath>>(m_IconView->get_selected_items());

    if (!paths.empty())
    {
        Gtk::TreePath path = paths[0];
        if (path)
        {
            size_t index = path[0];

            if (index + Settings.get_int("CacheSize") >= m_ImageList->get_size() - 1 &&
                !m_LastPage && !m_GetPostsThread)
            {
                ++m_Page;
                get_posts();
            }

            m_SignalSelectedChanged(index);
        }
    }
}

/**
 * Vertical scrollbar value changed
 **/
void Page::on_value_changed()
{
    double value = get_vadjustment()->get_value(),
           limit = get_vadjustment()->get_upper() -
                   get_vadjustment()->get_page_size() -
                   get_vadjustment()->get_step_increment();

    if (value >= limit && !m_LastPage && !m_GetPostsThread)
    {
        ++m_Page;
        get_posts();
    }
}
