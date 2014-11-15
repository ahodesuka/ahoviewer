#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <gdkmm.h>
#include <glibmm.h>

namespace AhoViewer
{
    class Image
    {
    public:
        Image(const std::string&);
        Image(const std::string&, const std::string&);
        virtual ~Image();

        static bool is_valid(const std::string&);
        static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

        inline const std::string get_path() const { return m_Path; }

        virtual std::string get_filename() const;
        virtual const Glib::RefPtr<Gdk::Pixbuf>& get_pixbuf();
        virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();

        virtual void load_pixbuf();
        virtual void reset_pixbuf();

        Glib::Dispatcher& signal_pixbuf_changed() { return m_SignalPixbufChanged; }

        static std::uint8_t const ThumbnailSize = 100;
    protected:
        void create_thumbnail();
        Glib::RefPtr<Gdk::Pixbuf> create_pixbuf_at_size(const std::string &path,
                                                        const double w, const double h);

        std::string m_Path, m_ThumbnailPath;

        Glib::RefPtr<Gdk::Pixbuf> m_Pixbuf,
                                  m_ThumbnailPixbuf;
        Glib::Threads::Mutex m_Mutex;
        Glib::Dispatcher m_SignalPixbufChanged;
    private:
        void create_save_thumbnail();
        static std::string ThumbnailDir;
    };
}

#endif /* _IMAGE_H_ */
