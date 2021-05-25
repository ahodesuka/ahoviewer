#include "page.h"
using namespace AhoViewer::Booru;

#include "curler.h"
#include "image.h"
#include "settings.h"
#include "site.h"
#include "threadpool.h"

#include <glibmm/i18n.h>
#include <iostream>

#define RETRY_COUNT 5

void Page::CellRendererThumbnail::get_preferred_width_vfunc(Gtk::Widget& widget,
                                                            int& minimum_width,
                                                            int& natural_width) const
{
    // Item padding is applied to right and left
    auto width{ widget.get_width() /
                (widget.get_width() / (Image::BooruThumbnailSize + IconViewItemPadding * 2)) };
    minimum_width = width - IconViewItemPadding * 2;
    natural_width = width - IconViewItemPadding * 2;
}

Page::Page()
    : Gtk::ScrolledWindow{},
      m_PopupMenu{ nullptr },
      m_IconView{ Gtk::make_managed<IconView>() },
      m_Tab{ Gtk::make_managed<Gtk::EventBox>() },
      m_TabIcon{ Gtk::make_managed<Gtk::Image>(Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU) },
      m_TabLabel{ Gtk::make_managed<Gtk::Label>(_("New Tab")) },
      m_MenuLabel{ Gtk::make_managed<Gtk::Label>(_("New Tab")) },
      m_TabButton{ Gtk::make_managed<Gtk::Button>() },
      m_ImageList{ std::make_shared<ImageList>(this) },
      m_SaveCancel{ Gio::Cancellable::create() }
{
    set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    set_shadow_type(Gtk::SHADOW_ETCHED_IN);

    // Create page tab {{{
    m_TabButton->add(*(Gtk::make_managed<Gtk::Image>(Gtk::Stock::CLOSE, Gtk::ICON_SIZE_MENU)));
    m_TabButton->property_relief() = Gtk::RELIEF_NONE;
    m_TabButton->set_focus_on_click(false);
    m_TabButton->set_tooltip_text(_("Close Tab"));
    m_TabButton->signal_clicked().connect([&]() { m_SignalClosed(this); });

    m_TabLabel->set_alignment(0.0, 0.5);
    m_TabLabel->set_ellipsize(Pango::ELLIPSIZE_END);

    m_MenuLabel->set_alignment(0.0, 0.5);
    m_MenuLabel->set_ellipsize(Pango::ELLIPSIZE_END);

    auto* hbox{ Gtk::make_managed<Gtk::HBox>() };
    hbox->pack_start(*m_TabIcon, false, false);
    hbox->pack_start(*m_TabLabel, true, true, 2);
    hbox->pack_start(*m_TabButton, false, false);
    hbox->show_all();

    m_Tab->set_visible_window(false);
    m_Tab->add(*hbox);
    m_Tab->signal_button_release_event().connect(
        sigc::mem_fun(*this, &Page::on_tab_button_release_event));
    // }}}

    m_ScrollConn = get_vadjustment()->signal_value_changed().connect(
        sigc::mem_fun(*this, &Page::on_value_changed));

    m_IconView->set_column_spacing(0);
    m_IconView->set_row_spacing(0);
    m_IconView->set_margin(0);
    m_IconView->set_item_padding(IconViewItemPadding);

    m_IconView->set_model(m_ListStore);
    m_IconView->set_selection_mode(Gtk::SELECTION_BROWSE);
    m_IconView->signal_selection_changed().connect(
        sigc::mem_fun(*this, &Page::on_selection_changed));
    m_IconView->signal_button_press_event().connect(
        sigc::mem_fun(*this, &Page::on_button_press_event));

    // Workaround to have fully centered pixbufs
    auto* cell{ Gtk::make_managed<CellRendererThumbnail>() };
    m_IconView->pack_start(*cell);
    m_IconView->add_attribute(cell->property_pixbuf(), m_Columns.pixbuf);

    m_SignalPostsDownloaded.connect(sigc::mem_fun(*this, &Page::on_posts_downloaded));
    m_SignalSaveProgressDisp.connect([&]() { m_SignalSaveProgress(this); });

    // Check if next page should be loaded after current finishes
    m_ImageList->signal_thumbnails_loaded().connect([&]() {
        on_selection_changed();
        if (!m_KeepAligned)
            on_value_changed();
    });

    add(*m_IconView);
    show_all();
}

Page::~Page()
{
    m_Curler.cancel();

    if (m_GetPostsThread.joinable())
        m_GetPostsThread.join();

    cancel_save();

    m_ListStore->clear();
}

void Page::set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
    m_ScrollConn.block();
    ImageList::Widget::set_pixbuf(index, pixbuf);
    while (Glib::MainContext::get_default()->pending())
        Glib::MainContext::get_default()->iteration(true);
    m_ScrollConn.unblock();

    // Only keep the thumbnail aligned if the user has not scrolled
    // and the thumbnail is most likely still being loaded
    if (m_KeepAligned && m_ImageList->get_index() >= index)
        scroll_to_selected();
}

void Page::set_selected(const size_t index)
{
    Gtk::TreePath path{ std::to_string(index) };
    m_IconView->select_path(path);
    scroll_to_selected();
}

