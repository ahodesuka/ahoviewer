#include "imageboxnote.h"
using namespace AhoViewer;

#include <iostream>

ImageBoxNote::ImageBoxNote(const Note &note)
  : Glib::ObjectBase{ "ImageBoxNote" },
    Gtk::Widget{},
    m_refCssProvider{ Gtk::CssProvider::create() },
    m_Note{ note },
    m_Popover{ *this }
{
    set_has_window(true);
    set_name("ImageBoxNote");

    set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);

    auto refStyleContext = get_style_context();
    refStyleContext->add_provider(m_refCssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    m_refCssProvider->load_from_resource("/ui/css/note.css");

    auto label{ Gtk::manage(new Gtk::Label{ m_Note.body }) };
    label->set_line_wrap(true);
    label->set_max_width_chars(40);
    label->show();

    m_Popover.add(*label);
    m_Popover.set_modal(false);
    m_Popover.set_name("NotePopover");

    auto css = Gtk::CssProvider::create();
    refStyleContext = m_Popover.get_style_context();
    refStyleContext->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    css->load_from_resource("/ui/css/note-popover.css");
}

Gtk::SizeRequestMode ImageBoxNote::get_request_mode_vfunc() const
{
    //Accept the default value supplied by the base class.
    return Gtk::Widget::get_request_mode_vfunc();
}

void ImageBoxNote::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = m_Note.w * m_Scale;
    natural_width = m_Note.w * m_Scale;
}

void ImageBoxNote::get_preferred_height_for_width_vfunc(int, int& minimum_height, int& natural_height) const
{
    minimum_height = m_Note.h * m_Scale;
    natural_height = m_Note.h * m_Scale;
}

void ImageBoxNote::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const
{
    minimum_height = m_Note.h * m_Scale;
    natural_height = m_Note.h * m_Scale;
}

void ImageBoxNote::get_preferred_width_for_height_vfunc(int, int& minimum_width, int& natural_width) const
{
    minimum_width = m_Note.w * m_Scale;
    natural_width = m_Note.w * m_Scale;
}

void ImageBoxNote::on_size_allocate(Gtk::Allocation& allocation)
{
    //Do something with the space that we have actually been given:
    //(We will not be given heights or widths less than we have requested, though
    //we might get more)

    //Use the offered allocation for this container:
    set_allocation(allocation);

    if (m_refGdkWindow)
        m_refGdkWindow->move_resize(
            allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height());
}

void ImageBoxNote::on_realize()
{
    //Do not call base class Gtk::Widget::on_realize().
    //It's intended only for widgets that set_has_window(false).

    set_realized();

    if (!m_refGdkWindow)
    {
        //Create the GdkWindow:
        GdkWindowAttr attributes;
        memset(&attributes, 0, sizeof(attributes));

        Gtk::Allocation allocation{ get_allocation() };

        //Set initial position and size of the Gdk::Window:
        attributes.x = allocation.get_x();
        attributes.y = allocation.get_y();
        attributes.width = allocation.get_width();
        attributes.height = allocation.get_height();

        attributes.event_mask = get_events() | Gdk::EXPOSURE_MASK;
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.wclass = GDK_INPUT_OUTPUT;

        m_refGdkWindow = Gdk::Window::create(
            get_parent_window(), &attributes, GDK_WA_X | GDK_WA_Y);
        set_window(m_refGdkWindow);

        //make the widget receive expose events
        m_refGdkWindow->set_user_data(gobj());
    }
}

void ImageBoxNote::on_unrealize()
{
    m_refGdkWindow.reset();
    Gtk::Widget::on_unrealize();
}

bool ImageBoxNote::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const Gtk::Allocation allocation{ get_allocation() };
    auto refStyleContext{ get_style_context() };

    // paint the background
    refStyleContext->render_background(cr, 0, 0,
        allocation.get_width(), allocation.get_height());
    // paint the border
    refStyleContext->render_frame(cr, 0, 0,
        allocation.get_width(), allocation.get_height());

    return Gtk::Widget::on_draw(cr);
}

bool ImageBoxNote::on_enter_notify_event(GdkEventCrossing*)
{
    m_Popover.show();
    return true;
}

bool ImageBoxNote::on_leave_notify_event(GdkEventCrossing *e)
{
    m_Popover.hide();
    return true;
}
