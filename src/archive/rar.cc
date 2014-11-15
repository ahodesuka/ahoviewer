#include <iostream>

#include "../config.h"
#include "../tempdir.h"

#ifdef HAVE_LIBUNRAR
#include "rar.h"
using namespace AhoViewer;

const char Rar::Magic[Rar::MagicSize] = { 'R', 'a', 'r', '!', 0x1A, 0x07 };

std::string Rar::extract(const std::string &path) const
{
    std::string extractedPath;
    size_t nExtracted = 0,
           nEntries = 0;

    RAROpenArchiveData archive;
    RARHeaderData header;
    memset(&archive, 0, sizeof(archive));

    archive.ArcName  = const_cast<char*>(path.c_str());
    archive.OpenMode = RAR_OM_LIST;

    HANDLE rar = RAROpenArchive(&archive);

    if (rar)
    {
        // Get number of entries for the progress signal
        while (RARReadHeader(rar, &header) == 0 && RARProcessFile(rar, RAR_SKIP, NULL, NULL) == 0)
            ++nEntries;

        RARCloseArchive(rar);

        // Change to extract mode and reopen
        archive.OpenMode = RAR_OM_EXTRACT;
        rar = RAROpenArchive(&archive);

        if (rar)
        {
            extractedPath = TempDir::get_instance().make_dir(Glib::path_get_basename(path));

            if (!extractedPath.empty())
            {
                while (RARReadHeader(rar, &header) == 0)
                {
                    RARProcessFile(rar, RAR_EXTRACT, const_cast<char*>(extractedPath.c_str()), NULL);
                    m_SignalExtractorProgress(++nExtracted, nEntries);
                }
            }

            RARCloseArchive(rar);
        }
    }

    return extractedPath;
}
#endif // HAVE_LIBUNRAR
