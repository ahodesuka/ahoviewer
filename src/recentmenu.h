#pragma once

#include <giomm/menu.h>
#include <gtkmm/recentmanager.h>

namespace AhoViewer
{
    class RecentMenu : public Gio::Menu
    {
    public:
        static Glib::RefPtr<RecentMenu> create();
        ~RecentMenu() override = default;

    protected:
        RecentMenu();

    private:
        void on_changed();

        std::vector<Glib::ustring> m_MimeTypes;
    };
}
