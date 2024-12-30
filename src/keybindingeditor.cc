#include "keybindingeditor.h"
using namespace AhoViewer;

#include "application.h"
#include "config.h"
#include "settings.h"

#include <cctype>
#include <glibmm/i18n.h>

KeybindingEditor::KeybindingEditor(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::Box{ cobj },
      m_Model{ Gtk::TreeStore::create(m_Columns) },
      m_AccelModel{ Gtk::TreeStore::create(m_AccelColumns) },
      m_ComboModel{ Gtk::ListStore::create(m_ComboColumns) }
{
    bldr->get_widget("KeybindingEditor::AccelDialog", m_AccelDialog);
    bldr->get_widget("KeybindingEditor::AccelView", m_AccelView);
    bldr->get_widget("KeybindingEditor::TreeView", m_TreeView);
    bldr->get_widget("KeybindingEditor::AccelCombo", m_AccelCombo);
    bldr->get_widget("KeybindingEditor::EditButton", m_EditButton);
    bldr->get_widget("KeybindingEditor::Add", m_AddButton);
    bldr->get_widget("KeybindingEditor::Save", m_SaveButton);
    bldr->get_widget("KeybindingEditor::Delete", m_DeleteButton);
    bldr->get_widget("KeybindingEditor::Default", m_DefaultButton);

    // The order of this array determines the order of the model in the tree view
    static constexpr std::array<std::string_view, 8> cats{
        "Booru Browser", // NOLINT
        "File",          // NOLINT
        "Navigation",    // NOLINT
#ifdef HAVE_LIBPEAS
        "Plugins",        // NOLINT
#else                     // !HAVE_LIBPEAS
        "", // NOLINT
#endif                    // !HAVE_LIBPEAS
        "Scroll",         // NOLINT
        "User Interface", // NOLINT
        "View Mode",      // NOLINT
        "Zoom"            // NOLINT
    };
    std::map<std::string_view, Gtk::TreeIter> cat_iters;
    for (auto cat : cats)
    {
        if (cat.empty())
            continue;

        auto it{ m_Model->append() };
        it->set_value(m_Columns.editable, false);
        it->set_value(m_Columns.name, std::string{ cat.data() });
        cat_iters.emplace(cat, it);
    }
#ifdef HAVE_LIBPEAS
    static constexpr auto is_plugin_loaded = [&](const std::string& action_name) {
        const auto& plugins{
            Application::get_default()->get_plugin_manager().get_window_plugins()
        };
        auto it{ std::find_if(plugins.cbegin(), plugins.cend(), [&](const auto& p) {
            return "win." + p->get_action_name() == action_name;
        }) };
        return it != plugins.end();
    };
#endif // HAVE_LIBPEAS

    // Add the keybindings to the treeview model
    for (const auto& [name, binds] : Settings.get_keybindings())
    {
        std::string cat{ get_cat_for_action(name) };
        auto parent_iter{ cat_iters.find(cat)->second };
#ifdef HAVE_LIBPEAS
        // When saved keybindings are loaded they do not check if the plugin is loaded
        if (cat == "Plugins" && !is_plugin_loaded(name))
            continue;
#endif // HAVE_LIBPEAS
        Gtk::TreeIter iter = m_Model->append(parent_iter->children());
        iter->set_value(m_Columns.editable, true);
        iter->set_value(m_Columns.name, name);
        iter->set_value(m_Columns.bindings_readable, get_readable_bindings(binds));
        iter->set_value(m_Columns.bindings, binds);
    }

    // Setup the main treeview renderers
    m_TreeView->append_column(_("Action"), m_Columns.name);
    m_TreeView->get_column(0)->set_cell_data_func(
        *(m_TreeView->get_column(0)->get_first_cell()),
        sigc::mem_fun(*this, &KeybindingEditor::action_data_func));

    m_TreeView->append_column(_("Keybindings"), m_Columns.bindings_readable);

    m_TreeView->set_model(m_Model);
    m_TreeView->signal_cursor_changed().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_cursor_changed));

    // Connect the accel dialog signals
    auto* close{ static_cast<Gtk::Button*>(
        m_AccelDialog->get_widget_for_response(Gtk::RESPONSE_CLOSE)) };
    close->signal_clicked().connect([&]() { m_AccelDialog->response(Gtk::RESPONSE_CLOSE); });
    m_AccelDialog->signal_response().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_dialog_response));
    // Gtk4
    // m_AccelDialog->set_hide_on_close(true);

    // Dialog treeview accel renderer
    auto* column{ Gtk::make_managed<Gtk::TreeViewColumn>() };
    auto* accel_renderer{ Gtk::make_managed<Gtk::CellRendererAccel>() };
    // Gtk::CellRendererAccel::OTHER in gtk4
    accel_renderer->property_accel_mode() = Gtk::CELL_RENDERER_ACCEL_MODE_OTHER;
    accel_renderer->property_editable()   = true;

    m_AccelView->set_model(m_AccelModel);
    accel_renderer->signal_accel_edited().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_accel_edited));
    column->pack_start(*accel_renderer);
    column->add_attribute(*accel_renderer, "accel-mods", 0);
    column->add_attribute(*accel_renderer, "accel-key", 1);
    column->add_attribute(*accel_renderer, "keycode", 2);
    m_AccelView->append_column(*column);

    m_AccelCombo->set_model(m_ComboModel);
    // Remove the default cell renderer
    m_AccelCombo->clear();
    m_AccelCombo->pack_start(m_ComboColumns.binding_readable);
    m_AccelCombo->signal_changed().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_combo_changed));
    m_EditButton->signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_show_accel_dialog));

    m_AddButton->signal_clicked().connect(sigc::mem_fun(*this, &KeybindingEditor::on_add_clicked));
    m_SaveButton->signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_save_clicked));
    m_DeleteButton->signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_delete_clicked));
    m_DefaultButton->signal_clicked().connect(
        sigc::mem_fun(*this, &KeybindingEditor::on_default_clicked));
}

