#include "site.h"

#include <chrono>
#include <fstream>
#include <glib/gstdio.h>
#include <gtkmm.h>
#include <iostream>
#include <utility>

using namespace AhoViewer::Booru;

#include "settings.h"

#ifdef HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif // HAVE_LIBSECRET

#ifdef _WIN32
#include <wincred.h>
#endif // _WIN32

// 1: page, 2: limit, 3: tags
const std::map<Type, std::string> Site::RequestURI = {
    { Type::DANBOORU_V2, "/posts.xml?page=%1&limit=%2&tags=%3" },
    { Type::GELBOORU, "/index.php?page=dapi&s=post&q=index&pid=%1&limit=%2&tags=%3" },
    { Type::MOEBOORU, "/post.xml?page=%1&limit=%2&tags=%3" },
    { Type::SHIMMIE, "/api/danbooru/find_posts/index.xml?page=%1&limit=%2&tags=%3" },
    { Type::DANBOORU, "/post/index.xml?page=%1&limit=%2&tags=%3" },
};

// 1: id
const std::map<Type, std::string> Site::PostURI = {
    { Type::DANBOORU_V2, "/posts/%1" },  { Type::GELBOORU, "/index.php?page=post&s=view&id=%1" },
    { Type::MOEBOORU, "/post/show/%1" }, { Type::SHIMMIE, "/post/view/%1" },
    { Type::DANBOORU, "/posts/%1" },
};

// 1: id
const std::map<Type, std::string> Site::NotesURI = {
    { Type::DANBOORU_V2, "/notes.xml?group_by=note&search[post_id]=%1" },
    { Type::GELBOORU, "/index.php?page=dapi&s=note&q=index&post_id=%1" },
    { Type::MOEBOORU, "/note.xml?post_id=%1" },
    { Type::SHIMMIE, "" },
    { Type::DANBOORU, "/note/index.xml?post_id=%1" },
};

const std::map<Type, std::string> Site::RegisterURI = {
    { Type::DANBOORU_V2, "/users/new" }, { Type::GELBOORU, "/index.php?page=account&s=reg" },
    { Type::MOEBOORU, "/user/signup" },  { Type::SHIMMIE, "/user_admin/create" },
    { Type::DANBOORU, "/users/new" },
};

// This is a workaround to have a private/protected constructor
// and still be able to use make_shared.
// Site's constructor shouldn't be called directly that's why it's protected.
// Site::create should be used since it will let us know if the site is actually
// valid or not
namespace
{
    struct SharedSite : public Site
    {
        template<typename... Args>
        SharedSite(Args&&... v) : Site(std::forward<Args>(v)...)
        {
        }
    };
}

std::shared_ptr<Site> Site::create(const std::string& name,
                                   const std::string& url,
                                   const Type type,
                                   const std::string& user,
                                   const std::string& pass,
                                   const unsigned int max_cons,
                                   const bool use_samples)
{
    Type t = type == Type::UNKNOWN ? get_type_from_url(url) : type;

    if (t != Type::UNKNOWN)
        return std::make_shared<SharedSite>(name, url, t, user, pass, max_cons, use_samples);

    return nullptr;
}

const Glib::RefPtr<Gdk::Pixbuf>& Site::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gtk::IconTheme::get_default()->load_icon(
        "image-missing", 16, Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_GENERIC_FALLBACK);

    return pixbuf;
}

