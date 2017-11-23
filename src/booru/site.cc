#include <chrono>
#include <fstream>
#include <iostream>
#include <glib/gstdio.h>

#include "site.h"
using namespace AhoViewer::Booru;

#include "settings.h"

#ifdef HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif // HAVE_LIBSECRET

// 1: page, 2: limit, 3: tags
const std::map<Site::Type, std::string> Site::RequestURI =
{
    { Type::DANBOORU, "/post/index.xml?page=%1&limit=%2&tags=%3" },
    { Type::GELBOORU, "/index.php?page=dapi&s=post&q=index&pid=%1&limit=%2&tags=%3" },
    { Type::MOEBOORU, "/post.xml?page=%1&limit=%2&tags=%3" },
};

// 1: id
const std::map<Site::Type, std::string> Site::PostURI =
{
    { Type::DANBOORU, "/posts/%1" },
    { Type::GELBOORU, "/index.php?page=post&s=view&id=%1" },
    { Type::MOEBOORU, "/post/show/%1" },
};

const std::map<Site::Type, std::string> Site::RegisterURI =
{
    { Type::DANBOORU, "/users/new" },
    { Type::GELBOORU, "/index.php?page=account&s=reg" },
    { Type::MOEBOORU, "/user/signup" },
};

struct _shared_site : public Site
{
    template<typename...Ts>
    _shared_site(Ts&&...args) : Site(std::forward<Ts>(args)...) { }
};

std::shared_ptr<Site> Site::create(const std::string &name, const std::string &url, const Type type,
                                   const std::string &user, const std::string &pass)
{
    Type t = type == Type::UNKNOWN ? get_type_from_url(url) : type;

    if (t != Type::UNKNOWN)
        return std::make_shared<_shared_site>(name, url, t, user, pass);

    return nullptr;
}

const Glib::RefPtr<Gdk::Pixbuf>& Site::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::Invisible().render_icon(Gtk::Stock::MISSING_IMAGE, Gtk::ICON_SIZE_MENU);

    return pixbuf;
}

#ifdef HAVE_LIBSECRET
void Site::on_password_lookup(GObject*, GAsyncResult *result, gpointer ptr)
{
    GError *error = NULL;
    gchar *password = secret_password_lookup_finish(result, &error);
    Site *s = static_cast<Site*>(ptr);

    if (!error && password)
    {
        s->m_Password = password;
        s->m_PasswordLookup();
        secret_password_free(password);
    }
    else if (error)
    {
        std::cerr << "Failed to lookup password for " << s->get_name() << std::endl
                  << "  " << error->message << std::endl;
        g_error_free(error);
    }
}

void Site::on_password_stored(GObject*, GAsyncResult *result, gpointer ptr)
{
    GError *error = NULL;
    secret_password_store_finish(result, &error);

    if (error)
    {
        Site *s = static_cast<Site*>(ptr);
        std::cerr << "Failed to set password for " << s->get_name() << std::endl
                  << "  " << error->message << std::endl;
        g_error_free(error);
    }
}
#endif // HAVE_LIBSECRET

Site::Type Site::get_type_from_url(const std::string &url)
{
    Curler curler;
    curler.set_no_body();
    curler.set_follow_location(false);

    for (const Type &type : { Type::GELBOORU, Type::MOEBOORU, Type::DANBOORU })
    {
        std::string uri = RequestURI.at(type);

        if (type == Type::GELBOORU)
            uri = uri.substr(0, uri.find("&pid"));
        else
            uri = uri.substr(0, uri.find("?"));

        curler.set_url(url + uri);
        if (curler.perform() && curler.get_response_code() == 200)
            return type;
    }

    return Type::UNKNOWN;
}

