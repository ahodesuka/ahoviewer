#ifndef _STATUSBAR_H_
#define _STATUSBAR_H_

#include <gtkmm.h>

#include "imagebox.h"

namespace AhoViewer
{
    class StatusBar : public Gtk::Frame
    {
    public:
        enum class Priority : uint8_t
        {
            NORMAL = 0,
            HIGH,
            TOOLTIP
        };
        StatusBar(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        virtual ~StatusBar() = default;

        void set_page_info(const size_t, const size_t);
        void set_resolution(const int, const int, const double, const ImageBox::ZoomMode);
        void set_filename(const std::string&);
        void set_message(const std::string&,
                         const Priority priority = Priority::NORMAL,
                         const std::uint8_t delay = 3);
        void set_progress(const double,
                          const Priority priority = Priority::NORMAL,
                          const std::uint8_t delay = 3);

        void clear_page_info();
        void clear_resolution();
        void clear_filename();
        void clear_message();
        void clear_progress();
    private:
        Gtk::Label *m_PageInfo,
                   *m_Resolution,
                   *m_Filename,
                   *m_Message;
        Gtk::VSeparator *m_FilenameSeparator;
        Gtk::ProgressBar *m_ProgressBar;
        Priority m_MessagePriority,
                 m_ProgressPriority;
        sigc::connection m_MessageConn,
                         m_ProgressConn;
    };
}

#endif /* _STATUSBAR_H_ */
