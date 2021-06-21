#include "imageboxnote.h"
using namespace AhoViewer;

ImageBoxNote::ImageBoxNote(const Note& note)
    : Glib::ObjectBase{ "ImageBoxNote" },
      Gtk::Widget{},
      m_CssProvider{ Gtk::CssProvider::create() },
      m_Note{ note },
      m_Popover{ *this }
{
    set_has_window(true);
    set_name("ImageBoxNote");

    set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);

    auto style_context = get_style_context();
    style_context->add_provider(m_CssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    m_CssProvider->load_from_resource("/ui/css/note.css");

    auto label{ Gtk::make_managed<Gtk::Label>(m_Note.body) };
    label->set_line_wrap(true);
    label->set_max_width_chars(40);
    label->show();

    m_Popover.add(*label);
    m_Popover.set_modal(false);
    m_Popover.set_name("NotePopover");
    m_Popover.signal_enter_notify_event().connect(
        sigc::mem_fun(*this, &ImageBoxNote::on_enter_notify_event));
    m_Popover.signal_leave_notify_event().connect(
        sigc::mem_fun(*this, &ImageBoxNote::on_leave_notify_event));

    auto css      = Gtk::CssProvider::create();
    style_context = m_Popover.get_style_context();
    style_context->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    css->load_from_resource("/ui/css/note-popover.css");
}

void ImageBoxNote::set_scale(const double scale)
{
    if (scale != m_Scale)
    {
        m_Scale = scale;
        queue_resize();
    }
}

Gtk::SizeRequestMode ImageBoxNote::get_request_mode_vfunc() const
{
    return Gtk::SizeRequestMode::SIZE_REQUEST_CONSTANT_SIZE;
}

void ImageBoxNote::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = m_Note.w * m_Scale;
    natural_width = m_Note.w * m_Scale;
}

void ImageBoxNote::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const
{
    minimum_height = m_Note.h * m_Scale;
    natural_height = m_Note.h * m_Scale;
}

void ImageBoxNote::on_size_allocate(Gtk::Allocation& allocation)
{
    // Do something with the space that we have actually been given:
    //(We will not be given heights or widths less than we have requested,
    // though we might get more)

    // Use the offered allocation for this container:
    set_allocation(allocation);

    if (m_GdkWindow)
        m_GdkWindow->move_resize(allocation.get_x(),
                                 allocation.get_y(),
                                 allocation.get_width(),
                                 allocation.get_height());
}

void ImageBoxNote::on_realize()
{
    // Do not call base class Gtk::Widget::on_realize().
    // It's intended only for widgets that set_has_window(false).
    set_realized();

    if (!m_GdkWindow)
    {
        // Create the GdkWindow:
        GdkWindowAttr attributes;
        memset(&attributes, 0, sizeof(attributes));

        Gtk::Allocation allocation{ get_allocation() };

        // Set initial position and size of the Gdk::Window:
        attributes.x      = allocation.get_x();
        attributes.y      = allocation.get_y();
        attributes.width  = allocation.get_width();
        attributes.height = allocation.get_height();

        attributes.event_mask  = get_events() | Gdk::EXPOSURE_MASK;
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.wclass      = GDK_INPUT_OUTPUT;

        m_GdkWindow = Gdk::Window::create(get_parent_window(), &attributes, GDK_WA_X | GDK_WA_Y);
        set_window(m_GdkWindow);

        // make the widget receive expose events
        m_GdkWindow->set_user_data(gobj());
    }
}

void ImageBoxNote::on_unrealize()
{
    m_GdkWindow.reset();
    Gtk::Widget::on_unrealize();
}

bool ImageBoxNote::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const Gtk::Allocation allocation{ get_allocation() };
    auto style_context{ get_style_context() };

    // paint the background
    style_context->render_background(cr, 0, 0, allocation.get_width(), allocation.get_height());
    // paint the border
    style_context->render_frame(cr, 0, 0, allocation.get_width(), allocation.get_height());

    return Gtk::Widget::on_draw(cr);
}

bool ImageBoxNote::on_enter_notify_event(GdkEventCrossing*)
{
    // There's a chance that the popdown animation is playing and needs to be cancelled via hide
    // before calling popup or else it will be ignored
    if (!m_TimeoutConn)
        m_Popover.hide();

    m_TimeoutConn.disconnect();
    m_Popover.popup();

    return true;
}

bool ImageBoxNote::on_leave_notify_event(GdkEventCrossing*)
{
    // 200 millisecond delay on hiding the note popup
    m_TimeoutConn = Glib::signal_timeout().connect(
        [&]() {
            m_Popover.popdown();
            return false;
        },
        200);

    return true;
}
