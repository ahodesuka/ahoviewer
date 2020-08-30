#include "statusbar.h"
using namespace AhoViewer;

#include <iomanip>
#include <sstream>

StatusBar::StatusBar(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr) : Gtk::Box(cobj)
{
    bldr->get_widget("PageInfoLabel", m_PageInfo);
    bldr->get_widget("ResolutionLabel", m_Resolution);
    bldr->get_widget("FilenameSeparator", m_FilenameSeparator);
    bldr->get_widget("FilenameLabel", m_Filename);
    bldr->get_widget("MessageLabel", m_Message);
    bldr->get_widget("ProgressBar", m_ProgressBar);
}

void StatusBar::set_page_info(const size_t page, const size_t total)
{
    m_PageInfo->set_text(std::to_string(page) + " / " + std::to_string(total));
}

void StatusBar::set_resolution(const int w,
                               const int h,
                               const double scale,
                               const ZoomMode zoom_mode)
{
    std::ostringstream ss;
    ss << std::setprecision(1) << std::fixed;
    ss << w << "x" << h << " (" << scale << "%) [" << static_cast<char>(zoom_mode) << "]";
    m_Resolution->set_text(ss.str());
}

void StatusBar::set_filename(const std::string& filename)
{
    m_Filename->set_text(filename);
}

void StatusBar::set_message(const std::string& msg,
                            const Priority priority,
                            const std::uint8_t delay)
{
    if (priority < m_MessagePriority)
        return;

    m_MessageConn.disconnect();
    m_MessagePriority = priority;

    m_Filename->hide();

    m_Message->set_text(msg);
    m_Message->show();

    // Tooltip messages are manually cleared.
    if (priority != Priority::TOOLTIP && delay > 0)
        m_MessageConn = Glib::signal_timeout().connect_seconds(
            sigc::bind_return(sigc::bind(sigc::mem_fun(*this, &StatusBar::clear_message), priority),
                              false),
            delay);
}

// Same as set_message but shows the progress bar as well
// This will only update the progress bar if the priority is lower
void StatusBar::set_progress(const std::string& msg,
                             const double fraction,
                             const Priority priority,
                             const std::uint8_t delay)
{
    if (priority < m_ProgressPriority)
        return;

    // Set message and timeout clean up
    set_message(msg, priority, delay);

    m_ProgressConn.disconnect();
    m_ProgressPriority = priority;
    m_ProgressBar->set_fraction(fraction);
    m_ProgressBar->show();

    if (delay > 0)
        m_ProgressConn = Glib::signal_timeout().connect_seconds(
            sigc::bind_return(
                sigc::bind(sigc::mem_fun(*this, &StatusBar::clear_progress), priority), false),
            delay);
}

void StatusBar::clear_page_info()
{
    m_PageInfo->set_text("");
}

void StatusBar::clear_resolution()
{
    m_Resolution->set_text("");
}

void StatusBar::clear_filename()
{
    // This whitespace is to keep the statusbar's height
    // consistant when there is nothing shown
    m_Filename->set_text(" ");
}

void StatusBar::clear_message(const Priority priority)
{
    if (priority < m_MessagePriority)
        return;

    m_MessagePriority = Priority::UNUSED;
    m_MessageConn.disconnect();

    // This means the set_progress was called with delay = 0
    // it is going to be called after this we dont want to clear the message
    // just yet or it will flicker  before set_progress sets the new message
    if (m_ProgressBar->is_visible() && !m_ProgressConn)
        return;

    m_Message->hide();
    m_Message->set_text("");

    m_Filename->show();
}

void StatusBar::clear_progress(const Priority priority)
{
    if (priority < m_ProgressPriority)
        return;

    m_ProgressPriority = Priority::UNUSED;
    m_ProgressConn.disconnect();
    m_ProgressBar->hide();
    m_ProgressBar->set_fraction(0);
}
