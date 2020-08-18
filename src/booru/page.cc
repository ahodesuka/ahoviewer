#include "page.h"

#include <glibmm/i18n.h>
#include <iostream>
using namespace AhoViewer::Booru;

#include "curler.h"
#include "image.h"
#include "settings.h"
#include "site.h"
#include "threadpool.h"

#define RETRY_COUNT 5

void Page::CellRendererThumbnail::get_preferred_width_vfunc(Gtk::Widget& widget,
                                                            int& minimum_width,
                                                            int& natural_width) const
{
    auto width{ widget.get_width() / (widget.get_width() / Image::BooruThumbnailSize) };
    minimum_width = width - IconViewItemPadding * 2; // Item padding is applied to right and left
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
    Gtk::TreePath path(std::to_string(index));
    m_IconView->select_path(path);
    scroll_to_selected();
}

void Page::scroll_to_selected()
{
    std::vector<Gtk::TreePath> paths = m_IconView->get_selected_items();

    if (!paths.empty())
    {
        Gtk::TreePath path = paths[0];
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
        size_t f = m_SearchTags.find_first_not_of(' '), l = m_SearchTags.find_last_not_of(' ');
        m_SearchTags = f == std::string::npos ? "*" : m_SearchTags.substr(f, l - f + 1);
    }
    else
    {
        if (site->get_type() != Type::MOEBOORU)
            m_SearchTags = "*";
    }
    std::string label = m_Site->get_name() +
                        (m_SearchTags == "*" || m_SearchTags.empty() ? "" : " - " + m_SearchTags);
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
        std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(img);
        bimage->cancel_download();
    }

    if (m_SaveImagesThread.joinable())
        m_SaveImagesThread.join();

    m_Saving = false;
}

