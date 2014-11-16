#include "imagebox.h"
using namespace AhoViewer;

#include "settings.h"
#include "statusbar.h"

const double ImageBox::SmoothScrollStep = 1000.0 / 60.0;

ImageBox::ImageBox(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::EventBox(cobj),
    m_LeftPtrCursor(Gdk::LEFT_PTR),
    m_FleurCursor(Gdk::FLEUR),
    m_ZoomMode(Settings.get_zoom_mode()),
    m_ZoomPercent(100)
{
    bldr->get_widget("ImageBox::Layout",  m_Layout);
    bldr->get_widget("ImageBox::HScroll", m_HScroll);
    bldr->get_widget("ImageBox::VScroll", m_VScroll);
    bldr->get_widget("ImageBox::Image",   m_GtkImage);

    m_HAdjust = Glib::RefPtr<Gtk::Adjustment>::cast_static(bldr->get_object("ImageBox::HAdjust"));
    m_VAdjust = Glib::RefPtr<Gtk::Adjustment>::cast_static(bldr->get_object("ImageBox::VAdjust"));
    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));
}

ImageBox::~ImageBox()
{

}

void ImageBox::queue_draw_image(const bool scroll)
{
    if (!get_realized() || !m_Image || (m_DrawConn && !scroll))
        return;

    m_DrawConn.disconnect();
    m_DrawConn = Glib::signal_idle().connect(
            sigc::bind_return(sigc::bind(sigc::mem_fun(*this, &ImageBox::draw_image), scroll), false));
}

void ImageBox::set_image(const std::shared_ptr<Image> &image)
{
    m_ImageConn.disconnect();
    reset_slideshow();

    m_Image = image;
    m_Scroll = true;
    m_ImageConn = m_Image->signal_pixbuf_changed().connect([ this ]() { queue_draw_image(m_Scroll); });

    queue_draw_image(true);
}

void ImageBox::clear_image()
{
    m_DrawConn.disconnect();
    m_GtkImage->clear();
    m_Layout->set_size(0, 0);

    m_HScroll->hide();
    m_VScroll->hide();

    m_StatusBar->clear_resolution();
    m_Image = nullptr;
    stop_slideshow();
}

void ImageBox::update_background_color()
{
    m_Layout->modify_bg(Gtk::STATE_NORMAL, Settings.get_background_color());
}

bool ImageBox::is_slideshow_running()
{
    return !!m_SlideshowConn;
}

void ImageBox::reset_slideshow()
{
    if (m_SlideshowConn)
    {
        stop_slideshow();
        start_slideshow();
    }
}

void ImageBox::start_slideshow()
{
    m_SlideshowConn = Glib::signal_timeout().connect_seconds(
            sigc::mem_fun(*this, &ImageBox::advance_slideshow), Settings.get_int("SlideshowDelay"));
}

void ImageBox::stop_slideshow()
{
    m_SlideshowConn.disconnect();
}

ImageBox::ZoomMode ImageBox::get_zoom_mode() const
{
    return m_ZoomMode;
}

void ImageBox::set_zoom_mode(const ZoomMode mode)
{
    if (mode != m_ZoomMode)
    {
        Settings.set_zoom_mode(mode);
        m_ZoomMode = mode;
        queue_draw_image();
    }
}

void ImageBox::set_statusbar(StatusBar *sb)
{
    m_StatusBar = sb;
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

void ImageBox::on_toggle_scrollbars()
{
    Settings.set_bool("ScrollbarsVisible", !Settings.get_bool("ScrollbarsVisible"));
    queue_draw_image();
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
    m_PopupMenu = static_cast<Gtk::Menu*>(m_UIManager->get_widget("/PopupMenu"));

    Glib::RefPtr<Gtk::ActionGroup> actionGroup =
        static_cast<std::vector<Glib::RefPtr<Gtk::ActionGroup>>>(m_UIManager->get_action_groups())[0];

    m_NextAction = actionGroup->get_action("NextImage");
    m_PreviousAction = actionGroup->get_action("PreviousImage");

    update_background_color();

    Gtk::EventBox::on_realize();
}

bool ImageBox::on_button_press_event(GdkEventButton *e)
{
    grab_focus();

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
                m_PopupMenu->popup(e->button, e->time);
                return true;
            case 8: // Back
                m_PreviousAction->activate();
                return true;
            case 9: // Forward
                m_NextAction->activate();
                return true;
        }
    }

    return Gtk::EventBox::on_button_press_event(e);
}

bool ImageBox::on_button_release_event(GdkEventButton *e)
{
    if (e->button == 1 || e->button == 2)
    {
        m_Layout->get_window()->set_cursor(m_LeftPtrCursor);

        if (e->button == 1 && m_PressX == m_PreviousX && m_PressY == m_PreviousY)
            m_NextAction->activate();

        return true;
    }

    return Gtk::EventBox::on_button_release_event(e);
}

