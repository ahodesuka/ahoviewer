#include "imagebox.h"
using namespace AhoViewer;

#include "application.h"
#include "imageboxnote.h"
#include "mainwindow.h"
#include "settings.h"
#include "statusbar.h"
#include "videobox.h"

#include <glibmm/i18n.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // ! M_PI

Gdk::RGBA ImageBox::DefaultBGColor = Gdk::RGBA();

ImageBox::ImageBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::ScrolledWindow{ cobj },
      m_HSmoothScroll{ get_hadjustment() },
      m_VSmoothScroll{ get_vadjustment() },
      m_MainWindow{ static_cast<MainWindow*>(get_toplevel()) },
      m_LeftPtrCursor{ Gdk::Cursor::create(Gdk::LEFT_PTR) },
      m_FleurCursor{ Gdk::Cursor::create(Gdk::FLEUR) },
      m_BlankCursor{ Gdk::Cursor::create(Gdk::BLANK_CURSOR) },
      m_ZoomMode{ Settings.get_zoom_mode() },
      m_RestoreScrollPos{ -1, -1, m_ZoomMode }
{
    auto model{ Glib::RefPtr<Gio::Menu>::cast_dynamic(bldr->get_object("ImageBoxPopoverMenu")) };
    // add the recent submenu
    model->insert_submenu(1, _("Open _Recent"), Application::get_default()->get_recent_menu());
    Gtk::Box* box;
    bldr->get_widget("MainWindow::Box", box);
    m_PopoverMenu = Gtk::make_managed<Gtk::Popover>(*box, model);
    static_cast<Gtk::Stack*>(m_PopoverMenu->get_child())->set_hhomogeneous(false);

    bldr->get_widget("ImageBox::Fixed", m_Fixed);
    bldr->get_widget("ImageBox::Overlay", m_Overlay);
    bldr->get_widget("ImageBox::Image", m_GtkImage);
    bldr->get_widget("ImageBox::NoteFixed", m_NoteFixed);
    bldr->get_widget_derived("VideoBox", m_VideoBox);
    bldr->get_widget_derived("StatusBar", m_StatusBar);

    m_StyleUpdatedConn = m_Fixed->signal_style_updated().connect(
        [&]() { m_Fixed->get_style_context()->lookup_color("theme_bg_color", DefaultBGColor); });
}

void ImageBox::queue_draw_image(const bool scroll)
{
    if (!get_realized() || !m_Image || (m_RedrawQueued && !(scroll || m_Loading)))
        return;

    m_DrawConn.disconnect();
    m_RedrawQueued = true;
    m_DrawConn     = Glib::signal_idle().connect(
        sigc::bind_return(sigc::bind(sigc::mem_fun(*this, &ImageBox::draw_image), scroll), false),
        Glib::PRIORITY_HIGH_IDLE);
}

void ImageBox::set_image(const std::shared_ptr<Image>& image)
{
    // very unlikely that image would be null here
    if (!image)
        return;

    if (image != m_Image)
    {
        m_AnimConn.disconnect();
        m_ImageConn.disconnect();
        m_NotesConn.disconnect();
        remove_scroll_callback(m_HSmoothScroll);
        remove_scroll_callback(m_VSmoothScroll);
        reset_slideshow();
        clear_notes();
        m_VideoBox->hide();

        // reset GIF stuff so if it stays cached and is viewed again it will
        // start from the first frame
        if (m_Image)
            m_Image->reset_gif_animation();

        m_Image     = image;
        m_FirstDraw = m_Loading = true;

        m_ImageConn = m_Image->signal_pixbuf_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &ImageBox::queue_draw_image), false));

        // Maybe the notes havent been downloaded yet
        if (m_Image->get_notes().empty())
            m_NotesConn = m_Image->signal_notes_changed().connect(
                sigc::mem_fun(*this, &ImageBox::on_notes_changed));

        queue_draw_image(true);
    }
    else
    {
        queue_draw_image();
    }
}

