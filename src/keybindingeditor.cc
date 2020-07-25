#include <cctype>
#include <glibmm/i18n.h>
#include <gtkmm/accelmap.h>

#include "keybindingeditor.h"
using namespace AhoViewer;

#include "config.h"
#include "settings.h"

KeybindingEditor::KeybindingEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::TreeView(cobj),
    m_Model(Gtk::TreeStore::create(m_Columns))
{
    for (const auto &i : Settings.get_keybindings())
    {
        Gtk::TreeIter parentIter = m_Model->append();
        parentIter->set_value(m_Columns.editable, false);
        parentIter->set_value(m_Columns.name, i.first);

        for (const auto &j : i.second)
        {
            Gtk::TreeIter iter = m_Model->append(parentIter->children());
            iter->set_value(m_Columns.editable, true);
            iter->set_value(m_Columns.name, j.first);
            iter->set_value(m_Columns.binding, j.second);
        }
    }

    append_column(_("Action"), m_Columns.name);
    get_column(0)->set_cell_data_func(*(get_column(0)->get_first_cell()),
            sigc::mem_fun(*this, &KeybindingEditor::action_data_func));

    Gtk::CellRendererAccel *accelRenderer = Gtk::manage(new Gtk::CellRendererAccel());
    accelRenderer->signal_accel_edited().connect(
            sigc::mem_fun(*this, &KeybindingEditor::on_accel_edited));
    accelRenderer->signal_accel_cleared().connect(
            sigc::mem_fun(*this, &KeybindingEditor::on_accel_cleared));

    append_column(_("Keybinding"), *accelRenderer);
    get_column(1)->set_cell_data_func(*accelRenderer,
            sigc::mem_fun(*this, &KeybindingEditor::accel_data_func));
    get_column(1)->add_attribute(accelRenderer->property_text(), m_Columns.binding);
    get_column(1)->add_attribute(accelRenderer->property_editable(), m_Columns.editable);

    set_model(m_Model);

    Gtk::ToolButton *toolButton = nullptr;
    bldr->get_widget("KeybindingEditor::ResetSelectedButton", toolButton);
    toolButton->signal_clicked().connect(sigc::mem_fun(*this, &KeybindingEditor::on_reset_selected));
}

void KeybindingEditor::on_reset_selected()
{
    Gtk::TreeIter iter = get_selection()->get_selected();

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

void KeybindingEditor::action_data_func(Gtk::CellRenderer *c, const Gtk::TreeIter &iter)
{
    Gtk::CellRendererText *cell = static_cast<Gtk::CellRendererText*>(c);
    std::string action = iter->get_value(m_Columns.name), val;

    for (std::string::iterator i = action.begin(); i != action.end(); ++i)
    {
        if (i != action.begin() && isupper(*i) && !isupper(*(i-1)))
            val += ' ';
        val += *i;
    }

    cell->property_text() = val;
}

void KeybindingEditor::accel_data_func(Gtk::CellRenderer *c, const Gtk::TreeIter &iter)
{
    Gtk::CellRendererText *cell = static_cast<Gtk::CellRendererText*>(c);
    std::string accel = iter->get_value(m_Columns.binding);
    guint key;
    Gdk::ModifierType mods;

    Gtk::AccelGroup::parse(accel, key, mods);

    cell->property_text() = Gtk::AccelGroup::get_label(key, mods);
}

void KeybindingEditor::on_accel_edited(const std::string &path, guint key, Gdk::ModifierType mods, guint)
{
    Gtk::TreeIter iter = m_Model->get_iter(path);
    std::string accel = Gtk::AccelGroup::name(key, mods),
                group, name;
    std::string accelPath = Glib::ustring::compose("<Actions>/" PACKAGE "/%1",
                                                     iter->get_value(m_Columns.name));
    bool exists = Gtk::AccelMap::lookup_entry(accelPath);

    // Clear binding if it is using this accelerator
    if (Settings.clear_keybinding(accel, group, name))
    {
        Gtk::TreeModel::Children children = m_Model->children();
        for (Gtk::TreeIter i = children.begin(); i != children.end(); ++i)
        {
            if (i->get_value(m_Columns.name) == group)
            {
                Gtk::TreeModel::Children gChildren = i->children();
                for (Gtk::TreeIter j = gChildren.begin(); j != gChildren.end(); ++j)
                {
                    if (j->get_value(m_Columns.name) == name)
                        j->set_value(m_Columns.binding, std::string(""));
                }
            }
        }
    }

    Settings.set_keybinding(iter->parent()->get_value(m_Columns.name),
                            iter->get_value(m_Columns.name),
                            accel);
    iter->set_value(m_Columns.binding, accel);
    Gtk::AccelMap::change_entry(accelPath, key, mods, true);

    // this signal is needed for actions that had no accelerator on startup
    if (!exists)
        m_SignalEdited(accelPath, iter->get_value(m_Columns.name));
}

void KeybindingEditor::on_accel_cleared(const std::string &path)
{
    Gtk::TreeIter iter = m_Model->get_iter(path);
    std::string accelPath = Glib::ustring::compose("<Actions>/" PACKAGE "/%1",
                                                   iter->get_value(m_Columns.name));

    Settings.set_keybinding(iter->parent()->get_value(m_Columns.name),
                            iter->get_value(m_Columns.name),
                            "");
    iter->set_value(m_Columns.binding, std::string(""));
    Gtk::AccelMap::change_entry(accelPath, 0, static_cast<Gdk::ModifierType>(0), true);
}
