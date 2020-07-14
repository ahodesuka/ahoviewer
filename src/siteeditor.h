#ifndef _SITEEDITOR_H_
#define _SITEEDITOR_H_

#include <gtkmm.h>

#include <thread>

namespace AhoViewer
{
    namespace Booru { class Site; }
    class SiteEditor : public Gtk::TreeView
    {
    public:
        friend class CellRendererIcon;

        SiteEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~SiteEditor() override;

        sigc::signal<void> signal_edited() const { return m_SignalEdited; }
    protected:
        virtual bool on_key_release_event(GdkEventKey *e) override;

        void on_cursor_changed() override;
    private:
        class CellRendererIcon : public Gtk::CellRenderer/*{{{*/
        {
        public:
            CellRendererIcon(SiteEditor *const editor)
              : Glib::ObjectBase(typeid(CellRendererIcon)),
                Gtk::CellRenderer(),
                m_LoadingProperty(*this, "loading", false),
                m_PulseProperty(*this, "pulse", 0u),
                m_PixbufProperty(*this, "pixbuf"),
                m_SiteEditor(editor),
                m_PixbufRenderer(Gtk::manage(new Gtk::CellRendererPixbuf())),
                m_SpinnerRenderer(Gtk::manage(new Gtk::CellRendererSpinner()))
            {
                m_LoadingProperty.get_proxy().signal_changed().connect([&]()
                        { m_SpinnerRenderer->property_active() = m_LoadingProperty.get_value(); });
                m_PulseProperty.get_proxy().signal_changed().connect([&]()
                        { m_SpinnerRenderer->property_pulse() = m_PulseProperty.get_value(); });
                m_PixbufProperty.get_proxy().signal_changed().connect([&]()
                        { m_PixbufRenderer->property_pixbuf() = m_PixbufProperty.get_value(); });

                m_SiteEditor->get_toplevel()->signal_show().connect([&]()
                        {
                            m_SpinnerConn = Glib::signal_timeout().connect(
                                    sigc::mem_fun(*this, &CellRendererIcon::update_spinner), 63);
                        });
                m_SiteEditor->get_toplevel()->signal_hide().connect([&]() { m_SpinnerConn.disconnect(); });
            }
            virtual ~CellRendererIcon() override = default;

            Glib::PropertyProxy<bool> property_loading()
                { return m_LoadingProperty.get_proxy(); }
            Glib::PropertyProxy<unsigned int> property_pulse()
                { return m_PulseProperty.get_proxy(); }
            Glib::PropertyProxy<Glib::RefPtr<Gdk::Pixbuf>> property_pixbuf()
                { return m_PixbufProperty.get_proxy(); }

            virtual void get_preferred_width_vfunc(Gtk::Widget &widget,
                                                   int &minimum_width,
                                                   int &natural_width) const override
            {
                if (m_LoadingProperty.get_value())
                    m_SpinnerRenderer->get_preferred_width(widget, minimum_width, natural_width);
                else
                    m_PixbufRenderer->get_preferred_width(widget, minimum_width, natural_width);
            }
            virtual void get_preferred_height_vfunc(Gtk::Widget &widget,
                                                   int &minimum_height,
                                                   int &natural_height) const override
            {
                if (m_LoadingProperty.get_value())
                    m_SpinnerRenderer->get_preferred_height(widget, minimum_height, natural_height);
                else
                    m_PixbufRenderer->get_preferred_height(widget, minimum_height, natural_height);
            }
            virtual void render_vfunc(const Cairo::RefPtr< ::Cairo::Context > &cr,
                                      Gtk::Widget& widget,
                                      const Gdk::Rectangle& background_area,
                                      const Gdk::Rectangle& cell_area,
                                      Gtk::CellRendererState flags) override
            {
                Gtk::CellRenderer::render_vfunc(cr, widget, background_area, cell_area, flags);

                if (m_LoadingProperty.get_value())
                    m_SpinnerRenderer->render(cr, widget, background_area, cell_area, flags);
                else
                    m_PixbufRenderer->render(cr, widget, background_area, cell_area, flags);
            }
        private:
            bool update_spinner()
            {
                Gtk::TreeModel::Children children = m_SiteEditor->m_Model->children();
                for (Gtk::TreeIter i = children.begin(); i != children.end(); ++i)
                {
                    if (i->get_value(m_SiteEditor->m_Columns.loading))
                    {
                        unsigned int pulse = i->get_value(m_SiteEditor->m_Columns.pulse);
                        i->set_value(m_SiteEditor->m_Columns.pulse, pulse >= 12 ? 0u : ++pulse);
                    }
                }

                return true;
            }

            Glib::Property<bool> m_LoadingProperty;
            Glib::Property<unsigned int> m_PulseProperty;
            Glib::Property<Glib::RefPtr<Gdk::Pixbuf>> m_PixbufProperty;
            SiteEditor *const m_SiteEditor;
            Gtk::CellRendererPixbuf *m_PixbufRenderer;
            Gtk::CellRendererSpinner *m_SpinnerRenderer;
            sigc::connection m_SpinnerConn;
        };/*}}}*/
        struct ModelColumns : public Gtk::TreeModelColumnRecord
        {
            ModelColumns() { add(icon); add(loading); add(pulse); add(name); add(url); add(samples); add(site); }
            Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
            Gtk::TreeModelColumn<bool> loading;
            Gtk::TreeModelColumn<unsigned int> pulse;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<std::string> url;
            Gtk::TreeModelColumn<bool> samples;
            Gtk::TreeModelColumn<std::shared_ptr<Booru::Site>> site;
        };

        void add_row();
        void delete_site();

        void on_name_edited(const std::string &p, const std::string &text);
        void on_url_edited(const std::string &p, const std::string &text);
        void on_samples_toggled(const std::string &p);

        bool is_name_unique(const Gtk::TreeIter &iter, const std::string &name) const;
        void add_edit_site(const Gtk::TreeIter &iter);

        void update_edited_site_icon();
        void on_site_checked();

        void on_username_edited();
        void on_password_edited();

        ModelColumns m_Columns;
        Glib::RefPtr<Gtk::ListStore> m_Model;

        Gtk::TreeViewColumn *m_NameColumn,
                            *m_UrlColumn,
                            *m_SampleColumn;
        Gtk::LinkButton *m_RegisterButton;
        Gtk::Entry *m_UsernameEntry,
                   *m_PasswordEntry;
        Gtk::Label *m_PasswordLabel;

        std::vector<std::shared_ptr<Booru::Site>>& m_Sites;
        Glib::RefPtr<Gdk::Pixbuf> m_ErrorPixbuf;

        std::shared_ptr<Booru::Site> m_SiteCheckSite;
        Gtk::TreeIter m_SiteCheckIter;
        bool m_SiteCheckEdit, m_SiteCheckEditSuccess;

        std::thread m_SiteCheckThread;
        Glib::Dispatcher m_SignalSiteChecked;

        sigc::connection m_UsernameConn,
                         m_PasswordConn;

        sigc::signal<void> m_SignalEdited;
    };
}

#endif /* _SITEEDITOR_H_ */
