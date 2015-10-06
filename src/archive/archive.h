#ifndef _ARCHIVE_H_
#define _ARCHIVE_H_

#include <functional>
#include <sigc++/sigc++.h>

#include "../image.h"

namespace AhoViewer
{
    // Progress signal to show extractor progress in the status bar
    typedef sigc::signal<void, size_t, size_t> SignalExtractorProgressType;

    class Archive
    {
    public:
        enum class Type
        {
            UNKNOWN,
            ZIP,
            RAR,
        };

        enum FileType
        {
            IMAGES   = (1u << 0),
            ARCHIVES = (1u << 1),
        };

        // Simple Image class that overrides get_filename and get_thumbnail
        class Image : public AhoViewer::Image
        {
        public:
            Image(const std::string &path, const Archive &archive);
            virtual std::string get_filename() const;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();
        private:
            const Archive &m_Archive;
        };

        // use ::create instead.
        explicit Archive(const std::string &path, const std::string &exDir, const std::string &parentDir);
        virtual ~Archive();

        virtual void extract() = 0;
        virtual bool has_valid_files(const FileType t = static_cast<FileType>(IMAGES | ARCHIVES)) const = 0;
        virtual size_t get_n_valid_files(const FileType t = static_cast<FileType>(IMAGES | ARCHIVES)) const = 0;

        static bool is_valid(const std::string &path);
        static bool is_valid_extension(const std::string &path);
        static std::unique_ptr<Archive> create(const std::string &path, const std::string &parentDir = "");

        const std::string get_path() const { return m_Path; }
        const std::string get_extracted_path() const { return m_ExtractedPath; }
        SignalExtractorProgressType signal_progress() { return m_SignalProgress; }

        void add_child(std::unique_ptr<Archive> &c) { m_Children.push_back(std::move(c)); }

        static const std::vector<std::string> MimeTypes, FileExtensions;
    protected:
        std::string m_Path, m_ExtractedPath;
        bool m_Child, m_Extracted;

        SignalExtractorProgressType m_SignalProgress;
    private:
        static Type get_type(const std::string &path);

        // Matches the largest archive MagicSize
        static const int MagicSize = 6;

        std::vector<std::unique_ptr<Archive>> m_Children;
    };
}

#endif /* _ARCHIVE_H_ */