void ImageBox::clear_image()
{
    m_SlideshowConn.disconnect();
    m_ImageConn.disconnect();
    m_NotesConn.disconnect();
    m_DrawConn.disconnect();
    m_AnimConn.disconnect();
    m_GtkImage->clear();
    m_Overlay->hide();
    m_VideoBox->hide();
    m_Fixed->set_size_request(0, 0);

    remove_scroll_callback(m_HSmoothScroll);
    remove_scroll_callback(m_VSmoothScroll);
    clear_notes();

    m_StatusBar->clear_resolution();
    m_Image = nullptr;
}

void ImageBox::update_background_color()
{
    auto css = Gtk::CssProvider::create();
    css->load_from_data(Glib::ustring::compose("scrolledwindow#ImageBox{background:%1;}",
                                               Settings.get_background_color().to_string()));
    get_style_context()->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void ImageBox::cursor_timeout()
{
    m_CursorConn.disconnect();
    m_Fixed->get_window()->set_cursor(m_LeftPtrCursor);

    if (Settings.get_int("CursorHideDelay") <= 0)
        return;

    m_CursorConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &ImageBox::on_cursor_timeout), Settings.get_int("CursorHideDelay"));
}

void ImageBox::reset_slideshow()
{
    if (m_SlideshowConn)
    {
        m_SlideshowConn.disconnect();
        toggle_slideshow();
    }
}

void ImageBox::toggle_slideshow()
{
    if (!m_SlideshowConn)
    {
        m_SlideshowConn = Glib::signal_timeout().connect_seconds(
            sigc::mem_fun(*this, &ImageBox::advance_slideshow), Settings.get_int("SlideshowDelay"));
    }
    else
    {
        m_SlideshowConn.disconnect();
    }
}

void ImageBox::set_zoom_mode(const ZoomMode mode)
{
    if (mode != m_ZoomMode)
    {
        Settings.set_zoom_mode(mode);
        m_ZoomMode = mode;
        queue_draw_image(true);
    }
}

void ImageBox::on_zoom_in()
{
    zoom(m_ZoomPercent + 10);
}

void ImageBox::on_zoom_out()
{
    zoom(m_ZoomPercent - 10);
}

void ImageBox::on_reset_zoom()
{
    zoom(100);
}

void ImageBox::on_scroll_up()
{
    scroll(0, -300);
}

void ImageBox::on_scroll_down()
{
    scroll(0, 300);
}

void ImageBox::on_scroll_left()
{
    scroll(-300, 0);
}

void ImageBox::on_scroll_right()
{
    scroll(300, 0);
}

void ImageBox::on_realize()
{
    m_NextAction     = m_MainWindow->lookup_action("NextImage");
    m_PreviousAction = m_MainWindow->lookup_action("PreviousImage");

    m_Fixed->get_style_context()->lookup_color("theme_bg_color", DefaultBGColor);
    update_background_color();

    Gtk::ScrolledWindow::on_realize();
}

bool ImageBox::on_button_press_event(GdkEventButton* e)
{
    grab_focus();
    cursor_timeout();

    // Ignore double/triple clicks
    if (e->type == GDK_BUTTON_PRESS)
    {
        switch (e->button)
        {
        case 1:
        case 2:
            m_PressX = m_PreviousX = e->x_root;
            m_PressY = m_PreviousY = e->y_root;
            return true;
        case 3:
        {
            // The popover is attached to the MainWindow::Box, but the event is coming with imagebox
            // coords
            int w, h, ww, wh, mbh = m_MainWindow->get_menubar_height();
            m_MainWindow->get_drawable_area_size(w, h);
            m_MainWindow->get_size(ww, wh);
            Gdk::Rectangle rect{
                static_cast<int>(e->x) + ww - w, static_cast<int>(e->y) + mbh, 1, 1
            };
            m_PopoverMenu->set_pointing_to(rect);
            m_PopoverMenu->popup();
            m_CursorConn.disconnect();
            return true;
        }
        case 8: // Back
            m_PreviousAction->activate();
            return true;
        case 9: // Forward
            m_NextAction->activate();
            return true;
        }
    }

    return Gtk::ScrolledWindow::on_button_press_event(e);
}

