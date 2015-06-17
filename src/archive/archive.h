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
        // Progress signal to show the progress in the status bar
        typedef sigc::signal<void, size_t, size_t> SignalProgressType;

        enum class Type
        {
            UNKNOWN,
            ZIP,
            RAR,
        };

        // An interface for archive extractors.
        // Extractors implement the extract method which extracts all files
        // from the archive while emitting the progress signal, and
        // returns the path to the extracted files.
        class Extractor
        {
        public:
            virtual ~Extractor() = default;
            virtual std::string extract(const std::string&, const std::shared_ptr<Archive>&) const = 0;

            SignalProgressType signal_progress() const
            {
                return m_SignalProgress;
            }
        protected:
            SignalProgressType m_SignalProgress;
        };

        // Simple Image class that overrides get_filename and get_thumbnail
        class Image : public AhoViewer::Image
        {
        public:
            Image(const std::string &path, const std::shared_ptr<Archive> archive);
            virtual std::string get_filename() const;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();
        private:
            std::shared_ptr<Archive> m_Archive;
        };

        // use ::create instead.
        explicit Archive(const std::string &path, const std::string &extractedPath);
        ~Archive();

        static bool is_valid(const std::string &path);
        static std::shared_ptr<Archive> create(const std::string &path,
                                               const std::shared_ptr<Archive> &parent = nullptr);

        const std::string get_path() const { return m_Path; }
        const std::string get_extracted_path() const { return m_ExtractedPath; }

        static const std::map<Type, const Extractor *const> Extractors;
        static const std::vector<std::string> MimeTypes;
    private:
        static Type get_type(const std::string &path);

        // Matches the largest extractor MagicSize
        static int const MagicSize = 6;

        std::string m_Path, m_ExtractedPath;
        std::vector<std::shared_ptr<Archive>> m_Children;
    };
}

#endif /* _ARCHIVE_H_ */
