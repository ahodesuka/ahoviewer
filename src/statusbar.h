#ifndef _STATUSBAR_H_
#define _STATUSBAR_H_

#include "util.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class StatusBar : public Gtk::Box
    {
    public:
        enum class Priority : uint8_t
        {
            UNUSED = 0,
            DOWNLOAD,
            SAVE,
            MESSAGE,
            TOOLTIP
        };
        StatusBar(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        ~StatusBar() override = default;

        void set_page_info(const size_t page, const size_t total);
        void set_resolution(const int w, const int h, const double scale, const ZoomMode zoom_mode);
        void set_filename(const std::string& filename);
        void set_message(const std::string& msg,
                         const Priority priority  = Priority::MESSAGE,
                         const std::uint8_t delay = 3);
        void set_progress(const std::string& msg,
                          const double prog,
                          const Priority priority  = Priority::MESSAGE,
                          const std::uint8_t delay = 3);

        void clear_page_info();
        void clear_resolution();
        void clear_filename();
        void clear_message(const Priority priority);
        void clear_progress(const Priority priority);

    private:
        Gtk::Label *m_PageInfo, *m_Resolution, *m_Filename, *m_Message;
        Gtk::Separator* m_FilenameSeparator;
        Gtk::ProgressBar* m_ProgressBar;
        Priority m_MessagePriority{ Priority::UNUSED }, m_ProgressPriority{ Priority::UNUSED };
        sigc::connection m_MessageConn, m_ProgressConn;
    };
}

#endif /* _STATUSBAR_H_ */