void KeybindingEditor::action_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const
{
    auto* cell{ static_cast<Gtk::CellRendererText*>(c) };
    std::string action{ iter->get_value(m_Columns.name) }, val;

    if (iter->get_value(m_Columns.editable))
    {
        if (action.find("ZoomMode::", 4) != std::string::npos)
        {
            val = get_readable_zoom_mode(action);
        }
        else
        {
            // Remove the win/app prefix, plugin actions might not have these
            auto prefix{ action.find_first_of('.') };
            if (prefix != std::string::npos)
                action = action.substr(prefix + 1);
            for (std::string::iterator i = action.begin(); i != action.end(); ++i)
            {
                if (i != action.begin() && isupper(*i) && !isupper(*(i - 1)))
                    val += ' ';
                val += *i;
            }
        }
    }
    else
    {
        val = action;
    }

    cell->property_text() = val;
}

Glib::ustring KeybindingEditor::get_readable_binding(const Glib::ustring& binding) const
{
    guint key{ 0 };
    GdkModifierType mods{ GdkModifierType(0) };
    gtk_accelerator_parse(binding.c_str(), &key, &mods);
    char* accel{ gtk_accelerator_get_label(key, mods) };
    Glib::ustring ret(accel);
    g_free(accel);

    return ret;
}

Glib::ustring
KeybindingEditor::get_readable_bindings(const std::vector<Glib::ustring>& bindings) const
{
    Glib::ustring s;
    for (const auto& k : bindings)
        s += get_readable_binding(k) + ", ";

    if (!s.empty())
        s = s.substr(0, s.length() - 2);

    return s;
}

void KeybindingEditor::on_combo_changed()
{
    auto it{ m_AccelCombo->get_active() };
    m_CurrentAccel = it ? it->get_value(m_ComboColumns.binding) : "";

    // update the button states
    update_accel(m_CurrentAccel);
}

