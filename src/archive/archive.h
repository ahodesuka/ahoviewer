#ifndef _ARCHIVE_H_
#define _ARCHIVE_H_

#include <functional>
#include <sigc++/sigc++.h>

#include "../image.h"

namespace AhoViewer
{
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
            IMAGES   = (1 << 0),
            ARCHIVES = (1 << 1),
        };

        // Simple Image class that overrides get_filename and get_thumbnail
        class Image : public AhoViewer::Image
        {
        public:
            Image(const std::string &file, const Archive &archive);
            virtual std::string get_filename() const override;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail() override;
            virtual void load_pixbuf() override;
        private:
            void extract_file();

            // The path to the image file inside of m_Archive
            std::string m_ArchiveFilePath;
            const Archive &m_Archive;
        };

        virtual ~Archive();

        static bool is_valid(const std::string &path);
        static bool is_valid_extension(const std::string &path);
        static std::unique_ptr<Archive> create(const std::string &path, const std::string &parentDir = "");

        virtual bool extract(const std::string &file) const = 0;
        virtual bool has_valid_files(const FileType t) const = 0;
        virtual std::vector<std::string> get_entries(const FileType t) const = 0;

        const std::string get_path() const { return m_Path; }
        const std::string get_extracted_path() const { return m_ExtractedPath; }

        static const std::vector<std::string> MimeTypes, FileExtensions;
    protected:
        Archive(const std::string &path, const std::string &exDir);

        std::string m_Path, m_ExtractedPath;
    private:
        static Type get_type(const std::string &path);

        // Matches the largest archive MagicSize
        static const int MagicSize = 6;
    };
}

#endif /* _ARCHIVE_H_ */
