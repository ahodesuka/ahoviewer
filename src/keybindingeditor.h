#pragma once

#include <gtkmm.h>

namespace AhoViewer
{
    class KeybindingEditor : public Gtk::TreeView
    {
        // accelPath, actionName
        using SignalEditedType = sigc::signal<void, const std::string&, const std::string&>;

    public:
        KeybindingEditor(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~KeybindingEditor() override = default;

        SignalEditedType signal_edited() const { return m_SignalEdited; }

    private:
        struct ModelColumns : public Gtk::TreeModelColumnRecord
        {
            ModelColumns()
            {
                add(editable);
                add(name);
                add(binding);
            }
            Gtk::TreeModelColumn<bool> editable;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<std::string> binding;
        };

        void on_reset_selected();

        void action_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const;
        void accel_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const;

        void on_accel_edited(const std::string& path, guint key, Gdk::ModifierType mods, guint);
        void on_accel_cleared(const std::string& path);

        ModelColumns m_Columns;
        Glib::RefPtr<Gtk::TreeStore> m_Model;

        SignalEditedType m_SignalEdited;
    };
}