void Page::scroll_to_selected()
{
    std::vector<Gtk::TreePath> paths{ m_IconView->get_selected_items() };

    if (!paths.empty())
    {
        Gtk::TreePath path{ paths[0] };
        if (path)
        {
            m_ScrollConn.block();
            m_IconView->scroll_to_path(path, false, 0, 0);
            m_ScrollConn.unblock();
        }
    }
}

void Page::search(const std::shared_ptr<Site>& site)
{
    if (!ask_cancel_save())
        return;

    m_Curler.cancel();

    cancel_save();
    m_ImageList->clear();

    if (m_GetPostsThread.joinable())
        m_GetPostsThread.join();

    m_Site       = site;
    m_Page       = 1;
    m_LastPage   = false;
    m_SearchTags = m_Tags;

    // Trim leading and trailing whitespace for tab label
    // and m_SearchTags which is used to display tags in other places
    if (!m_SearchTags.empty())
    {
        size_t f{ m_SearchTags.find_first_not_of(' ') }, l{ m_SearchTags.find_last_not_of(' ') };
        m_SearchTags = f == std::string::npos ? "" : m_SearchTags.substr(f, l - f + 1);
    }

    std::string label{ m_Site->get_name() + (m_SearchTags.empty() ? "" : " - " + m_SearchTags) };
    m_TabLabel->set_text(label);
    m_MenuLabel->set_text(label);
    m_TabIcon->set(m_Site->get_icon_pixbuf());

    m_Curler.set_share_handle(m_Site->get_share_handle());
    m_Curler.set_referer(m_Site->get_url());

    get_posts();
}

void Page::save_image(const std::string& path, const std::shared_ptr<Image>& img)
{
    m_SaveCancel->reset();
    if (m_SaveImagesThread.joinable())
        m_SaveImagesThread.join();

    m_Saving           = true;
    m_SaveImagesThread = std::thread([&, path, img]() {
        img->save(path);
        m_Saving = false;
    });
}

void Page::save_images(const std::string& path)
{
    m_SaveCancel->reset();
    if (m_SaveImagesThread.joinable())
        m_SaveImagesThread.join();

    m_Saving            = true;
    m_SaveImagesCurrent = 0;
    m_SaveImagesTotal   = m_ImageList->get_vector_size();
    m_SaveImagesThread  = std::thread([&, path]() {
        ThreadPool pool;
        for (const std::shared_ptr<AhoViewer::Image>& img : *m_ImageList)
        {
            pool.push([&, path, img]() {
                if (m_SaveCancel->is_cancelled())
                    return;

                std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(img);
                bimage->save(
                    Glib::build_filename(path, Glib::path_get_basename(bimage->get_filename())));
                ++m_SaveImagesCurrent;

                if (!m_SaveCancel->is_cancelled())
                    m_SignalSaveProgressDisp();
            });
        }

        m_SignalSaveProgressDisp();
        pool.wait();
        m_Saving = false;
    });
}

// Returns true if we want to cancel or we're not saving
bool Page::ask_cancel_save()
{
    if (!m_Saving)
        return true;

    auto* window = static_cast<Gtk::Window*>(get_toplevel());
    Gtk::MessageDialog dialog(*window,
                              _("Are you sure that you want to stop saving images?"),
                              false,
                              Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_YES_NO,
                              true);
    dialog.set_secondary_text(_("Closing this tab will stop the save operation."));

    return dialog.run() == Gtk::RESPONSE_YES;
}

void Page::cancel_save()
{
    m_SaveCancel->cancel();

    for (const std::shared_ptr<AhoViewer::Image>& img : *m_ImageList)
    {
        std::shared_ptr<Image> bimage{ std::static_pointer_cast<Image>(img) };
        bimage->cancel_download();
    }

    if (m_SaveImagesThread.joinable())
        m_SaveImagesThread.join();

    m_Saving = false;
}