#ifdef HAVE_LIBSECRET
void Site::on_password_lookup(GObject*, GAsyncResult* result, gpointer ptr)
{
    GError* error   = nullptr;
    gchar* password = secret_password_lookup_finish(result, &error);
    Site* s         = static_cast<Site*>(ptr);

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

void Site::on_password_stored(GObject*, GAsyncResult* result, gpointer ptr)
{
    GError* error = nullptr;
    secret_password_store_finish(result, &error);

    if (error)
    {
        Site* s = static_cast<Site*>(ptr);
        std::cerr << "Failed to set password for " << s->get_name() << std::endl
                  << "  " << error->message << std::endl;
        g_error_free(error);
    }
}
#endif // HAVE_LIBSECRET

Type Site::get_type_from_url(const std::string& url)
{
    Curler curler;
    curler.set_no_body();
    curler.set_follow_location(false);

    for (const Type& type :
         { Type::DANBOORU_V2, Type::GELBOORU, Type::MOEBOORU, Type::DANBOORU, Type::SHIMMIE })
    {
        std::string uri{ RequestURI.at(type) };

        if (type == Type::GELBOORU)
            uri = uri.substr(0, uri.find("&pid"));
        else
            uri = uri.substr(0, uri.find("?"));

        curler.set_url(url + uri);
        // Danbooru V2 returns a 204 (no content because set_no_body?)
        if (curler.perform() && (curler.get_response_code() == 200 ||
                                 (type == Type::DANBOORU_V2 && curler.get_response_code() == 204)))
            return type;
    }

    return Type::UNKNOWN;
}

void Site::share_lock_cb(CURL*, curl_lock_data data, curl_lock_access, void* userp)
{
    static_cast<Site*>(userp)->m_MutexMap[data].lock();
}

void Site::share_unlock_cb(CURL*, curl_lock_data data, void* userp)
{
    static_cast<Site*>(userp)->m_MutexMap[data].unlock();
}

Site::Site(std::string name,
           std::string url,
           const Type type,
           std::string user,
           std::string pass,
           const int max_cons,
           const bool use_samples)
    : m_Name{ std::move(name) },
      m_Url{ std::move(url) },
      m_Username{ std::move(user) },
      m_Password{ std::move(pass) },
      m_IconPath{ Glib::build_filename(Settings.get_booru_path(), m_Name + ".png") },
      m_TagsPath{ Glib::build_filename(Settings.get_booru_path(), m_Name + "-tags") },
      m_CookiePath{ Glib::build_filename(Settings.get_booru_path(), m_Name + "-cookie") },
      m_Type{ type },
      m_UseSamples{ use_samples },
      m_MaxConnections{ max_cons },
      m_ShareHandle{ curl_share_init() }
{
    curl_share_setopt(m_ShareHandle, CURLSHOPT_LOCKFUNC, &Site::share_lock_cb);
    curl_share_setopt(m_ShareHandle, CURLSHOPT_UNLOCKFUNC, &Site::share_unlock_cb);
    curl_share_setopt(m_ShareHandle, CURLSHOPT_USERDATA, this);

    // Types of data to share between curlers for this site
    for (auto d : {
// CURL_LOCK_DATA_CONNECT is broken in 7.57.0
#if LIBCURL_VERSION_NUM != 0x073900
             CURL_LOCK_DATA_CONNECT,
#endif
                 CURL_LOCK_DATA_COOKIE, CURL_LOCK_DATA_DNS, CURL_LOCK_DATA_SSL_SESSION,
                 CURL_LOCK_DATA_SHARE
         })
    {
        m_MutexMap.emplace(
            std::piecewise_construct, std::forward_as_tuple(d), std::forward_as_tuple());
        if (d != CURL_LOCK_DATA_SHARE)
            curl_share_setopt(m_ShareHandle, CURLSHOPT_SHARE, d);
    }
    m_Curler.set_share_handle(m_ShareHandle);

#ifdef HAVE_LIBSECRET
    if (!m_Username.empty())
        secret_password_lookup(SECRET_SCHEMA_COMPAT_NETWORK,
                               nullptr,
                               &Site::on_password_lookup,
                               this,
                               "user",
                               m_Username.c_str(),
                               "server",
                               m_Url.c_str(),
                               NULL);
#endif // HAVE_LIBSECRET

#ifdef _WIN32
    if (!m_Username.empty())
    {
        PCREDENTIALW pcred;

        std::string target = std::string(PACKAGE "/") + m_Name;
        wchar_t* TargetName =
            reinterpret_cast<wchar_t*>(g_utf8_to_utf16(target.c_str(), -1, NULL, NULL, NULL));

        if (TargetName)
        {
            BOOL r = CredReadW(TargetName, CRED_TYPE_GENERIC, 0, &pcred);

            if (!r)
            {
                std::cerr << "Failed to read password for " << m_Name << std::endl
                          << " errno " << GetLastError() << std::endl;
            }
            else
            {
                wchar_t* UserName = reinterpret_cast<wchar_t*>(
                    g_utf8_to_utf16(m_Username.c_str(), -1, NULL, NULL, NULL));

                if (UserName)
                {
                    if (wcscmp(pcred->UserName, UserName) == 0)
                        m_Password = (char*)pcred->CredentialBlob;

                    g_free(UserName);
                }

                CredFree(pcred);
            }

            g_free(TargetName);
        }
    }
#endif // _WIN32

    // Load tags
    if (Glib::file_test(m_TagsPath, Glib::FILE_TEST_EXISTS))
    {
        std::ifstream ifs(m_TagsPath);

        if (ifs)
            std::copy(std::istream_iterator<Tag>(ifs),
                      std::istream_iterator<Tag>(),
                      std::inserter(m_Tags, m_Tags.begin()));
    }
}

Site::~Site()
{
    m_Curler.cancel();
    if (m_IconCurlerThread.joinable())
        m_IconCurlerThread.join();

    cleanup_cookie();
    curl_share_cleanup(m_ShareHandle);
}

std::string Site::get_posts_url(const std::string& tags, size_t page)
{
    return Glib::ustring::compose(m_Url + RequestURI.at(m_Type),
                                  (m_Type == Type::GELBOORU ? page - 1 : page),
                                  Settings.get_int("BooruLimit"),
                                  tags);
}

std::string Site::get_post_url(const std::string& id)
{
    return Glib::ustring::compose(m_Url + PostURI.at(m_Type), id);
}

std::string Site::get_notes_url(const std::string& id)
{
    return Glib::ustring::compose(m_Url + NotesURI.at(m_Type), id);
}

void Site::add_tags(const std::vector<Tag>& tags)
{
    auto& favorite_tags{ Settings.get_favorite_tags() };
    // Add or update tags (type may have changed)
    for (const auto& t : tags)
    {
        auto it{ m_Tags.insert(t) };
        if (!it.second && t.type != Tag::Type::UNKNOWN)
            it.first->type = t.type;

        auto fav_it{ std::find(favorite_tags.begin(), favorite_tags.end(), t) };
        if (fav_it != favorite_tags.end() && t.type != Tag::Type::UNKNOWN)
            fav_it->type = t.type;
    }
}

bool Site::set_url(const std::string& url)
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

void Site::set_password(const std::string& s)
{
    m_NewAccount = true;
#ifdef HAVE_LIBSECRET
    if (!m_Username.empty())
        secret_password_store(SECRET_SCHEMA_COMPAT_NETWORK,
                              SECRET_COLLECTION_DEFAULT,
                              "password",
                              s.c_str(),
                              nullptr,
                              &Site::on_password_stored,
                              this,
                              "user",
                              m_Username.c_str(),
                              "server",
                              m_Url.c_str(),
                              NULL);
#endif // HAVE_LIBSECRET

#ifdef _WIN32
    if (!m_Username.empty())
    {
        std::string target = std::string(PACKAGE "/") + m_Name;
        wchar_t* TargetName =
            reinterpret_cast<wchar_t*>(g_utf8_to_utf16(target.c_str(), -1, NULL, NULL, NULL));

        if (TargetName)
        {
            wchar_t* UserName = reinterpret_cast<wchar_t*>(
                g_utf8_to_utf16(m_Username.c_str(), -1, NULL, NULL, NULL));

            if (UserName)
            {
                CREDENTIALW cred        = { 0 };
                cred.Type               = CRED_TYPE_GENERIC;
                cred.TargetName         = TargetName;
                cred.CredentialBlobSize = s.length();
                cred.CredentialBlob     = (LPBYTE)s.c_str();
                cred.Persist            = CRED_PERSIST_LOCAL_MACHINE;
                cred.UserName           = UserName;

                BOOL r = CredWriteW(&cred, 0);
                if (!r)
                    std::cerr << "Failed to set password for " << m_Name << std::endl
                              << " errno " << GetLastError() << std::endl;

                g_free(UserName);
            }

            g_free(TargetName);
        }
    }
#endif // _WIN32
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
                    if (line[0] == '#')
                        continue;

                    std::istringstream iss(line);
                    size_t i = 0;

                    while (std::getline(iss, tok, '\t'))
                    {
                        // Timestamp is always at the 4th index
                        if (i == 4)
                        {
                            char* after = nullptr;
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

                Curler curler(m_Url + "/index.php?page=account&s=login&code=00", m_ShareHandle);
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
            m_IconCurlerThread = std::thread([&]() {
                for (const std::string& url : { m_Url + "/favicon.ico", m_Url + "/favicon.png" })
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
                        catch (const Gdk::PixbufError& ex)
                        {
                            std::cerr << "Error while creating icon for " << m_Name << ": "
                                      << std::endl
                                      << "  " << ex.what() << std::endl;
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
        std::copy(m_Tags.begin(), m_Tags.end(), std::ostream_iterator<Tag>(ofs, "\n"));
}
