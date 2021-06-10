#pragma once

#include "archive/archive.h"
#include "image.h"
#include "threadpool.h"
#include "tsqueue.h"
#include "util.h"

#include <gtkmm.h>
#include <memory>
#include <string>
#include <vector>

namespace AhoViewer
{
    class ImageList : public sigc::trackable
    {
        using ImageVector = std::vector<std::shared_ptr<Image>>;

        // This signal is emitted when m_Index is changed.
        // It is connected by the MainWindow which tells the ImageBox to draw the new image.
        using SignalChangedType = sigc::signal<void, const std::shared_ptr<Image>&>;

        // Emitted when AutoOpenArchive is true and loading an archive fails.
        using SignalArchiveErrorType = sigc::signal<void, const std::string>;

        // Used by booru/archive lists to report progress while loading
        // First argument is the number loaded, second is the total to load
        using SignalLoadProgressType = sigc::signal<void, size_t, size_t>;

        // Used for async thumbnail pixbuf loading
        using PixbufPair = std::pair<size_t, Glib::RefPtr<Gdk::Pixbuf>>;

    public:
        // ImageList::Widget {{{
        // This is used by ThumbnailBar and Booru::Page.
        class Widget
        {
            // When the widget's selected item changes it will emit this signal.
            using SignalSelectedChangedType = sigc::signal<void, const size_t>;

        public:
            Widget() : m_ListStore(Gtk::ListStore::create(m_Columns)) { }
            virtual ~Widget() = default;

            SignalSelectedChangedType signal_selected_changed() const
            {
                return m_SignalSelectedChanged;
            }
            struct ModelColumns : public Gtk::TreeModelColumnRecord
            {
                ModelColumns() { add(pixbuf); }
                Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> pixbuf;
            };

            virtual void set_selected(const size_t) = 0;
            virtual void scroll_to_selected()       = 0;

            virtual void clear()
            {
                m_CursorConn.block();
                m_ListStore->clear();
                m_CursorConn.unblock();
            }
            virtual void set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
            {
                Gtk::TreeIter it = m_ListStore->get_iter(std::to_string(index));
                if (it)
                    it->set_value(0, pixbuf);
            }
            void reserve(const size_t s)
            {
                for (size_t i = 0; i < s; ++i)
                    m_ListStore->append();
            }
            void erase(const size_t i)
            {
                Gtk::TreeIter it = m_ListStore->get_iter(std::to_string(i));
                if (it)
                    m_ListStore->erase(it);
            }
            void insert(const size_t i, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
            {
                Gtk::TreeIter it = m_ListStore->get_iter(std::to_string(i));
                it               = it ? m_ListStore->insert(it) : m_ListStore->append();
                it->set_value(0, pixbuf);
            }

            // Member ordering here is important, m_ListStore requires m_Columns
            // during initialization
            ModelColumns m_Columns;
            Glib::RefPtr<Gtk::ListStore> m_ListStore;

        protected:
            SignalSelectedChangedType m_SignalSelectedChanged;
            sigc::connection m_CursorConn;
        };
        // }}}

        ImageList(Widget* const w);
        virtual ~ImageList();

        virtual void clear();
        bool load(const std::string path, std::string& error, int index = 0);

        // Action callbacks {{{
        void go_next();
        void go_previous();
        void go_first();
        void go_last();
        // }}}

        bool can_go_next() const;
        bool can_go_previous() const;

        virtual size_t get_size() const { return m_Images.size(); }

        size_t get_index() const { return m_Index; }
        const std::shared_ptr<Image>& get_current() const { return m_Images[m_Index]; }
        const Archive& get_archive() const { return *m_Archive; }
        bool empty() const { return m_Images.empty(); }
        bool from_archive() const { return !!m_Archive; }

        void set_scroll_position(const ScrollPos& s) { m_ScrollPos = s; }
        const ScrollPos& get_scroll_position() const { return m_ScrollPos; }

        virtual void
        set_current(const size_t index, const bool from_widget = false, const bool force = false);

        ImageVector::iterator begin() { return m_Images.begin(); }
        ImageVector::iterator end() { return m_Images.end(); }

        void on_cache_size_changed();

        SignalChangedType signal_changed() const { return m_SignalChanged; }
        SignalArchiveErrorType signal_archive_error() const { return m_SignalArchiveError; }
        SignalLoadProgressType signal_load_progress() const { return m_SignalLoadProgress; }
        sigc::signal<void> signal_cleared() const { return m_SignalCleared; }
        sigc::signal<void> signal_load_success() const { return m_SignalLoadSuccess; }
        sigc::signal<void> signal_size_changed() const { return m_SignalSizeChanged; }
        sigc::signal<void> signal_thumbnails_loaded() const { return m_SignalThumbnailsLoaded; }

    protected:
        virtual void load_thumbnails();
        virtual void cancel_thumbnail_thread();
        void update_cache();

        virtual void on_thumbnail_loaded();

        Widget* const m_Widget;
        ImageVector m_Images;
        size_t m_Index{ 0 };

        ScrollPos m_ScrollPos;

        Glib::RefPtr<Gio::Cancellable> m_ThumbnailCancel;
        std::thread m_ThumbnailThread;
        ThreadPool m_ThreadPool;
        TSQueue<PixbufPair> m_ThumbnailQueue;
        std::atomic<size_t> m_ThumbnailsLoading{ 0 }, m_ThumbnailsLoaded{ 0 };

        SignalChangedType m_SignalChanged;
        SignalLoadProgressType m_SignalLoadProgress;
        sigc::signal<void> m_SignalCleared;

    private:
        void reset();
        template<typename T>
        std::vector<std::string> get_entries(const std::string& path) const;

        void on_directory_changed(const Glib::RefPtr<Gio::File>& file,
                                  const Glib::RefPtr<Gio::File>&,
                                  Gio::FileMonitorEvent event);

        void set_current_relative(const int d);
        void cancel_cache();

        // Indicies of the Images in the current cache
        std::vector<size_t> m_Cache;
        // A queue of Images that need to be loaded
        TSQueue<std::shared_ptr<Image>> m_CacheQueue;
        std::unique_ptr<Archive> m_Archive;
        std::vector<std::string> m_ArchiveEntries;
        std::function<int(size_t, size_t)> m_IndexSort;

        Glib::RefPtr<Gio::Cancellable> m_CacheCancel;
        std::atomic<bool> m_CacheStop{ false };
        std::condition_variable m_CacheCond;
        std::mutex m_CacheMutex, m_ThumbnailMutex;
        std::thread m_CacheThread;
        Glib::RefPtr<Gio::FileMonitor> m_FileMonitor;

        Glib::Dispatcher m_SignalThumbnailLoaded;

        sigc::connection m_ThumbnailLoadedConn;

        SignalArchiveErrorType m_SignalArchiveError;
        sigc::signal<void> m_SignalLoadSuccess, m_SignalSizeChanged, m_SignalThumbnailsLoaded;
    };
}
