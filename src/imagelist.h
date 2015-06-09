#ifndef _IMAGELIST_H_
#define _IMAGELIST_H_

#include <gtkmm.h>
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <pugixml.hpp>

#include "archive/archive.h"
#include "image.h"

namespace AhoViewer
{
    namespace Booru { class Page; }
    class ImageList : public sigc::trackable
    {
    public:
        // This signal is emitted when m_Index is changed.
        // It is connected by the MainWindow which tells the ImageBox to draw the new image.
        typedef sigc::signal<void, const std::shared_ptr<Image>&> SignalChangedType;

        // Emitted when AutoOpenArchive is true and loading an archive fails.
        typedef sigc::signal<void, const std::string> SignalArchiveErrorType;

        // Emitted when clear() is called.
        typedef sigc::signal<void> SignalClearedType;

        // Used for async thumbnail pixbuf loading
        typedef std::pair<size_t, const Glib::RefPtr<Gdk::Pixbuf>> PixbufPair;

        // ImageList::Widget {{{
        // This is used by ThumbnailBar and Booru::Page.
        class Widget
        {
        public:
            // When the widget's selected item changes it will emit this signal.
            typedef sigc::signal<void, const size_t> SignalSelectedChangedType;

            class ModelColumns : public Gtk::TreeModel::ColumnRecord
            {
            public:
                ModelColumns() { add(pixbuf_column); }
                Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> pixbuf_column;
            };

            virtual ~Widget() { };

            void clear() { m_ListStore->clear(); }
            void reserve(const size_t s)
            {
                for (size_t i = 0; i < s; ++i)
                    m_ListStore->append();
            }
            void set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf> pixbuf)
            {
                m_ListStore->children()[index]->set_value(0, pixbuf);
            }

            virtual void on_thumbnails_loaded(const size_t) { }
            virtual void set_selected(const size_t) = 0;

            SignalSelectedChangedType signal_selected_changed() const { return m_SignalSelectedChanged; }
        protected:
            Glib::RefPtr<Gtk::ListStore> m_ListStore;
            SignalSelectedChangedType m_SignalSelectedChanged;
        };
        // }}}

        ImageList(Widget*);
        virtual ~ImageList();

        void clear();
        bool load(const std::string path, std::string &error, int index = 0);
        void load(const pugi::xml_node &posts, Booru::Page *const page);

        // Action callbacks {{{
        void go_next();
        void go_previous();
        void go_first();
        void go_last();
        // }}}

        size_t get_index() const { return m_Index; }
        size_t get_size() const { return m_Images.size(); }
        const std::shared_ptr<Image>& get_current() const { return m_Images.at(m_Index); }
        const std::vector<std::shared_ptr<Image>>& get_images() const { return m_Images; }
        const std::shared_ptr<Archive>& get_archive() const { return m_Archive; }
        bool empty() const { return m_Images.empty(); }
        bool from_archive() const { return !!m_Archive; }

        SignalChangedType signal_changed() const { return m_SignalChanged; }
        SignalArchiveErrorType signal_archive_error() const { return m_SignalArchiveError; }
        SignalClearedType signal_cleared() const { return m_SignalCleared; }
    private:
        std::vector<std::string> get_image_entries(const std::string &path, int recurseCount = 0);
        std::vector<std::string> get_archive_entries();
        void load_thumbnails();

        void on_thumbnail_loaded();
        void on_thumbnails_loaded();

        void set_current(const size_t index, const bool fromWidget = false);

        void update_cache();
        void cancel_cache();

        Widget *const m_Widget;
        std::vector<std::shared_ptr<Image>> m_Images;
        std::vector<size_t> m_Cache;
        std::shared_ptr<Archive> m_Archive;
        std::vector<std::string> m_ArchiveEntries;
        std::queue<PixbufPair> m_ThumbnailQueue;
        size_t m_Index;

        Glib::RefPtr<Gio::Cancellable> m_CacheCancel,
                                       m_ThumbnailCancel;
        Glib::Threads::Mutex m_ThumbnailMutex;
        Glib::Threads::Thread *m_CacheThread,
                              *m_ThumbnailThread;

        Glib::Dispatcher m_SignalThumbnailLoaded,
                         m_SignalThumbnailsLoaded;

        SignalChangedType m_SignalChanged;
        SignalArchiveErrorType m_SignalArchiveError;
        SignalClearedType m_SignalCleared;
    };
}

#endif /* _IMAGELIST_H_ */