bool ImageBox::on_motion_notify_event(GdkEventMotion *e)
{
    if (m_Image && ((e->state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK ||
                    (e->state & GDK_BUTTON2_MASK) == GDK_BUTTON2_MASK))
    {
        m_Layout->get_window()->set_cursor(m_FleurCursor);
        scroll(m_PreviousX - e->x_root, m_PreviousY - e->y_root, true);

        m_PreviousX = e->x_root;
        m_PreviousY = e->y_root;

        return true;
    }

    return Gtk::EventBox::on_motion_notify_event(e);
}

bool ImageBox::on_scroll_event(GdkEventScroll *e)
{
    grab_focus();

    switch (e->direction)
    {
        case GDK_SCROLL_UP:
            if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
                on_zoom_in();
            else
                scroll(0, -300);
            return true;
        case GDK_SCROLL_DOWN:
            if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
                on_zoom_out();
            else
                scroll(0, 300);
            return true;
        case GDK_SCROLL_LEFT:
            scroll(-300, 0);
            return true;
        case GDK_SCROLL_RIGHT:
            scroll(300, 0);
            return true;
    }

    return Gtk::EventBox::on_scroll_event(e);
}

void ImageBox::draw_image(const bool _scroll)
{
    std::shared_ptr<Image> image = m_Image;

    // Make sure the pixbuf is ready
    if (!image->get_pixbuf())
        return;

    // m_Scroll will be true if this is the first time the image is being drawn.
    bool scroll = _scroll || m_Scroll;
    m_Scroll = false;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = image->get_pixbuf(),
                              temp = pixbuf;

    get_window()->freeze_updates();
    m_HScroll->show();
    m_VScroll->show();

    while (Gtk::Main::events_pending())
        Gtk::Main::iteration();

    // Make sure the image wasn't changed while handling events.
    if (image != m_Image)
    {
        get_window()->thaw_updates();
        return;
    }

    int wWidth  = get_allocation().get_width(),
        wHeight = get_allocation().get_height(),
        lWidth  = m_Layout->get_allocation().get_width(),
        lHeight = m_Layout->get_allocation().get_height(),
        w       = lWidth,
        h       = lHeight;
    double windowAspect = (double)wWidth / wHeight,
           imageAspect  = (double)pixbuf->get_width() / pixbuf->get_height();
    bool hideScrollbars = !Settings.get_bool("ScrollbarsVisible") || Settings.get_bool("HideAll");

    if ((pixbuf->get_width() > wWidth || (pixbuf->get_height() > wHeight && pixbuf->get_width() > lWidth)) &&
        (m_ZoomMode == ZoomMode::FIT_WIDTH || (m_ZoomMode == ZoomMode::AUTO_FIT && windowAspect < imageAspect)))
    {
        w = std::floor(wWidth / imageAspect) > wHeight && !hideScrollbars ? lWidth : wWidth;
        temp = pixbuf->scale_simple(w, std::floor(w / imageAspect), Gdk::INTERP_BILINEAR);
    }
    else if ((pixbuf->get_height() > wHeight || (pixbuf->get_width() > wWidth && pixbuf->get_height() > lHeight)) &&
             (m_ZoomMode == ZoomMode::FIT_HEIGHT || (m_ZoomMode == ZoomMode::AUTO_FIT && windowAspect > imageAspect)))
    {
        h = std::floor(wHeight * imageAspect) > wWidth && !hideScrollbars ? lHeight : wHeight;
        temp = pixbuf->scale_simple(std::floor(h * imageAspect), h, Gdk::INTERP_BILINEAR);
    }
    else if (m_ZoomMode == ZoomMode::MANUAL && m_ZoomPercent != 100)
    {
        temp = pixbuf->scale_simple(pixbuf->get_width() * (double)m_ZoomPercent / 100,
                                    pixbuf->get_height() * (double)m_ZoomPercent / 100,
                                    Gdk::INTERP_BILINEAR);
    }

    if (hideScrollbars || temp->get_width() <= lWidth ||
            (temp->get_width() <= wWidth && temp->get_height() <= wHeight))
    {
        m_HScroll->hide();
        h = wHeight;
    }
    if (hideScrollbars || temp->get_height() <= lHeight ||
            (temp->get_height() <= wHeight && temp->get_width() <= wWidth))
    {
        m_VScroll->hide();
        w = wWidth;
    }

    m_Layout->move(*m_GtkImage, std::max(0, (w - temp->get_width()) / 2),
                                std::max(0, (h - temp->get_height()) / 2));
    m_Layout->set_size(temp->get_width(), temp->get_height());
    m_GtkImage->set(temp);

    // Reset the scrollbar positions
    if (scroll)
    {
        m_VAdjust->set_value(0);
        if (Settings.get_bool("MangaMode"))
        {
            while (Gtk::Main::events_pending())
                Gtk::Main::iteration(false);

            // Start at the right side of the image
            m_HAdjust->set_value(m_HAdjust->get_upper() - m_HAdjust->get_page_size());
        }
        else
        {
            m_HAdjust->set_value(0);
        }
    }

    get_window()->thaw_updates();
    double scale = (double)temp->get_width() / pixbuf->get_width() * 100;
    m_StatusBar->set_resolution(pixbuf->get_width(), pixbuf->get_height(), scale, m_ZoomMode);
}

void ImageBox::scroll(const int x, const int y, const bool panning, const bool fromSlideshow)
{
    int adjustUpperX = std::max(0, (int)(m_HAdjust->get_upper() - m_HAdjust->get_page_size())),
        adjustUpperY = std::max(0, (int)(m_VAdjust->get_upper() - m_VAdjust->get_page_size()));

    if (panning)
    {
        int nX = m_HAdjust->get_value() + x,
            nY = m_VAdjust->get_value() + y;

        if (nX <= adjustUpperX)
            m_HAdjust->set_value(nX);

        if (nY <= adjustUpperY)
            m_VAdjust->set_value(nY);
    }
    else
    {
        if (!fromSlideshow)
            reset_slideshow();

        if ((m_HAdjust->get_value() == adjustUpperX && x > 0) ||
            (m_VAdjust->get_value() == adjustUpperY && y > 0))
        {
            m_ScrollConn.disconnect();
            m_NextAction->activate();
            return;
        }

        if ((m_HAdjust->get_value() == 0 && x < 0) ||
                 (m_VAdjust->get_value() == 0 && y < 0))
        {
            m_ScrollConn.disconnect();
            m_PreviousAction->activate();
            return;
        }

        if (x != 0)
            smooth_scroll(x, m_HAdjust);
        else if (y != 0)
            smooth_scroll(y, m_VAdjust);
    }
}
void ImageBox::smooth_scroll(const int amount, const Glib::RefPtr<Gtk::Adjustment> &adj)
{
    // Cancel current animation if we changed direction
    if ((amount < 0 && m_ScrollTarget > 0) ||
        (amount > 0 && m_ScrollTarget < 0) ||
        adj != m_ScrollAdjust || !m_ScrollConn)
    {
        m_ScrollConn.disconnect();
        m_ScrollTarget = m_ScrollDuration = 0;
        m_ScrollAdjust = adj;
    }

    m_ScrollTime = 0;
    m_ScrollTarget += amount;
    m_ScrollStart = m_ScrollAdjust->get_value();

    if (m_ScrollTarget > 0)
    {
        double upperMax = m_ScrollAdjust->get_upper() - m_ScrollAdjust->get_page_size() -
                          m_ScrollAdjust->get_value();
        m_ScrollTarget = std::min(upperMax, m_ScrollTarget);
    }
    else if (m_ScrollTarget < 0)
    {
        double lowerMax = m_ScrollAdjust->get_lower() - m_ScrollAdjust->get_value();
        m_ScrollTarget = std::max(lowerMax, m_ScrollTarget);
    }

    m_ScrollDuration = std::ceil(std::min(500.0, std::abs(m_ScrollTarget)) / SmoothScrollStep) * SmoothScrollStep;

    if (!m_ScrollConn)
        m_ScrollConn = Glib::signal_timeout().connect(
                sigc::mem_fun(*this, &ImageBox::update_smooth_scroll),
                SmoothScrollStep);
}

bool ImageBox::update_smooth_scroll()
{
    m_ScrollTime += SmoothScrollStep;

    double val = m_ScrollTarget * std::sin(m_ScrollTime / m_ScrollDuration * (M_PI / 2)) + m_ScrollStart;
    val = m_ScrollTarget < 0 ? std::floor(val) : std::ceil(val);

    m_ScrollAdjust->set_value(val);

    return m_ScrollAdjust->get_value() != m_ScrollStart + m_ScrollTarget;
}


void ImageBox::zoom(const std::uint32_t percent)
{
    if (m_ZoomMode != ZoomMode::MANUAL || percent < 10 || percent > 400)
        return;

    m_ZoomPercent = percent;
    queue_draw_image();
}

bool ImageBox::advance_slideshow()
{
    // TODO: Smart scrolling, as an option
    // e.g. Scroll the width of the image before scrolling down
    scroll(0, 300, false, true);

    return true;
}