void Page::get_posts()
{
    std::string tags = m_SearchTags;
    m_KeepAligned    = true;

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
        size_t posts_count = 0;
        // Danbooru doesn't give the post count with the posts
        // Get it from thier counts api
        if (m_Page == 1 &&
            (m_Site->get_type() == Type::DANBOORU || m_Site->get_type() == Type::DANBOORU_V2))
        {
            m_Curler.set_url(m_Site->get_url() + "/counts/posts.xml?tags=" + tags);
            if (m_Curler.perform())
            {
                try
                {
                    xml::Document doc(reinterpret_cast<char*>(m_Curler.get_data()),
                                      m_Curler.get_data_size());
                    posts_count = std::stoul(doc.get_children()[0].get_value());
                }
                catch (const std::runtime_error& e)
                {
                }
                catch (const std::invalid_argument& e)
                {
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

        bool success       = false;
        size_t retry_count = 0;
        do
        {
            success = m_Curler.perform();
            if (success)
            {
                try
                {
                    m_Posts = std::make_unique<xml::Document>(
                        reinterpret_cast<char*>(m_Curler.get_data()), m_Curler.get_data_size());
                }
                catch (const std::runtime_error& e)
                {
                    std::cerr << e.what() << std::endl;
                    m_Posts = nullptr;
                }

                // XXX: Ocassionally Danbooru returns a 500 internal server error
                // "uninitialized constant LegacyController::Builder"
                // and sets success to false
                // XXX: Does this still happen? and does it happen with _V2?
                if (m_Posts && m_Site->get_type() == Type::DANBOORU)
                    success = m_Posts->get_attribute("success") != "false";
            }
        } while (!m_Curler.is_cancelled() && !success && ++retry_count < RETRY_COUNT);

        if (success && m_Posts && posts_count)
            m_Posts->set_attribute("count", std::to_string(posts_count));
        else if (!success && !m_Curler.is_cancelled())
            std::cerr << "Error while downloading posts on " << m_Curler.get_url() << std::endl
                      << "  " << m_Curler.get_error() << std::endl;

        m_Curler.clear();
        if (!m_Curler.is_cancelled())
        {
            // Get Tag types, only works on gelbooru.com not other gelbooru based sites
            if (success && m_Posts && m_Site->get_url().find("gelbooru.com") != std::string::npos)
                get_posts_tags();

            m_SignalPostsDownloaded();
        }
    });
}

void Page::get_posts_tags()
{
    std::string tags;
    {
        std::set<std::string> all_tags;

        for (const auto& post : m_Posts->get_children())
        {
            std::istringstream ss{ post.get_attribute("tags") };
            std::set<std::string> tags{ std::istream_iterator<std::string>{ ss },
                                        std::istream_iterator<std::string>{} };
            all_tags.insert(tags.begin(), tags.end());
        }

        std::ostringstream oss;
        std::copy(all_tags.begin(), all_tags.end(), std::ostream_iterator<std::string>(oss, " "));
        tags = oss.str();
    }

    if (tags.empty())
        return;

    // The above will leavea a trailing space, remove it
    tags.erase(tags.find_last_of(' '));
    tags = m_Curler.escape(tags);

    // We need to split the tags into multiple requests if they are larger than 5K bytes because
    // Gelbooru has a max request header size of 6K bytes, generally the other data in the header
    // should not exceed 1K bytes (but this may have to be looked at)
    const int max_query_size{ 5120 };
    static const std::string space{ "%20" };

    size_t splits_needed{ tags.length() / max_query_size };
    std::vector<std::string> split_tags;
    std::string::iterator it, last_it{ tags.begin() };

    for (size_t i{ 0 }; i < splits_needed; ++i)
    {
        // Find the last encoded space before max_query_size * (i+1)
        it = std::find_end(
            last_it, tags.begin() + max_query_size * (i + 1), space.begin(), space.end());
        split_tags.push_back(
            tags.substr(std::distance(tags.begin(), last_it), std::distance(last_it, it)));
        // Advance past the space so the next string wont start with it
        last_it = it + space.length();
    }
    // Add any remaining tags or all the tags if tags.length() < max_query_size
    split_tags.push_back(tags.substr(std::distance(tags.begin(), last_it)));
    std::vector<std::future<std::vector<Tag>>> jobs;

    static const auto tag_task = [](const std::shared_ptr<Site>& site, const std::string& t) {
        static const std::string tag_uri{ "/index.php?page=dapi&s=tag&q=index&limit=0&names=%1" };
        static const std::map<int, Tag::Type> gelbooru_type_lookup{ {
            { 0, Tag::Type::GENERAL },
            { 1, Tag::Type::ARTIST },
            { 3, Tag::Type::COPYRIGHT },
            { 4, Tag::Type::CHARACTER },
            { 5, Tag::Type::METADATA },
            { 6, Tag::Type::DEPRECATED },
        } };
        Curler c{ site->get_url() + Glib::ustring::compose(tag_uri, t) };
        std::vector<Tag> tags;
        if (c.perform())
        {
            try
            {
                xml::Document xml{ reinterpret_cast<char*>(c.get_data()), c.get_data_size() };
                auto nodes{ xml.get_children() };
                std::transform(
                    nodes.begin(), nodes.end(), std::back_inserter(tags), [](const auto& n) {
                        Tag::Type type;
                        auto i{ std::stoi(n.get_attribute("type")) };
                        if (gelbooru_type_lookup.find(i) != gelbooru_type_lookup.end())
                            type = gelbooru_type_lookup.at(i);
                        else
                            type = Tag::Type::UNKNOWN;
                        return Tag(n.get_attribute("name"), type);
                    });
            }
            catch (const std::runtime_error& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }
        return tags;
    };

    for (const auto& t : split_tags)
        jobs.push_back(std::async(std::launch::async, tag_task, m_Site, t));

    // Wait for all the jobs to finish and combine all the tags
    for (auto& job : jobs)
    {
        auto tags{ job.get() };
        m_PostsTags.insert(m_PostsTags.end(), tags.begin(), tags.end());
    }

    // Does this speed the lookup up?
    std::sort(m_PostsTags.begin(), m_PostsTags.end());
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
    if (m_Posts && m_Posts->get_attribute("success") == "false" &&
        (!m_Posts->get_attribute("reason").empty() || !m_Posts->get_value().empty()))
    {
        auto reason =
            m_Posts->get_value().empty() ? m_Posts->get_attribute("reason") : m_Posts->get_value();
        m_SignalDownloadError(reason);
    }
    else if (m_Posts)
    {
        auto n_posts = m_Posts->get_n_nodes();
        if (n_posts > 0)
        {
            auto size_before = m_ImageList->get_vector_size();
            m_ImageList->load(*m_Posts, *this, m_PostsTags);
            // Number of posts that actually got added to the image list
            // ie supported file types
            n_posts = m_ImageList->get_vector_size() - size_before;
        }

        if (n_posts == 0)
        {
            if (m_Page == 1)
                m_SignalDownloadError(_("No results found"));

            m_LastPage = true;
        }
    }
    // 401 = Unauthorized
    else if (m_Curler.get_response_code() == 401)
    {
        std::string e = Glib::ustring::compose(
            _("Failed to login as %1 on %2"), m_Site->get_username(), m_Site->get_name());
        m_SignalDownloadError(e);
    }
    // No network connection?
    else
    {
        std::string e =
            Glib::ustring::compose(_("Failed to download posts on %1"), m_Site->get_name());
        m_SignalDownloadError(e);
    }

    m_Posts = nullptr;
    m_PostsTags.clear();
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
