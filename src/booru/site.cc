#include "site.h"
using namespace AhoViewer::Booru;
using AhoViewer::Note;

#ifdef HAVE_LIBPEAS
using AhoViewer::Plugin::SitePlugin;
#endif // HAVE_LIBPEAS

#include "application.h"
using AhoViewer::Application;

extern "C"
{
#include "entities.h"
}
#include "image.h"
#include "settings.h"

#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <fstream>
#include <future>
#include <glib/gstdio.h>
#include <gtkmm.h>
#include <iostream>
#include <json.hpp>
#include <utility>

#ifdef HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif // HAVE_LIBSECRET

#ifdef _WIN32
#include <wincred.h>
#endif // _WIN32

// 1: page, 2: limit, 3: tags
const std::map<Type, std::string> Site::RequestURI{
    { Type::DANBOORU_V2, "/posts.xml?page=%1&limit=%2&tags=%3" },
    { Type::GELBOORU, "/index.php?page=dapi&s=post&q=index&pid=%1&limit=%2&tags=%3" },
    { Type::MOEBOORU, "/post.xml?page=%1&limit=%2&tags=%3" },
    { Type::DANBOORU, "/post/index.xml?page=%1&limit=%2&tags=%3" },
};

// 1: id
const std::map<Type, std::string> Site::PostURI{
    { Type::DANBOORU_V2, "/posts/%1" },
    { Type::GELBOORU, "/index.php?page=post&s=view&id=%1" },
    { Type::MOEBOORU, "/post/show/%1" },
    { Type::DANBOORU, "/post/show/%1" },
};

// 1: id
const std::map<Type, std::string> Site::NotesURI{
    { Type::DANBOORU_V2, "/notes.xml?group_by=note&search[post_id]=%1" },
    { Type::GELBOORU, "/index.php?page=dapi&s=note&q=index&post_id=%1" },
    { Type::MOEBOORU, "/note.xml?post_id=%1" },
    { Type::DANBOORU, "/note/index.xml?post_id=%1" },
};

const std::map<Type, std::string> Site::RegisterURI{
    { Type::DANBOORU_V2, "/users/new" },
    { Type::GELBOORU, "/index.php?page=account&s=reg" },
    { Type::MOEBOORU, "/user/signup" },
    { Type::DANBOORU, "/user/signup" },
};

static constexpr std::array<std::pair<int, Tag::Type>, 6> gb_types{ {
    { 0, Tag::Type::GENERAL },
    { 1, Tag::Type::ARTIST },
    { 3, Tag::Type::COPYRIGHT },
    { 4, Tag::Type::CHARACTER },
    { 5, Tag::Type::METADATA },
    { 6, Tag::Type::DEPRECATED },
} };
static constexpr std::array<std::pair<int, Tag::Type>, 6> yd_types{ {
    { 0, Tag::Type::GENERAL },
    { 1, Tag::Type::ARTIST },
    { 3, Tag::Type::COPYRIGHT },
    { 4, Tag::Type::CHARACTER },
    { 5, Tag::Type::COPYRIGHT }, // circle
    { 6, Tag::Type::METADATA },
} };
static constexpr std::array<std::pair<int, Tag::Type>, 6> kc_types{ {
    { 0, Tag::Type::GENERAL },
    { 1, Tag::Type::ARTIST },
    { 3, Tag::Type::COPYRIGHT },
    { 4, Tag::Type::CHARACTER },
    { 5, Tag::Type::METADATA },
    { 6, Tag::Type::COPYRIGHT }, // circle
} };

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
                                   Type type,
                                   const std::string& user,
                                   const std::string& pass,
                                   const bool use_samples)
{
    // When trying to create a new site from the site editor
    if (type == Type::UNKNOWN)
    {
#ifdef HAVE_LIBPEAS
        auto [t, p]{ get_type_from_url(url) };
        if (t != Type::UNKNOWN)
        {
            auto site{ std::make_shared<SharedSite>(name, url, t, user, pass, use_samples) };

            if (t == Type::PLUGIN)
                site->set_plugin(p);

            return site;
        }
#else  // !HAVE_LIBPEAS
        auto t{ get_type_from_url(url) };
        if (t != Type::UNKNOWN)
            return std::make_shared<SharedSite>(name, url, t, user, pass, use_samples);
#endif // !HAVE_LIBPEAS
    }
    else
    {
        return std::make_shared<SharedSite>(name, url, type, user, pass, use_samples);
    }

    return nullptr;
}