void Page::get_posts()
{
    std::string tags{ m_SearchTags };
    m_KeepAligned = true;

    if (m_Tags.find("rating:") == std::string::npos)
    {
        if (Settings.get_booru_max_rating() == Rating::SAFE)
        {
            tags += " rating:safe";
        }
        else if (Settings.get_booru_max_rating() == Rating::QUESTIONABLE)
        {
            tags += " -rating:explicit";
        }
    }

    tags = m_Curler.escape(tags);

    m_GetPostsThread = std::thread([&, tags]() {
        // DanbooruV2 doesn't give the post count with the posts
        // Get it from thier counts api
        if (m_Page == 1 && m_Site->get_type() == Type::DANBOORU_V2)
        {
            m_Curler.set_url(m_Site->get_url() + "/counts/posts.xml?tags=" + tags);
            if (m_Curler.perform())
            {
                try
                {
                    xml::Document doc{ reinterpret_cast<char*>(m_Curler.get_data()),
                                       m_Curler.get_data_size() };
                    if (doc.get_n_nodes())
                    {
                        std::string posts_count{ doc.get_children()[0].get_value() };
                        // Usually when you use a wildcard operator danbooru's count api will return
                        // a blank value here (blank but contains some whitespace and newlines)
                        if (posts_count.find_first_not_of(" \n\r") != std::string::npos)
                            m_PostsCount = std::stoul(posts_count);
                    }
                }
                catch (const std::runtime_error& e)
                {
                    std::cerr << e.what() << std::endl << m_Curler.get_data() << std::endl;
                }
                catch (const std::invalid_argument& e)
                {
                    std::cerr << e.what() << std::endl
                              << "Failed to parse posts_count" << std::endl;
                }
            }
            else if (m_Curler.is_cancelled())
            {
                return;
            }
        }

        m_Curler.set_url(m_Site->get_posts_url(tags, m_Page));

        if (m_Site->get_type() == Type::GELBOORU)
            m_Curler.set_cookie_file(m_Site->get_cookie());
        else
            m_Curler.set_http_auth(m_Site->get_username(), m_Site->get_password());

        bool success{ false };
        size_t retry_count{ 0 };
        do
        {
            success = m_Curler.perform();
            if (success)
            {
                auto [posts, posts_count, error]{ m_Site->parse_post_data(
                    m_Curler.get_data(), m_Curler.get_data_size()) };
                m_Posts = std::move(posts);
                if (posts_count > 0)
                    m_PostsCount = posts_count;
                m_PostsError = error;
            }
        } while (!m_Curler.is_cancelled() && !success && ++retry_count < RETRY_COUNT);

        if (!success && !m_Curler.is_cancelled())
        {
            std::cerr << "Error while downloading posts on " << m_Curler.get_url() << std::endl
                      << "  " << m_Curler.get_error() << std::endl;

            m_PostsError =
                Glib::ustring::compose(_("Failed to download posts on %1"), m_Site->get_name());
        }

        if (!m_Curler.is_cancelled())
            m_SignalPostsDownloaded();
    });

    m_SignalPostsDownloadStarted();
}

bool Page::get_next_page()
{
    // Do not fetch the next page if this is the last
    // or the current page is still loading
    if (m_LastPage || m_GetPostsThread.joinable() || m_ImageList->is_loading())
        return false;

    if (!m_Saving)
    {
        ++m_Page;
        get_posts();

        return false;
    }
    else if (!m_GetNextPageConn)
    {
        m_GetNextPageConn =
            Glib::signal_timeout().connect(sigc::mem_fun(*this, &Page::get_next_page), 1000);
    }

    return true;
}

// Adds the downloaded posts to the image list.
void Page::on_posts_downloaded()
{
    if (!m_PostsError.empty())
    {
        m_SignalDownloadError(m_PostsError);
    }
    // 401 = Unauthorized
    else if (m_Curler.get_response_code() == 401)
    {
        auto e{ Glib::ustring::compose(
            _("Failed to login as %1 on %2"), m_Site->get_username(), m_Site->get_name()) };
        m_SignalDownloadError(e);
    }
    else
    {
        auto n_posts{ m_Posts.size() };
        if (!m_Posts.empty())
        {
            auto size_before{ m_ImageList->get_vector_size() };
            m_ImageList->load(m_Posts, m_PostsCount);
            // Number of posts that actually got added to the image list
            // ie supported file types
            n_posts = m_ImageList->get_vector_size() - size_before;
        }

        // No posts added to the imagelist
        if (n_posts == 0)
        {
            if (m_Page == 1)
                m_SignalDownloadError(_("No results found"));
            else
                m_SignalOnLastPage();

            m_LastPage = true;
        }
    }

    m_Curler.clear();
    m_Posts.clear();
    m_GetPostsThread.join();
}

void Page::on_selection_changed()
{
    std::vector<Gtk::TreePath> paths{ m_IconView->get_selected_items() };

    if (!paths.empty())
    {
        Gtk::TreePath path{ paths[0] };
        if (path)
        {
            size_t index = path[0];

            if (index + Settings.get_int("CacheSize") >= m_ImageList->get_vector_size() - 1)
                get_next_page();

            m_SignalSelectedChanged(index);
        }
    }
}

// Vertical scrollbar value changed
void Page::on_value_changed()
{
    double value = get_vadjustment()->get_value(),
           limit = get_vadjustment()->get_upper() - get_vadjustment()->get_page_size() -
                   get_vadjustment()->get_step_increment() * 3;
    m_KeepAligned = false;

    if (value >= limit)
        get_next_page();
}

bool Page::on_button_press_event(GdkEventButton* e)
{
    if (e->type == GDK_BUTTON_PRESS && e->button == 3)
    {
        Gtk::TreePath path = m_IconView->get_path_at_pos(e->x, e->y);

        if (path)
        {
            m_IconView->select_path(path);
            m_IconView->scroll_to_path(path, false, 0, 0);

            m_PopupMenu->popup_at_pointer((GdkEvent*)e);

            return true;
        }
    }

    return false;
}

bool Page::on_tab_button_release_event(GdkEventButton* e)
{
    if (e->type == GDK_BUTTON_RELEASE && e->button == 2)
        m_SignalClosed(this);

    return false;
}
