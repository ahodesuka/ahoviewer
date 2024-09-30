#pragma once

#include "mainwindow.h"
#include "windowabstract.h"

#include <libpeas/peas.h>
#include <sigc++/signal.h>
#include <string>

namespace AhoViewer::Plugin
{
    class WindowPlugin : public sigc::trackable
    {
    public:
        WindowPlugin(PeasPluginInfo* pi, AhoviewerWindowAbstract* const a);
        ~WindowPlugin();

        std::string get_name() const { return m_Name; }
        std::string get_description() const { return m_Description; }
        std::string get_action_name() const { return m_ActionName; }
        std::string get_action_accel() const { return m_ActionAccel; }

        bool is_hidden() const { return m_Hidden; }

        void on_activate(MainWindow* w);
        void on_deactivate();

    private:
        AhoviewerWindowAbstract* m_Abstract;

        std::string m_Name, m_Description, m_ActionName, m_ActionAccel;
        bool m_Hidden, m_Activated{ false };
    };
}
