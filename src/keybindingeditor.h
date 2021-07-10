#pragma once

#include "util.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class KeybindingEditor : public Gtk::Box
    {
    public:
        KeybindingEditor(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~KeybindingEditor() override = default;

    private:
        struct ModelColumns : public Gtk::TreeModel::ColumnRecord
        {
            ModelColumns()
            {
                add(editable);
                add(name);
                add(bindings_readable);
                add(bindings);
            }
            Gtk::TreeModelColumn<bool> editable;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<Glib::ustring> bindings_readable;
            Gtk::TreeModelColumn<std::vector<Glib::ustring>> bindings;
        };
        struct ComboColumns : public Gtk::TreeModel::ColumnRecord
        {
            ComboColumns()
            {
                add(binding);
                add(binding_readable);
            }
            Gtk::TreeModelColumn<Glib::ustring> binding, binding_readable;
        };
        struct AccelColumns : public Gtk::TreeModel::ColumnRecord
        {
            AccelColumns()
            {
                add(mods);
                add(key);
                add(keycode);
            }
            Gtk::TreeModelColumn<int> mods;
            Gtk::TreeModelColumn<uint32_t> key, keycode;
        };

        // Maps actions to their categories
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 37>
            BindingCategoryMap{ {
                { "win.OpenFile", "File" },
                { "win.DeleteImage", "File" },
                { "app.Preferences", "File" },
                { "win.Close", "File" },
                { "app.Quit", "File" },

                { "win.ToggleMangaMode", "View Mode" },
                { "win.ZoomMode::A", "View Mode" },
                { "win.ZoomMode::W", "View Mode" },
                { "win.ZoomMode::H", "View Mode" },
                { "win.ZoomMode::M", "View Mode" },

                { "win.ToggleFullscreen", "User Interface" },
                { "win.ToggleMenuBar", "User Interface" },
                { "win.ToggleStatusBar", "User Interface" },
                { "win.ToggleScrollbars", "User Interface" },
                { "win.ToggleThumbnailBar", "User Interface" },
                { "win.ToggleBooruBrowser", "User Interface" },
                { "win.ToggleHideAll", "User Interface" },

                { "win.ZoomIn", "Zoom" },
                { "win.ZoomOut", "Zoom" },
                { "win.ResetZoom", "Zoom" },

                { "win.NextImage", "Navigation" },
                { "win.PreviousImage", "Navigation" },
                { "win.FirstImage", "Navigation" },
                { "win.LastImage", "Navigation" },
                { "win.ToggleSlideshow", "Navigation" },

                { "win.ScrollUp", "Scroll" },
                { "win.ScrollDown", "Scroll" },
                { "win.ScrollLeft", "Scroll" },
                { "win.ScrollRight", "Scroll" },

                { "win.NewTab", "Booru Browser" },
                { "win.SaveImage", "Booru Browser" },
                { "win.SaveImageAs", "Booru Browser" },
                { "win.SaveImages", "Booru Browser" },
                { "win.ViewPost", "Booru Browser" },
                { "win.CopyImageURL", "Booru Browser" },
                { "win.CopyImageData", "Booru Browser" },
                { "win.CopyPostURL", "Booru Browser" },
            } };

        static auto get_cat_for_action(const std::string_view key)
        {
            static constexpr auto map{
                FlatMap<std::string_view, std::string_view, BindingCategoryMap.size()>{
                    { BindingCategoryMap } }
            };
            try
            {
                return map.at(key).data();
            }
            catch (std::range_error&)
            {
                return "Plugins";
            }
        };
        // Maps Zoom modes to more readable names
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> ZoomModeMap{ {
            { "win.ZoomMode::A", "Auto Fit Mode" },
            { "win.ZoomMode::W", "Fit Width Mode" },
            { "win.ZoomMode::H", "Fit Height Mode" },
            { "win.ZoomMode::M", "Manual Zoom Mode" },
        } };
        static auto get_readable_zoom_mode(const std::string_view key)
        {
            static constexpr auto map{
                FlatMap<std::string_view, std::string_view, ZoomModeMap.size()>{ { ZoomModeMap } }
            };
            return map.at(key).data();
        };

        // TreeView action column, changes win.ScrollDown to Scroll Down
        void action_data_func(Gtk::CellRenderer* c, const Gtk::TreeIter& iter) const;
        // Takes in a binding and returns a readable version
        Glib::ustring get_readable_binding(const Glib::ustring& binding) const;
        Glib::ustring get_readable_bindings(const std::vector<Glib::ustring>& bindings) const;
        void update_accel(const Glib::ustring& accel);
        void update_modifiers();

        void on_combo_changed();
        void on_show_accel_dialog();
        void on_dialog_response(int resp);
        void on_add_clicked();
        void on_save_clicked();
        void on_delete_clicked();
        void on_default_clicked();

        void on_cursor_changed();

        void on_accel_edited(const std::string&, guint key, Gdk::ModifierType mods, guint keycode);

        std::string m_CurrentAction;
        Glib::ustring m_CurrentAccel;
        std::vector<Glib::ustring> m_CurrentBindings, m_CurrentDefaults;

        Gtk::MessageDialog* m_AccelDialog;
        Gtk::TreeView *m_TreeView, *m_AccelView;
        Gtk::ComboBox* m_AccelCombo;
        Gtk::Button *m_EditButton, *m_AddButton, *m_SaveButton, *m_DeleteButton, *m_DefaultButton;

        ModelColumns m_Columns;
        AccelColumns m_AccelColumns;
        ComboColumns m_ComboColumns;
        Glib::RefPtr<Gtk::TreeStore> m_Model, m_AccelModel;
        Glib::RefPtr<Gtk::ListStore> m_ComboModel;

        std::tuple<guint, GdkModifierType, guint> m_AccelOut;
    };
}