void KeybindingEditor::update_accel(const Glib::ustring& accel)
{
    Glib::ustring binding{ accel };

    if (accel != m_CurrentAccel)
    {
        guint key{ 0 };
        GdkModifierType mods{ GdkModifierType(0) };
        gtk_accelerator_parse(accel.c_str(), &key, &mods);
        char* name{ gtk_accelerator_name(key, mods) };
        binding = name;
        g_free(name);

        m_AccelCombo->get_active()->set_value(m_ComboColumns.binding, binding);
        m_AccelCombo->get_active()->set_value(m_ComboColumns.binding_readable,
                                              get_readable_binding(binding));
    }

    // Enable the save button if the binding isnt in the current bindings vector
    m_SaveButton->set_sensitive(!binding.empty() && std::find(m_CurrentBindings.begin(),
                                                              m_CurrentBindings.end(),
                                                              binding) == m_CurrentBindings.end());
    // if binding is empty that means the combobox model is empty
    m_EditButton->set_sensitive(!binding.empty());
    m_DeleteButton->set_sensitive(!binding.empty());
    // Only enable the default button if there are defaults and changes have been made
    m_DefaultButton->set_sensitive(!m_CurrentDefaults.empty() &&
                                   m_CurrentBindings != m_CurrentDefaults);
}

void KeybindingEditor::clear_keybinding(const Glib::ustring& binding)
{
    auto pair{ Settings.clear_keybinding(binding) };
    if (!pair.first.empty())
    {
        Gtk::TreeIter it;
        m_Model->foreach_iter([&](const Gtk::TreeIter& iter) {
            Glib::ustring s{ iter->get_value(m_Columns.name) };
            if (s == pair.first)
            {
                it = iter;
                return true;
            }
            return false;
        });
        it->set_value(m_Columns.bindings_readable, get_readable_bindings(pair.second));
        it->set_value(m_Columns.bindings, pair.second);
    }
}

void KeybindingEditor::on_show_accel_dialog()
{
    m_AccelModel->clear();
    auto it{ m_AccelModel->append() };
    if (!m_CurrentAccel.empty())
    {
        guint key;
        GdkModifierType mods;
        gtk_accelerator_parse(m_CurrentAccel.c_str(), &key, &mods);
        it->set_value(m_AccelColumns.mods, static_cast<int>(mods));
        it->set_value(m_AccelColumns.key, key);
    }
    auto* w{ static_cast<Gtk::Window*>(get_toplevel()) };
    m_AccelDialog->set_transient_for(*w);
    m_AccelDialog->show();
}

void KeybindingEditor::on_dialog_response(int resp)
{
    m_AccelDialog->hide();
    if (resp != Gtk::RESPONSE_OK)
    {
        // Remove placehold when cancelling an add
        auto it{ m_AccelCombo->get_active() };
        if (it->get_value(m_ComboColumns.binding).empty())
            on_delete_clicked();
        return;
    }

    auto [key, mods, keycode]{ m_AccelOut };
    char* accel{ gtk_accelerator_name_with_keycode(nullptr, key, keycode, mods) };
    update_accel(accel);
    g_free(accel);
}

void KeybindingEditor::on_add_clicked()
{
    Gtk::TreeIter it;
    m_ComboModel->foreach_iter([&](const Gtk::TreeIter& iter) {
        Glib::ustring s{ iter->get_value(m_ComboColumns.binding) };
        if (s.empty())
        {
            it = iter;
            return true;
        }
        return false;
    });
    // Don't add a placeholder if one exists
    if (!it)
    {
        Glib::ustring accel{ "" };
        it = m_ComboModel->append();
        it->set_value(m_ComboColumns.binding, accel);
        it->set_value(m_ComboColumns.binding_readable, accel);
    }

    m_AccelCombo->set_active(it);
    on_show_accel_dialog();
}

