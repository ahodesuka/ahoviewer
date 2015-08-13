#include <fstream>
#include <iostream>

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

std::shared_ptr<Site> Site::create(const std::string &name, const std::string &url)
{
    Type t = get_type_from_url(url);

    if (t != Type::UNKNOWN)
        return std::make_shared<Site>(name, url, t);

    return nullptr;
}

const Glib::RefPtr<Gdk::Pixbuf>& Site::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::Invisible().render_icon(Gtk::Stock::MISSING_IMAGE, Gtk::ICON_SIZE_MENU);

    return pixbuf;
}

Site::Type Site::get_type_from_url(const std::string &url)
{
    Curler curler;
    curler.set_no_body();

    for (const Type &type : { Type::GELBOORU, Type::DANBOORU })
    {
        std::string uri(RequestURI.at(type));
        std::string s(url + uri.substr(0, uri.find("?")));

        curler.set_url(s);
        if (curler.perform())
            return type;
    }

    return Type::UNKNOWN;
}

Site::Site(std::string name, std::string url, Type type)
  : m_Name(name),
    m_Url(url),
    m_IconPath(Glib::build_filename(Settings.get_booru_path(), m_Name + ".png")),
    m_TagsPath(Glib::build_filename(Settings.get_booru_path(), m_Name + "-tags")),
    m_Type(type),
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
                      std::inserter(m_Tags, m_Tags.begin()));
        }
    }
}

Site::~Site()
{
    m_Curler.cancel();
    if (m_IconCurlerThread)
    {
        m_IconCurlerThread->join();
        m_IconCurlerThread = nullptr;
    }
}

std::string Site::get_posts_url(const std::string &tags, size_t page)
{
    return Glib::ustring::compose(m_Url + RequestURI.at(m_Type),
                                  (m_Type == Type::GELBOORU ? page - 1 : page),
                                  Settings.get_int("BooruLimit"), m_Curler.escape(tags));
}

void Site::add_tags(const std::set<std::string> &tags)
{
    m_Tags.insert(tags.begin(), tags.end());
}

bool Site::set_url(const std::string &url)
{
    if (url != m_Url)
    {
        Type type = get_type_from_url(url);

        if (type == Type::UNKNOWN)
            return false;

        m_Url  = url;
        m_Type = type;
    }

    return true;
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

Glib::RefPtr<Gdk::Pixbuf> Site::get_icon_pixbuf(const bool update)
{
    if (!m_IconPixbuf || update)
    {
        if (!update && Glib::file_test(m_IconPath, Glib::FILE_TEST_EXISTS))
        {
            m_IconPixbuf = Gdk::Pixbuf::create_from_file(m_IconPath);
        }
        else
        {
            m_IconPixbuf = get_missing_pixbuf();
            // Attempt to download the site's favicon
            m_IconCurlerThread = Glib::Threads::Thread::create([ this ]()
            {
                for (const std::string &url : { m_Url + "/favicon.ico",
                                                m_Url + "/favicon.png" })
                {
                    m_Curler.set_url(url);
                    if (m_Curler.perform())
                    {
                        Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create();
                        loader->set_size(16, 16);

                        try
                        {
                            loader->write(m_Curler.get_data(), m_Curler.get_data_size());
                            loader->close();
                            m_IconPixbuf = loader->get_pixbuf();
                            m_IconPixbuf->save(m_IconPath, "png");
                            m_SignalIconDownloaded();
                            break;
                        }
                        catch (const Gdk::PixbufError &ex)
                        {
                            std::cerr << "Error while creating icon for " << m_Name
                                      << ": " << std::endl << "  " << ex.what() << std::endl;
                        }
                    }
                }
            });

            if (update)
            {
                m_IconCurlerThread->join();
                m_IconCurlerThread = nullptr;
            }
        }
    }

    return m_IconPixbuf;
}

void Site::save_tags() const
{
    std::ofstream ofs(m_TagsPath);

    if (ofs)
        std::copy(m_Tags.begin(), m_Tags.end(), std::ostream_iterator<std::string>(ofs, "\n"));
}

void Site::set_row_values(Gtk::TreeRow row)
{
    m_IconDownloadedConn.disconnect();
    m_IconDownloadedConn = m_SignalIconDownloaded.connect([ this, row ]() { row.set_value(0, m_IconPixbuf); });

    row.set_value(0, get_icon_pixbuf());
    row.set_value(1, m_Name);
}