Site::Site(const std::string &name, const std::string &url, const Type type,
           const std::string &user, const std::string &pass)
  : m_Name(name),
    m_Url(url),
    m_Username(user),
    m_Password(pass),
    m_IconPath(Glib::build_filename(Settings.get_booru_path(), m_Name + ".png")),
    m_TagsPath(Glib::build_filename(Settings.get_booru_path(), m_Name + "-tags")),
    m_CookiePath(Glib::build_filename(Settings.get_booru_path(), m_Name + "-cookie")),
    m_Type(type),
    m_NewAccount(false),
    m_CookieTS(0)
{
#ifdef HAVE_LIBSECRET
    if (!m_Username.empty())
        secret_password_lookup(SECRET_SCHEMA_COMPAT_NETWORK,
                               NULL,
                               &Site::on_password_lookup, this,
                               "user", m_Username.c_str(),
                               "server", m_Url.c_str(),
                               NULL);
#endif // HAVE_LIBSECRET

    // Load tags
    if (Glib::file_test(m_TagsPath, Glib::FILE_TEST_EXISTS))
    {
        std::ifstream ifs(m_TagsPath);

        if (ifs)
            std::copy(std::istream_iterator<std::string>(ifs),
                      std::istream_iterator<std::string>(),
                      std::inserter(m_Tags, m_Tags.begin()));
    }
}

Site::~Site()
{
    m_Curler.cancel();
    if (m_IconCurlerThread.joinable())
        m_IconCurlerThread.join();

    cleanup_cookie();
}

std::string Site::get_posts_url(const std::string &tags, size_t page)
{
    return Glib::ustring::compose(m_Url + RequestURI.at(m_Type),
                                  (m_Type == Type::GELBOORU ? page - 1 : page),
                                  Settings.get_int("BooruLimit"), tags);
}

std::string Site::get_post_url(const std::string &id)
{
    return Glib::ustring::compose(m_Url + PostURI.at(m_Type), id);
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

void Site::set_password(const std::string &s)
{
    m_NewAccount = true;
#ifdef HAVE_LIBSECRET
    if (!m_Username.empty())
        secret_password_store(SECRET_SCHEMA_COMPAT_NETWORK,
                              SECRET_COLLECTION_DEFAULT,
                              "password", s.c_str(),
                              NULL,
                              &Site::on_password_stored, this,
                              "user", m_Username.c_str(),
                              "server", m_Url.c_str(),
                              NULL);
#endif // HAVE_LIBSECRET
    m_Password = s;
}

// FIXME: Hopefully Gelbooru implements basic HTTP Authentication
std::string Site::get_cookie()
{
    if (m_Type != Type::GELBOORU)
        return "";

    if (!m_Username.empty() && !m_Password.empty())
    {
        using namespace std::chrono;
        uint64_t cts = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

        if (cts >= m_CookieTS || m_NewAccount)
        {
            // Get cookie expiration timestamp
            if (Glib::file_test(m_CookiePath, Glib::FILE_TEST_EXISTS))
            {
                std::ifstream ifs(m_CookiePath);
                std::string line, tok;

                while (std::getline(ifs, line))
                {
                    if (line.compare(0, 10, "#HttpOnly_") == 0)
                        line = line.substr(1);

                    // Skip comments
                    if (line[0] == '#') continue;

                    std::istringstream iss(line);
                    size_t i = 0;

                    while (std::getline(iss, tok, '\t'))
                    {
                        // Timestamp is always at the 4th index
                        if (i == 4)
                        {
                            char *after = nullptr;
                            uint64_t ts = strtoull(tok.c_str(), &after, 10);

                            if (ts < m_CookieTS || m_CookieTS == 0)
                                m_CookieTS = ts;
                        }

                        ++i;
                    }
                }
            }

            // Login and get a new cookie
            // This does not check whether or not the login succeeded.
            if (cts >= m_CookieTS || m_NewAccount)
            {
                if (Glib::file_test(m_CookiePath, Glib::FILE_TEST_EXISTS))
                    g_unlink(m_CookiePath.c_str());

                Curler curler(m_Url + "/index.php?page=account&s=login&code=00");
                std::string f = Glib::ustring::compose("user=%1&pass=%2", m_Username, m_Password);

                curler.set_cookie_jar(m_CookiePath);
                curler.set_post_fields(f);
                curler.perform();

                m_NewAccount = false;
            }
        }
    }

    return m_CookiePath;
}

void Site::cleanup_cookie() const
{
    if ((m_Username.empty() || m_Password.empty() || m_NewAccount) &&
            Glib::file_test(m_CookiePath, Glib::FILE_TEST_EXISTS))
        g_unlink(m_CookiePath.c_str());
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
            m_IconCurlerThread = std::thread([ this ]()
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
                            m_SignalIconDownloaded();
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
            });

            if (update)
                m_IconCurlerThread.join();
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