bool ImageBox::on_button_release_event(GdkEventButton* e)
{
    if (e->button == 1 || e->button == 2)
    {
        m_Fixed->get_window()->set_cursor(m_LeftPtrCursor);

        if (e->button == 1 && m_PressX == m_PreviousX && m_PressY == m_PreviousY)
        {
            if (Settings.get_bool("SmartNavigation"))
            {
                int w, h;
                m_MainWindow->get_drawable_area_size(w, h);

                if (e->x < w / 2)
                {
                    m_PreviousAction->activate();
                    return true;
                }
            }

            m_NextAction->activate();
        }

        return true;
    }

    return Gtk::ScrolledWindow::on_button_release_event(e);
}

bool ImageBox::on_motion_notify_event(GdkEventMotion* e)
{
    // Left click and middle click dragging = panning
    if (m_Image && ((e->state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK ||
                    (e->state & GDK_BUTTON2_MASK) == GDK_BUTTON2_MASK))
    {
        m_Fixed->get_window()->set_cursor(m_FleurCursor);
        scroll(m_PreviousX - e->x_root, m_PreviousY - e->y_root, true);

        m_PreviousX = e->x_root;
        m_PreviousY = e->y_root;

        return true;
    }
    else
    {
        cursor_timeout();
    }

    return Gtk::ScrolledWindow::on_motion_notify_event(e);
}

// Override default scroll behavior to provide interpolated scrolling
bool ImageBox::on_scroll_event(GdkEventScroll* e)
{
    bool handled{ e->delta_x != 0 || e->delta_y != 0 || e->direction != GDK_SCROLL_SMOOTH };
    grab_focus();
    cursor_timeout();

    // Scroll down
    if (e->delta_y > 0 || e->direction == GDK_SCROLL_DOWN)
    {
        if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
            on_zoom_out();
        else
            scroll(0, 300);
    }
    // Scroll up
    else if (e->delta_y < 0 || e->direction == GDK_SCROLL_UP)
    {
        if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
            on_zoom_in();
        else
            scroll(0, -300);
    }

    // Scroll right
    if (e->delta_x > 0 || e->direction == GDK_SCROLL_RIGHT)
        scroll(300, 0);
    // Scroll left
    else if (e->delta_x < 0 || e->direction == GDK_SCROLL_LEFT)
        scroll(-300, 0);

    return handled || Gtk::ScrolledWindow::on_scroll_event(e);
}

// This must be called after m_Orig(Width/Height) are set
// It sets the values of w, h to their scaled values, and x, y to center
// coordinates for m_Fixed to use
// returns the drawable area's width and height
void ImageBox::get_scale_and_position(int& w, int& h, int& x, int& y)
{
    int ww, wh;
    m_MainWindow->get_drawable_area_size(ww, wh);

    w = m_OrigWidth;
    h = m_OrigHeight;

    double window_aspect = static_cast<double>(ww) / wh, image_aspect = static_cast<double>(w) / h;

    // These do not take the scrollbar size in to account, because I assume that
    // overlay scrollbars are enabled
    if (w > ww && (m_ZoomMode == ZoomMode::FIT_WIDTH ||
                   (m_ZoomMode == ZoomMode::AUTO_FIT && window_aspect <= image_aspect)))
    {
        w = ww;
        h = std::ceil(w / image_aspect);
    }
    else if (h > wh && (m_ZoomMode == ZoomMode::FIT_HEIGHT ||
                        (m_ZoomMode == ZoomMode::AUTO_FIT && window_aspect >= image_aspect)))
    {
        h = wh;
        w = std::ceil(h * image_aspect);
    }
    else if (m_ZoomMode == ZoomMode::MANUAL && m_ZoomPercent != 100)
    {
        w *= static_cast<double>(m_ZoomPercent) / 100;
        h *= static_cast<double>(m_ZoomPercent) / 100;
    }

    x = std::max(0, (ww - w) / 2);
    y = std::max(0, (wh - h) / 2);
}

void ImageBox::draw_image(bool scroll)
{
    // Don't draw images that don't exist (obviously)
    // Don't draw loading animated GIFs
    // Don't draw loading images that haven't created a pixbuf yet
    // Don't draw loading webm files (only booru images can have a loading webm)
    // The pixbuf_changed signal will fire when the above images are ready to be drawn
    if (!m_Image || (m_Image->is_loading() &&
                     (m_Image->is_animated_gif() || !m_Image->get_pixbuf() || m_Image->is_webm())))
    {
        m_RedrawQueued = false;
        return;
    }

    // Temporary pixbuf used when scaling is needed
    Glib::RefPtr<Gdk::Pixbuf> temp_pixbuf;
    bool error{ false };

    // if the image is still loading we want to draw all requests
    // Only booru images will do this
    m_Loading = m_Image->is_loading();

    if (!m_Image->is_webm())
    {
        // Start animation if this is a new animated GIF
        if (m_Image->is_animated_gif() && !m_Loading && !m_AnimConn)
        {
            m_AnimConn.disconnect();
            if (!m_Image->get_gif_finished_looping())
                m_AnimConn = Glib::signal_timeout().connect(
                    sigc::mem_fun(*this, &ImageBox::update_animation),
                    m_Image->get_gif_frame_delay());
        }

        Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_Image->get_pixbuf();

        if (pixbuf)
        {
            // Set this here incase we dont need to scale
            temp_pixbuf = pixbuf;

            m_OrigWidth  = pixbuf->get_width();
            m_OrigHeight = pixbuf->get_height();
        }
        else
        {
            error = true;
        }
    }
    else if (!m_VideoBox->is_playing())
    {
        std::tie(error, m_OrigWidth, m_OrigHeight) = m_VideoBox->load_file(m_Image->get_path());
    }

    if (error)
    {
        temp_pixbuf  = Image::get_missing_pixbuf();
        m_OrigWidth  = temp_pixbuf->get_width();
        m_OrigHeight = temp_pixbuf->get_height();
    }

    int w, h, x, y;
    get_scale_and_position(w, h, x, y);
    m_Scale =
        m_ZoomMode == ZoomMode::MANUAL ? m_ZoomPercent : static_cast<double>(w) / m_OrigWidth * 100;
    if (!m_Image->is_webm() && !error && (w != m_OrigWidth || h != m_OrigHeight))
        temp_pixbuf = m_Image->get_pixbuf()->scale_simple(w, h, Gdk::INTERP_BILINEAR);

    double h_adjust_val{ 0 }, v_adjust_val{ 0 };

    // Used to keep the adjustments centered when manual zooming
    if (m_ZoomScroll)
    {
        h_adjust_val =
            get_hadjustment()->get_value() /
            std::max(get_hadjustment()->get_upper() - get_hadjustment()->get_page_size(), 1.0);
        v_adjust_val =
            get_vadjustment()->get_value() /
            std::max(get_vadjustment()->get_upper() - get_vadjustment()->get_page_size(), 1.0);
    }

    get_window()->freeze_updates();

    m_Fixed->set_size_request(w, h);

    if (temp_pixbuf)
    {
        m_Fixed->move(*m_Overlay, x, y);
        m_GtkImage->set(temp_pixbuf);
        m_Overlay->show();
    }
    else
    {
        if (!m_VideoBox->is_playing())
        {
            m_GtkImage->clear();
            m_Overlay->hide();
            m_VideoBox->show();
        }

        m_Fixed->move(*m_VideoBox, x, y);
        m_VideoBox->set_size_request(w, h);
    }

    // Reset the scrollbar positions
    if (scroll || m_FirstDraw)
    {
        if (m_RestoreScrollPos.v != -1 && m_RestoreScrollPos.zoom == m_ZoomMode)
        {
            // Restore and reset stored scrollbar positions
            get_vadjustment()->set_value(m_RestoreScrollPos.v);
            get_hadjustment()->set_value(m_RestoreScrollPos.h);
            m_RestoreScrollPos = { -1, -1, m_ZoomMode };
        }
        else
        {
            get_vadjustment()->set_value(0);
            if (Settings.get_bool("MangaMode"))
                // Start at the right side of the image
                get_hadjustment()->set_value(get_hadjustment()->get_upper() -
                                             get_hadjustment()->get_page_size());
            else
                get_hadjustment()->set_value(0);
        }

        if (m_FirstDraw && !m_Image->get_notes().empty() && !m_NotesConn)
            on_notes_changed();

        m_FirstDraw = false;
    }
    else if (m_ZoomScroll)
    {
        get_hadjustment()->set_value(std::max(
            h_adjust_val * (get_hadjustment()->get_upper() - get_hadjustment()->get_page_size()),
            0.0));
        get_vadjustment()->set_value(std::max(
            v_adjust_val * (get_vadjustment()->get_upper() - get_vadjustment()->get_page_size()),
            0.0));
        m_ZoomScroll = false;
    }

    // Finally start playing the video
    if (!error && !m_VideoBox->is_playing() && m_Image->is_webm())
        m_VideoBox->start();

    get_window()->thaw_updates();
    m_RedrawQueued = false;
    m_StatusBar->set_resolution(m_OrigWidth, m_OrigHeight, m_Scale, m_ZoomMode);

    update_notes();
    m_SignalImageDrawn();
}

bool ImageBox::update_animation()
{
    if (m_Image->is_loading())
        return true;

    bool gif_finished{ m_Image->gif_advance_frame() };

    if (!gif_finished)
        m_AnimConn = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &ImageBox::update_animation), m_Image->get_gif_frame_delay());

    return false;
}