void KeybindingEditor::on_save_clicked()
{
    auto binding{ m_AccelCombo->get_active()->get_value(m_ComboColumns.binding) };
    if (m_CurrentAccel.empty())
    {
        m_CurrentBindings.emplace_back(binding);
    }
    else
    {
        auto it{ std::find(m_CurrentBindings.begin(), m_CurrentBindings.end(), m_CurrentAccel) };
        *it = binding;
    }

    std::sort(m_CurrentBindings.begin(), m_CurrentBindings.end());

    clear_keybinding(binding);
    Settings.set_keybindings(m_CurrentAction, m_CurrentBindings);

    auto iter{ m_TreeView->get_selection()->get_selected() };
    iter->set_value(m_Columns.bindings_readable, get_readable_bindings(m_CurrentBindings));
    iter->set_value(m_Columns.bindings, m_CurrentBindings);

    // This updates the combobox model and button states
    on_cursor_changed();
}

void KeybindingEditor::on_delete_clicked()
{
    auto it{ m_AccelCombo->get_active() };

    // Remove it from the settings and update treeview
    if (!it->get_value(m_ComboColumns.binding).empty())
    {
        m_CurrentBindings.erase(
            std::find(m_CurrentBindings.begin(), m_CurrentBindings.end(), m_CurrentAccel));
        Settings.set_keybindings(m_CurrentAction, m_CurrentBindings);

        auto iter{ m_TreeView->get_selection()->get_selected() };
        iter->set_value(m_Columns.bindings_readable, get_readable_bindings(m_CurrentBindings));
        iter->set_value(m_Columns.bindings, m_CurrentBindings);
    }

    // Remove it from the combobox
    auto next{ m_ComboModel->erase(it) };
    if (next)
        m_AccelCombo->set_active(next);
    else
        m_AccelCombo->set_active(0);
}

void KeybindingEditor::on_default_clicked()
{
    auto accels{ Settings.get_default_keybindings(m_CurrentAction) };

    for (auto& accel : accels)
        clear_keybinding(accel);

    Settings.reset_keybindings(m_CurrentAction);
    auto iter{ m_TreeView->get_selection()->get_selected() };
    iter->set_value(m_Columns.bindings_readable, get_readable_bindings(m_CurrentDefaults));
    iter->set_value(m_Columns.bindings, m_CurrentDefaults);

    // This updates the combobox model and button states
    on_cursor_changed();
}

void KeybindingEditor::on_cursor_changed()
{
    m_ComboModel->clear();

    auto iter{ m_TreeView->get_selection()->get_selected() };
    // Selected a category, disable all the buttons
    if (!iter->get_value(m_Columns.editable))
    {
        m_EditButton->set_sensitive(false);
        m_AddButton->set_sensitive(false);
        m_SaveButton->set_sensitive(false);
        m_DeleteButton->set_sensitive(false);
        m_DefaultButton->set_sensitive(false);
    }
    else
    {
        m_CurrentAction   = iter->get_value(m_Columns.name);
        m_CurrentDefaults = Settings.get_default_keybindings(m_CurrentAction);
        m_CurrentBindings = iter->get_value(m_Columns.bindings);
        bool has_bindings{ false };
        for (const auto& s : m_CurrentBindings)
        {
            if (s.empty())
                continue;

            auto it{ m_ComboModel->append() };
            it->set_value(m_ComboColumns.binding, s);
            it->set_value(m_ComboColumns.binding_readable, get_readable_binding(s));
            has_bindings = true;
        }

        m_AddButton->set_sensitive(true);
        m_EditButton->set_sensitive(has_bindings);
        m_AccelCombo->set_active(0);
    }
}

void KeybindingEditor::on_accel_edited(const std::string&,
                                       guint key,
                                       Gdk::ModifierType mods,
                                       guint keycode)
{
    m_AccelOut = { key, static_cast<GdkModifierType>(mods), keycode };
    m_AccelDialog->response(Gtk::RESPONSE_OK);
}
