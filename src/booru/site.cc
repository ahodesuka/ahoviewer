#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>

#include "site.h"
using namespace AhoViewer::Booru;

#include "settings.h"
#include "tempdir.h"

// 1: page, 2: limit, 3: tags
const std::map<Site::Type, std::string> Site::RequestURI =
{
    { Type::DANBOORU, "/post/index.xml?page=%1&limit=%2&tags=%3" },
    { Type::GELBOORU, "/index.php?page=dapi&s=post&q=index&pid=%1&limit=%2&tags=%3" },
};

const Glib::RefPtr<Gdk::Pixbuf>& Site::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::Invisible().render_icon(Gtk::Stock::MISSING_IMAGE, Gtk::ICON_SIZE_MENU);

    return pixbuf;
}

Site::Type Site::string_to_type(std::string type)
{
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "gelbooru")
         return Type::GELBOORU;

    return Type::DANBOORU;
}

Site::Site(std::string name, std::string url, Type type)
  : m_Name(name),
    m_Url(url),
    m_IconPath(Glib::build_filename(Settings.BooruPath, m_Name + ".png")),
    m_TagsPath(Glib::build_filename(Settings.BooruPath, m_Name + "-tags")),
    m_Type(type),
    m_Curl(new Curler),
    m_IconCurlerThread(nullptr)
{
    // Load tags
    if (Glib::file_test(m_TagsPath, Glib::FILE_TEST_EXISTS))
    {
        std::ifstream ifs(m_TagsPath);
        if (ifs)
        {
            std::copy(std::istream_iterator<std::string>(ifs),
                      std::istream_iterator<std::string>(),
                      std::back_inserter(m_Tags));
        }
    }
}

Site::~Site()
{
    if (m_IconCurlerThread)
    {
        m_IconCurlerThread->join();
        m_IconCurlerThread = nullptr;
    }

    delete m_Curl;
}

pugi::xml_node Site::download_posts(const std::string &tags, size_t page)
{
    pugi::xml_document doc;
    std::string url(Glib::ustring::compose(m_Url + RequestURI.at(m_Type),
                                           (m_Type == Type::GELBOORU ? page - 1 : page),
                                           Settings.get_int("BooruLimit"), m_Curl->escape(tags)));

    m_Curl->set_url(url);
    if (m_Curl->perform())
    {
        doc.load_buffer(m_Curl->get_data(), m_Curl->get_data_size());
    }
    else
    {
        std::cerr << "Error while downloading posts on " << url << std::endl
                  << "  " << m_Curl->get_error() << std::endl;
    }

    return doc.document_element();
}

std::string Site::get_path()
{
    if (m_Path.empty())
    {
        m_Path = TempDir::get_instance().make_dir(m_Name);
        g_mkdir_with_parents(Glib::build_filename(m_Path, "thumbnails").c_str(), 0755);
    }

    return m_Path;
}

void Site::save_tags() const
{
    std::ofstream ofs(m_TagsPath);

    if (ofs)
        std::copy(m_Tags.begin(), m_Tags.end(), std::ostream_iterator<std::string>(ofs, "\n"));
}

void Site::set_row_values(Gtk::TreeModel::Row row)
{
    row.set_value(1, m_Name);
    if (Glib::file_test(m_IconPath, Glib::FILE_TEST_EXISTS))
    {
        m_IconPixbuf = Gdk::Pixbuf::create_from_file(m_IconPath);
        row.set_value(0, m_IconPixbuf);
    }
    else
    {
        m_IconPixbuf = get_missing_pixbuf();
        // Attempt to download the site's favicon
        m_IconCurlerThread = Glib::Threads::Thread::create([ this, row ]()
        {
            for (const std::string &url : { m_Url + "/favicon.ico",
                                            m_Url + "/favicon.png" })
            {
                m_Curl->set_url(url);
                if (m_Curl->perform())
                {
                    Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create();
                    loader->set_size(16, 16);

                    try
                    {
                        loader->write(m_Curl->get_data(), m_Curl->get_data_size());
                        loader->close();
                        m_IconPixbuf = loader->get_pixbuf();
                        m_IconPixbuf->save(m_IconPath, "png");
                        break;
                    }
                    catch (const Gdk::PixbufError &ex)
                    {
                        std::cerr << "Error while creating icon for " << m_Name
                                  << ": " << std::endl << "  " << ex.what() << std::endl;
                    }
                }
            }

            row.set_value(0, m_IconPixbuf);
        });
    }
}
