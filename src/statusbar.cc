#include <iomanip>
#include <sstream>

#include "statusbar.h"
using namespace AhoViewer;

StatusBar::StatusBar(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::HBox(cobj),
    m_MessagePriority(Priority::NORMAL),
    m_ProgressPriority(Priority::NORMAL)
{
    bldr->get_widget("PageInfoLabel",    m_PageInfo);
    bldr->get_widget("ResolutionLabel",  m_Resolution);
    bldr->get_widget("FilenameLabel",    m_Filename);
    bldr->get_widget("MessageLabel",     m_Message);
    bldr->get_widget("MessageSeparator", m_MessageSeparator);
    bldr->get_widget("ProgressBar",      m_ProgressBar);
}

void StatusBar::set_page_info(const size_t page, const size_t total)
{
    m_PageInfo->set_text(std::to_string(page) + " / " + std::to_string(total));
}

void StatusBar::set_resolution(const int w, const int h, const double scale,
                               const ImageBox::ZoomMode zoom_mode)
{
    std::ostringstream ss;
    ss << std::setprecision(1) << std::fixed;
    ss << w << "x" << h << " (" << scale << "%) [" << static_cast<char>(zoom_mode) << "]";
    m_Resolution->set_text(ss.str());
}

void StatusBar::set_filename(const std::string &filename)
{
    m_Filename->set_text(filename);
}

void StatusBar::set_message(const std::string &msg, const Priority priority, const std::uint8_t delay)
{
    if (priority < m_MessagePriority)
        return;

    m_MessageConn.disconnect();
    m_MessagePriority = priority;
    m_Message->set_text(msg);
    m_MessageSeparator->show();

    // Tooltip messages are manually cleared.
    if (priority != Priority::TOOLTIP && delay > 0)
        m_MessageConn = Glib::signal_timeout().connect_seconds(
                sigc::bind_return(sigc::mem_fun(*this, &StatusBar::clear_message), false), delay);
}

void StatusBar::set_progress(const double fraction, const Priority priority, const std::uint8_t delay)
{
    if (priority < m_ProgressPriority)
        return;

    m_ProgressConn.disconnect();
    m_ProgressPriority = priority;
    m_ProgressBar->set_fraction(fraction);
    m_ProgressBar->show();

    if (delay > 0)
        m_ProgressConn = Glib::signal_timeout().connect_seconds(
                sigc::bind_return(sigc::mem_fun(*this, &StatusBar::clear_progress), false), delay);
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
    m_Filename->set_text("");
}

void StatusBar::clear_message()
{
    m_MessageConn.disconnect();
    m_MessageSeparator->hide();
    m_Message->set_text("");
    m_MessagePriority = Priority::NORMAL;
}

void StatusBar::clear_progress()
{
    m_ProgressConn.disconnect();
    m_ProgressBar->hide();
    m_ProgressBar->set_fraction(0);
    m_ProgressPriority = Priority::NORMAL;
}