void ImageBox::scroll(const int x, const int y, const bool panning, const bool from_slideshow)
{
    int adjust_upper_x = std::max(0,
                                  static_cast<int>((get_hadjustment()->get_upper()) -
                                                   get_hadjustment()->get_page_size())),
        adjust_upper_y = std::max(0,
                                  static_cast<int>((get_vadjustment()->get_upper()) -
                                                   get_vadjustment()->get_page_size()));

    if (!from_slideshow)
        reset_slideshow();

    if (panning)
    {
        int n_x = get_hadjustment()->get_value() + x, n_y = get_vadjustment()->get_value() + y;

        if (n_x <= adjust_upper_x)
            get_hadjustment()->set_value(n_x);

        if (n_y <= adjust_upper_y)
            get_vadjustment()->set_value(n_y);
    }
    else
    {
        // Rounds scroll amount up if the end/start of the adjustment is
        // within 50% of the initial scroll amount
        // adjv = adjustment value, adjmax = adjustment max value, v = scroll value
        static auto round_scroll = [](const int adjv, const int adjmax, const int v) {
            const int range = std::abs(v * 0.5);
            // Going down/right
            if (v > 0 && adjmax - (adjv + v) <= range)
                return adjmax - adjv;
            // Going up/left, and not already going to scroll to the end
            else if (adjv > std::abs(v) && adjv + v <= range)
                return -adjv;

            return v;
        };

        // Scroll to next page
        if ((get_hadjustment()->get_value() == adjust_upper_x && x > 0) ||
            (get_vadjustment()->get_value() == adjust_upper_y && y > 0))
        {
            if (m_SlideshowConn && !m_NextAction->get_enabled())
                m_SignalSlideshowEnded();
            else
                m_NextAction->activate();
        }
        // Scroll to previous page
        else if ((get_hadjustment()->get_value() == 0 && x < 0) ||
                 (get_vadjustment()->get_value() == 0 && y < 0))
        {
            m_PreviousAction->activate();
        }
        else if (x != 0)
        {
            smooth_scroll(round_scroll(get_hadjustment()->get_value(), adjust_upper_x, x),
                          m_HSmoothScroll);
        }
        else if (y != 0)
        {
            smooth_scroll(round_scroll(get_vadjustment()->get_value(), adjust_upper_y, y),
                          m_VSmoothScroll);
        }
    }
}

