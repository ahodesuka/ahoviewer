#include <glibmm/i18n.h>
#include <iostream>

#include "page.h"
using namespace AhoViewer::Booru;

#include "curler.h"
#include "image.h"
#include "settings.h"

Page::Page(Gtk::Menu *menu)
  : Gtk::ScrolledWindow(),
    m_PopupMenu(menu),
    m_ImageFetcher(std::unique_ptr<ImageFetcher>(new ImageFetcher())),
    m_IconView(Gtk::manage(new Gtk::IconView())),
    m_Tab(Gtk::manage(new Gtk::EventBox())),
    m_TabIcon(Gtk::manage(new Gtk::Image(Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU))),
    m_TabLabel(Gtk::manage(new Gtk::Label(_("New Tab")))),
    m_TabButton(Gtk::manage(new Gtk::Button())),
    m_ImageList(std::make_shared<ImageList>(this)),
    m_Page(0),
    m_NumPosts(0),
    m_LastPage(false),
    m_Saving(false),
    m_SaveCancel(Gio::Cancellable::create()),
    m_GetPostsThread(nullptr),
    m_SaveImagesThread(nullptr)
{
    set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    set_shadow_type(Gtk::SHADOW_ETCHED_IN);

    // Create page tab {{{
    Glib::RefPtr<Gtk::RcStyle> style = m_TabButton->get_modifier_style();
    style->set_ythickness(0);
    style->set_xthickness(0);

    m_TabButton->modify_style(style);
    m_TabButton->add(*(Gtk::manage(new Gtk::Image(Gtk::Stock::CLOSE, Gtk::ICON_SIZE_MENU))));
    m_TabButton->property_relief() = Gtk::RELIEF_NONE;
    m_TabButton->set_focus_on_click(false);
    m_TabButton->set_tooltip_text(_("Close Tab"));
    m_TabButton->signal_clicked().connect([ this ]() { m_SignalClosed(this); });

    m_TabLabel->set_alignment(0.0, 0.5);
    m_TabLabel->set_ellipsize(Pango::ELLIPSIZE_END);

    Gtk::HBox *hbox = Gtk::manage(new Gtk::HBox());
    hbox->pack_start(*m_TabIcon, false, false);
    hbox->pack_start(*m_TabLabel, true, true, 2);
    hbox->pack_start(*m_TabButton, false, false);
    hbox->show_all();

    m_Tab->add(*hbox);
    m_Tab->signal_button_press_event().connect(sigc::mem_fun(*this, &Page::on_tab_button_press_event));
    // }}}

    get_vadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &Page::on_value_changed));

    ModelColumns columns;
    m_ListStore = Gtk::ListStore::create(columns);
    m_IconView->set_model(m_ListStore);
    m_IconView->set_selection_mode(Gtk::SELECTION_BROWSE);
    m_IconView->signal_selection_changed().connect(sigc::mem_fun(*this, &Page::on_selection_changed));
    m_IconView->signal_button_press_event().connect(sigc::mem_fun(*this, &Page::on_button_press_event));

    // Workaround to have fully centered pixbufs
    Gtk::CellRendererPixbuf *cell = Gtk::manage(new Gtk::CellRendererPixbuf());
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(m_IconView->gobj()),
                               GTK_CELL_RENDERER(cell->gobj()), TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(m_IconView->gobj()),
                                  GTK_CELL_RENDERER(cell->gobj()), "pixbuf", 0);

    m_SignalPostsDownloaded.connect(sigc::mem_fun(*this, &Page::on_posts_downloaded));
    m_SignalSaveProgressDisp.connect([ this ]()
    {
        m_SignalSaveProgress(m_SaveImagesCurrent, m_SaveImagesTotal);
    });

    add(*m_IconView);
    show_all();
}

Page::~Page()
{
    m_Curler.cancel();
    m_CountsCurler.cancel();

    if (m_GetPostsThread)
    {
        m_GetPostsThread->join();
        m_GetPostsThread = nullptr;
    }

    cancel_save();
}

void Page::set_selected(const size_t index)
{
    get_window()->freeze_updates();

    Gtk::TreePath path(std::to_string(index));
    m_IconView->select_path(path);
    gtk_icon_view_set_cursor(m_IconView->gobj(), path.gobj(), NULL, FALSE);

    get_window()->thaw_updates();
}

void Page::search(const std::shared_ptr<Site> &site)
{
    if (!ask_cancel_save())
        return;

    m_Curler.cancel();
    m_CountsCurler.cancel();

    cancel_save();
    m_ImageList->clear();

    m_Site = site;
    m_Page = 1;
    m_LastPage = false;

    std::string tags = m_Tags;

    // Trim whitespace for the tab label text
    if (!tags.empty())
    {
        size_t f = tags.find_first_not_of(' '),
               l = tags.find_last_not_of(' ');
        tags = f == std::string::npos ? "" : " - " + tags.substr(f, l - f + 1);
    }

    m_TabLabel->set_text(site->get_name() + tags);
    m_TabIcon->set(site->get_icon_pixbuf());

    get_posts();
}

void Page::save_image(const std::string &path, const std::shared_ptr<Image> &img)
{
    m_SaveCancel->reset();

    m_Saving            = true;
    m_SaveImagesThread  = Glib::Threads::Thread::create([ this, path, img ]()
    {
        img->save(path);
        m_Saving = false;
    });
}

