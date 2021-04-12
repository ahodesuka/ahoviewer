#pragma once

#include "../image.h"

#include <functional>
#include <sigc++/sigc++.h>

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

        // Simple Image class that represents a image from an archive
        class Image final : public AhoViewer::Image
        {
        public:
            Image(const std::string& file, const Archive& archive);
            std::string get_filename() const override;
            const Glib::RefPtr<Gdk::Pixbuf>&
            get_thumbnail(Glib::RefPtr<Gio::Cancellable> c) override;
            void load_pixbuf(Glib::RefPtr<Gio::Cancellable> c) override;

            void save(const std::string& path);

        private:
            void extract_file();

            // The path to the image file inside of m_Archive
            std::string m_ArchiveFilePath;
            const Archive& m_Archive;
        };

        virtual ~Archive();

        static bool is_valid(const std::string& path);
        static bool is_valid_extension(const std::string& path);
        static std::unique_ptr<Archive> create(const std::string& path);

        virtual bool extract(const std::string& file) const                  = 0;
        virtual bool has_valid_files(const FileType t) const                 = 0;
        virtual std::vector<std::string> get_entries(const FileType t) const = 0;

        const std::string get_path() const { return m_Path; }
        const std::string get_extracted_path() const { return m_ExtractedPath; }

        static const std::vector<std::string> MimeTypes, FileExtensions;

    protected:
        Archive(std::string path, std::string ex_dir);

        std::string m_Path, m_ExtractedPath;

    private:
        static Type get_type(const std::string& path);

        // Matches the largest archive MagicSize
        static constexpr int MagicSize{ 6 };
    };
}