void ImageBox::smooth_scroll(const int amount, ImageBox::SmoothScroll& ss)
{
    // Start a new callback if changing directions or theres no active scrolling callback
    auto target{ ss.end - ss.start };
    if (!ss.scrolling || (amount < 0 && target > 0) || (amount > 0 && target < 0))
    {
        remove_scroll_callback(ss);
        ss.start = ss.adj->get_value();
        ss.end   = ss.start + amount;
    }
    // Continuing an anmation by adding the remaining amount to the current target
    else
    {
        ss.start = ss.adj->get_value();
        ss.end += amount;
    }

    ss.start_time = get_frame_clock()->get_frame_time();
    ss.end_time =
        ss.start_time + std::min(500, static_cast<int>(std::abs(ss.end - ss.start))) * 1000;

    if (!ss.scrolling)
    {
        ss.id = add_tick_callback([&ss](const Glib::RefPtr<Gdk::FrameClock>& clock) {
            auto now{ clock->get_frame_time() };
            if (now < ss.end_time && ss.adj->get_value() != ss.end)
            {
                auto t{ static_cast<double>(now - ss.start_time) / (ss.end_time - ss.start_time) };
                ss.adj->set_value((ss.end - ss.start) * std::sin(t * (M_PI / 2)) + ss.start);
                return (ss.scrolling = true);
            }
            else
            {
                ss.adj->set_value(ss.end);
                return (ss.scrolling = false);
            }
        });
    }
}