// Used when no site favicon can be loaded
const Glib::RefPtr<Gdk::Pixbuf>& Site::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gtk::IconTheme::get_default()->load_icon(
        "image-missing", 16, Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_GENERIC_FALLBACK);

    return pixbuf;
}

#ifdef HAVE_LIBSECRET
void Site::on_password_lookup(GObject*, GAsyncResult* result, gpointer ptr)
{
    GError* error{ nullptr };
    gchar* password{ secret_password_lookup_finish(result, &error) };
    auto* s{ static_cast<Site*>(ptr) };

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
    GError* error{ nullptr };
    secret_password_store_finish(result, &error);

    if (error)
    {
        auto* s{ static_cast<Site*>(ptr) };
        std::cerr << "Failed to set password for " << s->get_name() << std::endl
                  << "  " << error->message << std::endl;
        g_error_free(error);
    }
}
#endif // HAVE_LIBSECRET

#ifdef HAVE_LIBPEAS
std::pair<Type, std::shared_ptr<SitePlugin>>
#else  // !HAVE_LIBPEAS
Type
#endif // !HAVE_LIBPEAS
Site::get_type_from_url(const std::string& url)
{
    Type t{ Type::UNKNOWN };
#ifdef HAVE_LIBPEAS
    std::shared_ptr<SitePlugin> plugin{ nullptr };
#endif // !HAVE_LIBPEAS

    Curler curler;
    curler.set_follow_location(false);

    for (auto type :
         { Type::DANBOORU_V2, Type::GELBOORU, Type::MOEBOORU, Type::DANBOORU, Type::PLUGIN })
    {
        if (type != Type::PLUGIN)
        {
            auto uri{ Glib::ustring::compose(
                RequestURI.at(type), type == Type::GELBOORU ? 0 : 1, 1, "") };

            curler.set_url(url + uri);
            if (curler.perform() && curler.get_response_code() == 200)
            {
                try
                {
                    xml::Document xml{ reinterpret_cast<char*>(curler.get_data()),
                                       curler.get_data_size() };

                    // Make sure it actually returned posts xml, since html won't throw an exception
                    // above
                    if (xml.get_name() == "posts")
                    {
                        t = type;
                        break;
                    }
                }
                catch (const std::runtime_error&)
                {
                }
            }
        }
#ifdef HAVE_LIBPEAS
        else
        {
            for (const auto& site :
                 Application::get_default()->get_plugin_manager().get_site_plugins())
            {
                auto uri{ site->get_test_uri() };
                if (uri.empty())
                    continue;
                curler.set_url(url + uri);
                if (curler.perform() && curler.get_response_code() == 200)
                {
                    t      = type;
                    plugin = site;
                    break;
                }
            }
        }
#endif // !HAVE_LIBPEAS
    }

#ifdef HAVE_LIBPEAS
    return { t, plugin };
#else  // !HAVE_LIBPEAS
    return t;
#endif // !HAVE_LIBPEAS
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
      m_ShareHandle{ curl_share_init() }
{
    curl_share_setopt(m_ShareHandle, CURLSHOPT_LOCKFUNC, &Site::share_lock_cb);
    curl_share_setopt(m_ShareHandle, CURLSHOPT_UNLOCKFUNC, &Site::share_unlock_cb);
    curl_share_setopt(m_ShareHandle, CURLSHOPT_USERDATA, this);

    // Types of data to share between curlers for this site
    for (auto d : { CURL_LOCK_DATA_CONNECT,
                    CURL_LOCK_DATA_COOKIE,
                    CURL_LOCK_DATA_DNS,
                    CURL_LOCK_DATA_SSL_SESSION,
                    CURL_LOCK_DATA_SHARE })
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
        wchar_t* target_name =
            reinterpret_cast<wchar_t*>(g_utf8_to_utf16(target.c_str(), -1, NULL, NULL, NULL));

        if (target_name)
        {
            BOOL r = CredReadW(target_name, CRED_TYPE_GENERIC, 0, &pcred);

            if (!r)
            {
                std::cerr << "Failed to read password for " << m_Name << std::endl
                          << " errno " << GetLastError() << std::endl;
            }
            else
            {
                wchar_t* user_name = reinterpret_cast<wchar_t*>(
                    g_utf8_to_utf16(m_Username.c_str(), -1, NULL, NULL, NULL));

                if (user_name)
                {
                    if (wcscmp(pcred->UserName, user_name) == 0)
                        m_Password = (char*)pcred->CredentialBlob;

                    g_free(user_name);
                }

                CredFree(pcred);
            }

            g_free(target_name);
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

    // Download /tag/summary.json, konachan has 2 tld's .com for nsfw and .net for only sfw
    if (m_Type == Type::MOEBOORU && (m_Url.find("yande.re") != std::string::npos ||
                                     m_Url.find("konachan.") != std::string::npos))
    {
        std::thread{ [&]() {
            using nlohmann::json;
            using nlohmann::detail::parse_error;

            bool is_yandere{ m_Url.find("yande.re") != std::string::npos };
            auto url{ m_Url + "/tag/summary.json?version=%1" };

            if (Glib::file_test(m_TagsPath + "-types", Glib::FILE_TEST_EXISTS))
                url = Glib::ustring::compose(
                    url,
                    Settings.get_int(is_yandere ? "YandereTagsVersion" : "KonachanTagsVersion"));
            else
                url = Glib::ustring::compose(url, 0);

            Curler curler{ url, m_ShareHandle };
            if (curler.perform())
            {
                try
                {
                    json j = json::parse(curler.get_buffer());

                    Settings.set(is_yandere ? "YandereTagsVersion" : "KonachanTagsVersion",
                                 j["version"].get<int>());

                    std::string data, line;

                    // Load saved tag types
                    if (j.contains("unchanged"))
                    {
                        std::ifstream ifs(m_TagsPath + "-types");
                        if (ifs)
                            data.assign((std::istreambuf_iterator<char>(ifs)),
                                        std::istreambuf_iterator<char>());
                    }
                    else
                    {
                        data = j["data"].get<std::string>();

                        std::ofstream ofs(m_TagsPath + "-types");
                        ofs << data;
                    }

                    std::istringstream iss{ data };

                    auto& types{ m_Url.find("yande.re") != std::string::npos ? yd_types
                                                                             : kc_types };

                    std::scoped_lock lock{ m_TagMutex };
                    while (std::getline(iss, line, ' '))
                    {
                        int i{ -1 };
                        iss >> i;

                        if (i == -1)
                            break;

                        Tag::Type type{ Tag::Type::UNKNOWN };
                        auto it{ std::find_if(types.begin(), types.end(), [i](const auto& t) {
                            return t.first == i;
                        }) };
                        if (it != types.end())
                            type = it->second;

                        std::string tags, tag;
                        iss >> tags;

                        // Remove the surrounding back ticks
                        tags = tags.substr(1, tags.length() - 2);

                        std::istringstream tags_iss{ tags };

                        while (std::getline(tags_iss, tag, '`'))
                            m_MoebooruTags.emplace(tag, type);
                    }
                }
                catch (const parse_error& e)
                {
                    std::cerr << "Failed to parse summary.json for " << m_Name << std::endl
                              << "  " << e.what() << std::endl;
                }
            }
        } }.detach();
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
#ifdef HAVE_LIBPEAS
    if (m_Type == Type::PLUGIN)
        return m_Url + m_Plugin->get_posts_uri(tags, page, Settings.get_int("BooruLimit"));
#endif // HAVE_LIBPEAS
    return Glib::ustring::compose(m_Url + RequestURI.at(m_Type),
                                  (m_Type == Type::GELBOORU ? page - 1 : page),
                                  Settings.get_int("BooruLimit"),
                                  tags);
}

void Site::add_tags(const std::vector<Tag>& tags)
{
    auto& favorite_tags{ Settings.get_favorite_tags() };
    // Add or update tags (type may have changed)
    for (const auto& t : tags)
    {
        auto [tag, success]{ m_Tags.insert(t) };
        if (!success && t.type != Tag::Type::UNKNOWN)
            tag->type = t.type;

        auto fav_it{ std::find(favorite_tags.begin(), favorite_tags.end(), t) };
        if (fav_it != favorite_tags.end() && t.type != Tag::Type::UNKNOWN)
            fav_it->type = t.type;
    }
}

bool Site::get_multiplexing() const
{
#ifdef HAVE_LIBPEAS
    if (m_Plugin)
        return m_Plugin->get_multiplexing();
#endif // HAVE_LIBPEAS

    // XXX: Maybe implement this as a hidden site value in config files?
    // Haven't come across a booru that needs it disabled (aside from sankaku which is why its here
    // in the first place)
    return true;
}

bool Site::set_url(std::string url)
{
    if (url != m_Url)
    {
#ifdef HAVE_LIBPEAS
        auto [type, plugin]{ get_type_from_url(url) };
#else  // !HAVE_LIBPEAS
        Type type{ get_type_from_url(url) };
#endif // !HAVE_LIBPEAS

        if (type == Type::UNKNOWN)
            return false;

        m_Url  = std::move(url);
        m_Type = type;
#ifdef HAVE_LIBPEAS
        m_Plugin = plugin;
#endif // HAVE_LIBPEAS
    }

    return true;
}

std::string Site::get_register_url() const
{
    if (m_Type != Type::PLUGIN)
        return m_Url + RegisterURI.at(m_Type);
#ifdef HAVE_LIBPEAS
    else
        return m_Plugin->get_register_url(m_Url);
#endif // HAVE_LIBPEAS

    return "";
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
        wchar_t* target_name =
            reinterpret_cast<wchar_t*>(g_utf8_to_utf16(target.c_str(), -1, NULL, NULL, NULL));

        if (target_name)
        {
            wchar_t* user_name = reinterpret_cast<wchar_t*>(
                g_utf8_to_utf16(m_Username.c_str(), -1, NULL, NULL, NULL));

            if (user_name)
            {
                CREDENTIALW cred        = { 0 };
                cred.Type               = CRED_TYPE_GENERIC;
                cred.TargetName         = target_name;
                cred.CredentialBlobSize = s.length();
                cred.CredentialBlob     = (LPBYTE)s.c_str();
                cred.Persist            = CRED_PERSIST_LOCAL_MACHINE;
                cred.UserName           = user_name;

                BOOL r = CredWriteW(&cred, 0);
                if (!r)
                    std::cerr << "Failed to set password for " << m_Name << std::endl
                              << " errno " << GetLastError() << std::endl;

                g_free(user_name);
            }

            g_free(target_name);
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
                auto site_url{ m_Url };
#ifdef HAVE_LIBPEAS
                if (m_Type == Type::PLUGIN)
                {
                    auto url{ m_Plugin->get_icon_url(m_Url) };
                    if (!url.empty())
                        site_url = url;
                }
#endif // HAVE_LIBPEAS

                std::array<std::string, 2> icon_urls;
                if (Image::is_valid_extension(site_url))
                {
                    icon_urls[0] = site_url;
                }
                else
                {
                    icon_urls[0] = site_url + "/favicon.ico";
                    icon_urls[1] = site_url + "/favicon.png";
                }

                m_Curler.set_referer(m_Url);
                m_Curler.set_follow_location(false);

                for (const auto& url : icon_urls)
                {
                    if (url.empty())
                    {
                        std::cout << "empty" << std::endl;
                        continue;
                    }

                    m_Curler.set_url(url);
                    if (m_Curler.perform())
                    {
                        Glib::RefPtr<Gdk::PixbufLoader> loader{ Gdk::PixbufLoader::create() };
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

std::tuple<std::vector<PostDataTuple>, size_t, std::string>
Site::parse_post_data(unsigned char* data, const size_t size)
{
    std::vector<PostDataTuple> posts;
    std::string error;
    size_t posts_count{ 0 };

    if (m_Type != Type::PLUGIN)
    {
        try
        {
            xml::Document posts_xml{ reinterpret_cast<char*>(data), size };

            if (posts_xml.get_attribute("success") == "false" &&
                (!posts_xml.get_attribute("reason").empty() || !posts_xml.get_value().empty()))
            {
                error = posts_xml.get_value().empty() ? posts_xml.get_attribute("reason")
                                                      : posts_xml.get_value();
            }
            else
            {
                std::string c{ posts_xml.get_attribute("count") };
                if (!c.empty())
                {
                    try
                    {
                        posts_count = std::stoul(c);
                    }
                    catch (const std::invalid_argument&)
                    {
                        std::cerr << "Failed to parse post count '" << c << "'" << std::endl;
                    }
                }

                // Use a pointer here to prevent copying m_MoebooruTags for no reason when used
                std::unordered_map<std::string, Tag::Type> cpy;
                std::unordered_map<std::string, Tag::Type>* posts_tags{ nullptr };

                if (m_Url.find("gelbooru.com") != std::string::npos)
                {
                    cpy        = get_posts_tags(posts_xml);
                    posts_tags = &cpy;
                }
                else if (m_Url.find("yande.re") != std::string::npos ||
                         m_Url.find("konachan.") != std::string::npos)
                {
                    // The tag map could still be loading at this point, although it is unlikely
                    std::scoped_lock lock{ m_TagMutex };
                    posts_tags = &m_MoebooruTags;
                }

                for (const xml::Node& post : posts_xml.get_children())
                {
                    std::vector<Tag> tags;
                    std::string id, image_url, thumb_url, post_url, notes_url, date, source, rating,
                        score;

                    if (m_Type == Type::DANBOORU_V2)
                    {
                        id        = post.get_value("id");
                        image_url = post.get_value(m_UseSamples ? "large-file-url" : "file-url");
                        thumb_url = post.get_value("preview-file-url");
                        date      = post.get_value("created-at");
                        source    = post.get_value("source");
                        rating    = post.get_value("rating");
                        score     = post.get_value("score");
                    }
                    else
                    {
                        id        = post.get_attribute("id");
                        image_url = post.get_attribute(m_UseSamples ? "sample_url" : "file_url");
                        thumb_url = post.get_attribute("preview_url");
                        date      = post.get_attribute("created_at");
                        source    = post.get_attribute("source");
                        rating    = post.get_attribute("rating");
                        score     = post.get_attribute("score");
                    }

                    if (m_Type == Type::DANBOORU_V2)
                    {
                        static constexpr std::array<std::pair<Tag::Type, std::string_view>, 5>
                            tag_types{ {
                                { Tag::Type::ARTIST, "tag-string-artist" },
                                { Tag::Type::CHARACTER, "tag-string-character" },
                                { Tag::Type::COPYRIGHT, "tag-string-copyright" },
                                { Tag::Type::METADATA, "tag-string-meta" },
                                { Tag::Type::GENERAL, "tag-string-general" },
                            } };
                        for (const auto& v : tag_types)
                        {
                            std::istringstream ss{ post.get_value(v.second.data()) };
                            std::transform(std::istream_iterator<std::string>{ ss },
                                           std::istream_iterator<std::string>{},
                                           std::back_inserter(tags),
                                           [v](const std::string& t) { return Tag(t, v.first); });
                        }
                    }
                    else
                    {
                        std::istringstream ss{ post.get_attribute("tags") };

                        // Use the posts_tags from gelbooru to find the tag type for every tag
                        if (posts_tags && !posts_tags->empty())
                        {
                            std::transform(std::istream_iterator<std::string>{ ss },
                                           std::istream_iterator<std::string>{},
                                           std::back_inserter(tags),
                                           [&posts_tags](const std::string& t) {
                                               auto it{ posts_tags->find(t) };

                                               return Tag(t,
                                                          it != posts_tags->end()
                                                              ? it->second
                                                              : Tag::Type::UNKNOWN);
                                           });
                        }
                        else
                        {
                            std::transform(std::istream_iterator<std::string>{ ss },
                                           std::istream_iterator<std::string>{},
                                           std::back_inserter(tags),
                                           [](const std::string& t) { return Tag(t); });
                        }
                    }

                    // This makes this function not thread-safe, but realistically you're not
                    // going to be calling this more than once at the same time
                    add_tags(tags);

                    // Prepends either the site url or https (not sure if any sites give links
                    // without a protocol), or does nothing to the url if it doesnt start with a
                    // /
                    static const auto ensure_url = [&](std::string& url) {
                        if (url[0] == '/')
                        {
                            if (url[1] == '/')
                                url = "https:" + url;
                            else
                                url = m_Url + url;
                        }
                    };

                    ensure_url(thumb_url);
                    ensure_url(image_url);

                    post_url = Glib::ustring::compose(m_Url + PostURI.at(m_Type), id);

                    bool has_notes{ false };
                    // Moebooru doesnt have a has_notes attribute, instead they have
                    // last_noted_at which is a unix timestamp or 0 if no notes
                    if (m_Type == Type::MOEBOORU)
                        has_notes = post.get_attribute("last_noted_at") != "0";
                    else if (m_Type == Type::DANBOORU_V2)
                        has_notes = !post.get_value("last-noted-at").empty();
                    else
                        has_notes = post.get_attribute("has_notes") == "true";

                    if (has_notes)
                        notes_url = Glib::ustring::compose(m_Url + NotesURI.at(m_Type), id);

                    // Some older Gelbooru based sites have a bug where their thumbnail urls file
                    // extension match the normal file extension even though all thumbnails are .jpg
                    if (m_Type == Type::GELBOORU)
                        thumb_url = thumb_url.substr(0, thumb_url.find_last_of('.')) + ".jpg";

                    date::sys_seconds t;
                    // DANBOORU_V2 provides dates in the format "%FT%T%Ez"
                    if (m_Type == Type::DANBOORU_V2)
                    {
                        std::string input{ date };
                        std::istringstream stream{ input };
                        stream >> date::parse("%FT%T%Ez", t);

                        if (stream.fail())
                            std::cerr << "Failed to parse date '" << date << "' on site " << m_Name
                                      << std::endl;
                    }
                    // Moebooru provides unix timestamp
                    else if (m_Type == Type::MOEBOORU)
                    {
                        t = static_cast<date::sys_seconds>(
                            std::chrono::duration<long long>(std::stoll(date)));
                    }
                    // Gelbooru, Danbooru "%a %b %d %T %z %Y"
                    else
                    {
                        std::istringstream stream{ date };
                        stream >> date::parse("%a %b %d %T %z %Y", t);

                        if (stream.fail())
                            std::cerr << "Failed to parse date '" << date << "' on site " << m_Name
                                      << std::endl;
                    }

                    PostInfo post_info{
                        format_date_time(t), source, get_rating_string(rating), score
                    };

                    posts.emplace_back(image_url, thumb_url, post_url, notes_url, tags, post_info);
                }
            }
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Site::parse_post_data: " << e.what() << std::endl;
        }
    }
#ifdef HAVE_LIBPEAS
    else
    {
        auto [p, pc, pe]{ m_Plugin->parse_post_data(data, size, m_Url.c_str(), m_UseSamples) };
        posts       = std::move(p);
        posts_count = pc;
        error       = pe;

        for (const auto& post : posts)
            add_tags(std::get<4>(post));
    }
#endif // HAVE_LIBPEAS

    return { posts, posts_count, error };
}

std::vector<Note> Site::parse_note_data(unsigned char* data, const size_t size) const
{
    std::vector<Note> notes;

    if (m_Type != Type::PLUGIN)
    {
        try
        {
            xml::Document doc(reinterpret_cast<char*>(data), size);

            for (const xml::Node& n : doc.get_children())
            {
                std::string body;
                int w, h, x, y;

                if (m_Type == Type::DANBOORU_V2)
                {
                    if (n.get_value("is-active") != "true")
                        continue;

                    body = n.get_value("body");
                    w    = std::stoi(n.get_value("width"));
                    h    = std::stoi(n.get_value("height"));
                    x    = std::stoi(n.get_value("x"));
                    y    = std::stoi(n.get_value("y"));
                }
                else
                {
                    if (n.get_attribute("is_active") != "true")
                        continue;

                    body = n.get_attribute("body");
                    w    = std::stoi(n.get_attribute("width"));
                    h    = std::stoi(n.get_attribute("height"));
                    x    = std::stoi(n.get_attribute("x"));
                    y    = std::stoi(n.get_attribute("y"));
                }

                // Remove all html tags, replace line breaks with \n
                std::string::size_type sp;
                while ((sp = body.find('<')) != std::string::npos)
                {
                    std::string::size_type ep{ body.find('>', sp) };
                    if (ep == std::string::npos)
                        break;

                    if (body.substr(sp, ep).find("<br") != std::string::npos)
                        body.replace(sp, ep + 1 - sp, "\n");
                    else
                        body.erase(sp, ep + 1 - sp);
                }

                decode_html_entities_utf8(body.data(), nullptr);
                notes.emplace_back(body, w, h, x, y);
            }
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Site::parse_note_data: " << e.what() << std::endl;
        }
    }
#ifdef HAVE_LIBPEAS
    else
    {
        notes = m_Plugin->parse_note_data(data, size);
    }
#endif // HAVE_LIBPEAS

    return notes;
}

// Used to get tag types for gelbooru.com
std::unordered_map<std::string, Tag::Type> Site::get_posts_tags(const xml::Document& posts) const
{
    using TagMap = std::unordered_map<std::string, Tag::Type>;
    TagMap posts_tags;

    std::string tags;
    {
        std::vector<std::string> all_tags;

        for (const auto& post : posts.get_children())
        {
            std::istringstream ss{ post.get_attribute("tags") };
            std::vector<std::string> tags{ std::istream_iterator<std::string>{ ss },
                                           std::istream_iterator<std::string>{} };
            all_tags.insert(all_tags.end(), tags.begin(), tags.end());
        }

        std::sort(all_tags.begin(), all_tags.end());
        all_tags.erase(std::unique(all_tags.begin(), all_tags.end()), all_tags.end());

        std::ostringstream oss;
        std::copy(all_tags.begin(), all_tags.end(), std::ostream_iterator<std::string>(oss, " "));
        tags = oss.str();

        if (!all_tags.empty())
            posts_tags.reserve(all_tags.size());
    }

    // unlikely
    if (tags.empty())
        return posts_tags;

    // The above will leavea a trailing space, remove it
    tags.erase(tags.find_last_of(' '));
    tags = m_Curler.escape(tags);

    // We need to split the tags into multiple requests if they are larger than 5K bytes because
    // Gelbooru has a max request header size of 6K bytes, generally the other data in the header
    // should not exceed 1K bytes (but this may have to be looked at)
    const int max_query_size{ 5120 };
    static const std::string_view space{ "%20" };

    size_t splits_needed{ tags.length() / max_query_size };
    std::vector<std::string> split_tags;
    std::string::iterator it, last_it{ tags.begin() };

    for (size_t i = 0; i < splits_needed; ++i)
    {
        // Find the last encoded space before max_query_size * (i+1)
        it = std::find_end(
            last_it, tags.begin() + max_query_size * (i + 1), space.begin(), space.end());
        split_tags.push_back(
            tags.substr(std::distance(tags.begin(), last_it), std::distance(last_it, it)));
        // Advance past the space so the next string wont start with it
        last_it = it + space.length();
    }
    // Add any remaining tags or all the tags if tags.length() < max_query_size
    split_tags.push_back(tags.substr(std::distance(tags.begin(), last_it)));
    std::vector<std::future<TagMap>> jobs;

    static const auto tag_task = [](const std::string& url, const std::string& t) {
        static const std::string tag_uri{ "/index.php?page=dapi&s=tag&q=index&limit=0&names=%1" };
        Curler c{ url + Glib::ustring::compose(tag_uri, t) };
        TagMap tags;
        if (c.perform())
        {
            try
            {
                xml::Document xml{ reinterpret_cast<char*>(c.get_data()), c.get_data_size() };
                auto nodes{ xml.get_children() };
                tags.reserve(nodes.size());

                std::transform(
                    nodes.begin(),
                    nodes.end(),
                    std::inserter(tags, tags.end()),
                    [](const auto& n) -> std::pair<std::string, Tag::Type> {
                        Tag::Type type{ Tag::Type::UNKNOWN };
                        auto i{ std::stoi(n.get_attribute("type")) };
                        auto it{ std::find_if(gb_types.begin(), gb_types.end(), [i](const auto& t) {
                            return t.first == i;
                        }) };
                        if (it != gb_types.end())
                            type = it->second;
                        return { n.get_attribute("name"), type };
                    });
            }
            catch (const std::runtime_error& e)
            {
                std::cerr << "Site::get_posts_tags: " << e.what() << std::endl;
            }
        }
        return tags;
    };

    for (const auto& t : split_tags)
        jobs.push_back(std::async(std::launch::async, tag_task, m_Url, t));

    // Wait for all the jobs to finish and combine all the tags
    for (auto& job : jobs)
        posts_tags.merge(job.get());

    return posts_tags;
}
