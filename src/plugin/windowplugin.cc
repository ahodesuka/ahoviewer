#include "windowplugin.h"
using namespace AhoViewer::Plugin;

#include "util.h"
using AhoViewer::Util::null_check_string;

#include <iostream>

WindowPlugin::WindowPlugin(PeasPluginInfo* pi, AhoviewerWindowAbstract* a)
    : m_Abstract{ a },
      m_Hidden{ static_cast<bool>(peas_plugin_info_is_hidden(pi)) }
{
    m_Name        = null_check_string(peas_plugin_info_get_name(pi));
    m_Description = null_check_string(peas_plugin_info_get_description(pi));
    m_ActionName  = null_check_string(peas_plugin_info_get_external_data(pi, "ActionName"));
}

WindowPlugin::~WindowPlugin()
{
    on_deactivate();
}

void WindowPlugin::on_activate(MainWindow* w)
{
    w->signal_hide().connect(sigc::mem_fun(*this, &WindowPlugin::on_deactivate));

    g_object_set(m_Abstract, "main-window", w, nullptr);

    peas_activatable_activate(PEAS_ACTIVATABLE(m_Abstract));
    m_Activated = true;
}

void WindowPlugin::on_deactivate()
{
    // A plugin with a GtkWindow should respond to the deactivate signal by hiding itself
    if (m_Activated)
    {
        g_object_set(m_Abstract, "main-window", nullptr, nullptr);
        peas_activatable_deactivate(PEAS_ACTIVATABLE(m_Abstract));
        m_Activated = false;
    }
}