void ImageBox::remove_scroll_callback(ImageBox::SmoothScroll& ss)
{
    if (ss.scrolling)
    {
        remove_tick_callback(ss.id);
        ss.scrolling = false;
    }
}

void ImageBox::zoom(const std::uint32_t percent)
{
    if (m_ZoomMode != ZoomMode::MANUAL || percent < 10 || percent > 400)
        return;

    m_ZoomScroll  = m_ZoomPercent != percent;
    m_ZoomPercent = percent;
    queue_draw_image();
}

bool ImageBox::advance_slideshow()
{
    scroll(0, 300, false, true);

    return true;
}

bool ImageBox::on_cursor_timeout()
{
    // hide_controls will return false if the cursor is over the controls widget
    // meaning we should not hide the cursor
    if (!m_VideoBox->is_playing() || m_VideoBox->hide_controls(true))
        m_Fixed->get_window()->set_cursor(m_BlankCursor);

    return false;
}

void ImageBox::on_notes_changed()
{
    double scale{ m_Scale / 100 };
    for (auto& note : m_Image->get_notes())
    {
        auto n{ Gtk::make_managed<ImageBoxNote>(note) };
        m_Notes.push_back(n);

        n->set_scale(scale);
        m_NoteFixed->put(*n, n->get_x(), n->get_y());
        n->show();
    }
}

void ImageBox::clear_notes()
{
    for (auto& note : m_Notes)
        m_NoteFixed->remove(*note);

    m_Notes.clear();
}

void ImageBox::update_notes()
{
    double scale{ m_Scale / 100 };
    for (auto& note : m_Notes)
    {
        note->set_scale(scale);
        m_NoteFixed->move(*note, note->get_x(), note->get_y());
    }
}
