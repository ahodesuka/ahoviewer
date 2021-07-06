#include "keybindingeditor.h"
using namespace AhoViewer;

#include "application.h"
#include "config.h"
#include "settings.h"

#include <cctype>
#include <glibmm/i18n.h>
#include <gtkmm/accelmap.h>

KeybindingEditor::KeybindingEditor(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::TreeView(cobj),
      m_Model(Gtk::TreeStore::create(m_Columns))
{
    for (const auto& i : Settings.get_keybindings())
    {
        Gtk::TreeIter parent_iter = m_Model->append();
        parent_iter->set_value(m_Columns.editable, false);
        parent_iter->set_value(m_Columns.name, i.first);

#ifdef HAVE_LIBPEAS
        // Only show keybindings for plugins that have been loaded
        if (i.first == "Plugins")
        {
            const auto& plugins{
                Application::get_default()->get_plugin_manager().get_window_plugins()
            };
            for (auto& p : plugins)
            {
                if (!p->get_action_name().empty())
                {
                    Gtk::TreeIter iter = m_Model->append(parent_iter->children());
                    iter->set_value(m_Columns.editable, true);
                    iter->set_value(m_Columns.name, p->get_action_name());
                    iter->set_value(m_Columns.binding,
                                    Settings.get_keybinding(i.first, p->get_action_name()));
                }
            }
            continue;
        }
#endif // HAVE_LIBPEAS
        for (const auto& j : i.second)
        {
            Gtk::TreeIter iter = m_Model->append(parent_iter->children());
            iter->set_value(m_Columns.editable, true);
            iter->set_value(m_Columns.name, j.first);
            iter->set_value(m_Columns.binding, j.second);
        }
    }

    append_column(_("Action"), m_Columns.name);
    get_column(0)->set_cell_data_func(*(get_column(0)->get_first_cell()),
                                      sigc::mem_fun(*this, &KeybindingEditor::action_data_func));

    auto* accel_renderer{ Gtk::make_managed<Gtk::CellRendererAccel>() };
    accel_renderer->signal_accel_edited().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_accel_edited));
    accel_renderer->signal_accel_cleared().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_accel_cleared));

    append_column(_("Keybinding"), *accel_renderer);
    get_column(1)->set_cell_data_func(*accel_renderer,
                                      sigc::mem_fun(*this, &KeybindingEditor::accel_data_func));
    get_column(1)->add_attribute(accel_renderer->property_text(), m_Columns.binding);
    get_column(1)->add_attribute(accel_renderer->property_editable(), m_Columns.editable);

    set_model(m_Model);

    Gtk::ToolButton* tool_button{ nullptr };
    bldr->get_widget("KeybindingEditor::ResetSelectedButton", tool_button);
    tool_button->signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_reset_selected));
}

void KeybindingEditor::on_reset_selected()
{
    Gtk::TreeIter iter{ get_selection()->get_selected() };

    if (iter && iter->parent())
    {
        std::string accel = Settings.reset_keybinding(iter->parent()->get_value(m_Columns.name),
                                                      iter->get_value(m_Columns.name));

        guint key;
        Gdk::ModifierType mods;
        Gtk::AccelGroup::parse(accel, key, mods);

        on_accel_edited(m_Model->get_path(iter).to_string(), key, mods, 0);
    }
}

// Transforms action names into readable keybinding names
//   ScrollDown -> Scroll Down
void KeybindingEditor::action_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const
{
    auto* cell{ static_cast<Gtk::CellRendererText*>(c) };
    std::string action{ iter->get_value(m_Columns.name) }, val;

    for (std::string::iterator i = action.begin(); i != action.end(); ++i)
    {
        if (i != action.begin() && isupper(*i) && !isupper(*(i - 1)))
            val += ' ';
        val += *i;
    }

    cell->property_text() = val;
}

void KeybindingEditor::accel_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const
{
    auto* cell{ static_cast<Gtk::CellRendererText*>(c) };
    std::string accel{ iter->get_value(m_Columns.binding) };
    guint key;
    Gdk::ModifierType mods;

    Gtk::AccelGroup::parse(accel, key, mods);

    cell->property_text() = Gtk::AccelGroup::get_label(key, mods);
}

void KeybindingEditor::on_accel_edited(const std::string& path,
                                       guint key,
                                       Gdk::ModifierType mods,
                                       guint)
{
    Gtk::TreeIter iter{ m_Model->get_iter(path) };
    std::string accel{ Gtk::AccelGroup::name(key, mods) }, group, name;
    std::string accel_path{ Glib::ustring::compose("<Actions>/" PACKAGE "/%1",
                                                   iter->get_value(m_Columns.name)) };
    bool exists{ Gtk::AccelMap::lookup_entry(accel_path) };

    // Clear binding if it is using this accelerator
    if (Settings.clear_keybinding(accel, group, name))
    {
        Gtk::TreeModel::Children children = m_Model->children();
        for (const auto& i : children)
        {
            if (i.get_value(m_Columns.name) == group)
            {
                Gtk::TreeModel::Children g_children = i.children();
                for (const auto& j : g_children)
                {
                    if (j.get_value(m_Columns.name) == name)
                        j.set_value(m_Columns.binding, std::string(""));
                }
            }
        }
    }

    Settings.set_keybinding(
        iter->parent()->get_value(m_Columns.name), iter->get_value(m_Columns.name), accel);
    iter->set_value(m_Columns.binding, accel);
    Gtk::AccelMap::change_entry(accel_path, key, mods, true);

    // this signal is needed for actions that had no accelerator on startup
    if (!exists)
        m_SignalEdited(accel_path, iter->get_value(m_Columns.name));
}

void KeybindingEditor::on_accel_cleared(const std::string& path)
{
    Gtk::TreeIter iter{ m_Model->get_iter(path) };
    std::string accel_path{ Glib::ustring::compose("<Actions>/" PACKAGE "/%1",
                                                   iter->get_value(m_Columns.name)) };

    Settings.set_keybinding(
        iter->parent()->get_value(m_Columns.name), iter->get_value(m_Columns.name), "");
    iter->set_value(m_Columns.binding, std::string(""));
    Gtk::AccelMap::change_entry(accel_path, 0, static_cast<Gdk::ModifierType>(0), true);
}