void Page::save_images(const std::string &path)
{
    m_SaveCancel->reset();

    m_Saving            = true;
    m_SaveImagesCurrent = 0;
    m_SaveImagesTotal   = m_ImageList->get_vector_size();
    m_SaveImagesThread  = Glib::Threads::Thread::create([ this, path ]()
    {
        // 2 if cachesize is 0, 8 if cachesize > 2
        Glib::ThreadPool pool(std::max(std::min(Settings.get_int("CacheSize") * 4, 8), 2));
        for (const std::shared_ptr<AhoViewer::Image> &img : m_ImageList->get_images())
        {
            pool.push([ this, path, img ]()
            {
                if (m_SaveCancel->is_cancelled())
                    return;

                std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(img);
                bimage->save(Glib::build_filename(path, Glib::path_get_basename(bimage->get_filename())));
                ++m_SaveImagesCurrent;

                if (!m_SaveCancel->is_cancelled())
                    m_SignalSaveProgressDisp();
            });
        }

        pool.shutdown(m_SaveCancel->is_cancelled());
        m_Saving = false;
    });
}

/**
 * Returns true if we want to cancel or we're not saving
 **/
bool Page::ask_cancel_save()
{
    if (!m_Saving)
        return true;

    Gtk::Window *window = static_cast<Gtk::Window*>(get_toplevel());
    Gtk::MessageDialog dialog(*window, _("Are you sure that you want to stop saving images?"),
                              false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
    dialog.set_secondary_text(_("Closing this tab will stop the save operation."));

    return dialog.run() == Gtk::RESPONSE_YES;
}

void Page::cancel_save()
{
    m_SaveCancel->cancel();

    for (const std::shared_ptr<AhoViewer::Image> &img : m_ImageList->get_images())
    {
        std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(img);
        bimage->cancel_download();
    }

    if (m_SaveImagesThread)
    {
        m_SaveImagesThread->join();
        m_SaveImagesThread = nullptr;
    }
}

void Page::get_posts()
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

    if (m_GetPostsThread)
        m_GetPostsThread->join();

    m_Curler.set_url(m_Site->get_posts_url(tags, m_Page));

    m_GetPostsThread = Glib::Threads::Thread::create([ this, tags ]()
    {
        size_t postsCount = 0;
        // Danbooru doesn't give the post count with the posts
        // Get it from thier counts api
        if (m_Page == 1 && m_Site->get_type() == Site::Type::DANBOORU)
        {
            m_CountsCurler.set_url(m_Site->get_url() + "/counts/posts.xml?tags=" + tags);
            if (m_CountsCurler.perform())
            {
                xmlDocument doc(reinterpret_cast<char*>(m_CountsCurler.get_data()), m_CountsCurler.get_data_size());
                postsCount = std::stoul(doc.get_children()[0].get_value());
            }
        }

        if (m_Curler.perform())
        {
            m_Posts = std::make_shared<xmlDocument>(
                    reinterpret_cast<char*>(m_Curler.get_data()), m_Curler.get_data_size());
            m_NumPosts = m_Posts->get_n_nodes();

            if (postsCount)
                m_Posts->set_attribute("count", std::to_string(postsCount));
        }
        else
        {
            m_NumPosts = 0;
            std::cerr << "Error while downloading posts on " << m_Curler.get_url() << std::endl
                      << "  " << m_Curler.get_error() << std::endl;
        }

        if (!m_Curler.is_cancelled())
            m_SignalPostsDownloaded();
    });
}

bool Page::get_next_page()
{
    if (m_LastPage || m_GetPostsThread)
        return false;

    if (!m_Saving)
    {
        ++m_Page;
        get_posts();

        return false;
    }
    else if (!m_GetNextPageConn)
    {
        m_GetNextPageConn = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Page::get_next_page), 1000);
    }

    return true;
}

/**
 * Adds the downloaded posts to the image list.
 **/
void Page::on_posts_downloaded()
{
    if (m_Posts->get_attribute("success") == "false" && !m_Posts->get_attribute("reason").empty())
    {
        m_SignalDownloadError(m_Posts->get_attribute("reason"));
    }
    else if (m_NumPosts > 0)
    {
        reserve(m_NumPosts);
        m_ImageList->load(m_Posts, this);
    }
    else if (m_Page == 1)
    {
        m_SignalDownloadError(_("No results found"));
    }

    if (m_NumPosts < static_cast<size_t>(Settings.get_int("BooruLimit")))
        m_LastPage = true;

    m_Posts = nullptr;
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

            if (index + Settings.get_int("CacheSize") >= m_ImageList->get_vector_size() - 1)
                get_next_page();

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

    if (value >= limit)
        get_next_page();
}

bool Page::on_button_press_event(GdkEventButton *e)
{
    if (e->type == GDK_BUTTON_PRESS && e->button == 3)
    {
        Gtk::TreePath path = m_IconView->get_path_at_pos(e->x, e->y);

        if (path)
        {
            m_IconView->select_path(path);
            m_IconView->scroll_to_path(path, false, 0, 0);

            m_PopupMenu->popup(e->button, e->time);

            return true;
        }
    }

    return false;
}

bool Page::on_tab_button_press_event(GdkEventButton *e)
{
    if (e->type == GDK_BUTTON_PRESS && e->button == 2)
        m_SignalClosed(this);

    return false;
}
