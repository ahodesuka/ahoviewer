#ifndef _RAR_H_
#define _RAR_H_

#include "archive.h"

namespace AhoViewer
{
    class Rar : public Archive
    {
    public:
        Rar(const Glib::ustring &path, const Glib::ustring &exDir);
        virtual ~Rar() override = default;

        virtual bool extract(const Glib::ustring &file) const override;
        virtual bool has_valid_files(const FileType t) const override;
        virtual std::vector<std::string> get_entries(const FileType t) const override;

        static const int MagicSize = 6;
        static const char Magic[MagicSize];
    };
}

#endif /* _RAR_H_ */
