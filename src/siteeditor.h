#ifndef _SITEEDITOR_H_
#define _SITEEDITOR_H_

#include <gtkmm.h>

#include "booru/site.h"

namespace AhoViewer
{
    class SiteEditor : public Gtk::TreeView
    {
    public:
        friend class CellRendererIcon;

        SiteEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~SiteEditor();

        sigc::signal<void> signal_edited() const { return m_SignalEdited; }
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
                m_LoadingProperty.get_proxy().signal_changed().connect([ this ]()
                        { m_SpinnerRenderer->property_active() = m_LoadingProperty.get_value(); });
                m_PulseProperty.get_proxy().signal_changed().connect([ this ]()
                        { m_SpinnerRenderer->property_pulse() = m_PulseProperty.get_value(); });
                m_PixbufProperty.get_proxy().signal_changed().connect([ this ]()
                        { m_PixbufRenderer->property_pixbuf() = m_PixbufProperty.get_value(); });

                m_SiteEditor->get_toplevel()->signal_show().connect([ this ]()
                        {
                            m_SpinnerConn = Glib::signal_timeout().connect(
                                    sigc::mem_fun(*this, &CellRendererIcon::update_spinner), 63);
                        });
                m_SiteEditor->get_toplevel()->signal_hide().connect([ this ]() { m_SpinnerConn.disconnect(); });
            }
            virtual ~CellRendererIcon() = default;

            Glib::PropertyProxy<bool> property_loading()
                { return m_LoadingProperty.get_proxy(); }
            Glib::PropertyProxy<unsigned int> property_pulse()
                { return m_PulseProperty.get_proxy(); }
            Glib::PropertyProxy<Glib::RefPtr<Gdk::Pixbuf>> property_pixbuf()
                { return m_PixbufProperty.get_proxy(); }
        protected:
            virtual void get_size_vfunc(Gtk::Widget &widget,
                                        const Gdk::Rectangle *cell_area,
                                        int *x_offset,
                                        int *y_offset,
                                        int *width,
                                        int *height) const
            {
                if (m_LoadingProperty.get_value())
                    m_SpinnerRenderer->get_size(widget, *cell_area, *x_offset, *y_offset, *width, *height);
                else
                    m_PixbufRenderer->get_size(widget, *cell_area, *x_offset, *y_offset, *width, *height);

                Gtk::CellRenderer::get_size_vfunc(widget, cell_area, x_offset, y_offset, width, height);
            }
            virtual void render_vfunc(const Glib::RefPtr<Gdk::Drawable> &window,
                                      Widget &widget,
                                      const Gdk::Rectangle &bg_area,
                                      const Gdk::Rectangle &cell_area,
                                      const Gdk::Rectangle &expose_area,
                                      Gtk::CellRendererState flags)
            {
                Gtk::CellRenderer::render_vfunc(window, widget, bg_area, cell_area, expose_area, flags);

                if (m_LoadingProperty.get_value())
                    m_SpinnerRenderer->render(Glib::RefPtr<Gdk::Window>::cast_static(window), widget,
                                             bg_area, cell_area, expose_area, flags);
                else
                    m_PixbufRenderer->render(Glib::RefPtr<Gdk::Window>::cast_static(window), widget,
                                             bg_area, cell_area, expose_area, flags);
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
            ModelColumns() { add(icon); add(loading); add(pulse); add(name); add(url); add(site); }
            Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
            Gtk::TreeModelColumn<bool> loading;
            Gtk::TreeModelColumn<unsigned int> pulse;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<std::string> url;
            Gtk::TreeModelColumn<std::shared_ptr<Booru::Site>> site;
        };

        void add_row();
        void delete_site();

        void on_name_edited(const std::string &p, const std::string &text);
        void on_url_edited(const std::string &p, const std::string &text);

        bool is_name_unique(const Gtk::TreeIter &iter, const std::string &name) const;
        void add_edit_site(const Gtk::TreeIter &iter);

        void update_edited_site_icon();
        void on_site_checked();

        ModelColumns m_Columns;
        Glib::RefPtr<Gtk::ListStore> m_Model;

        Gtk::TreeView::Column *m_NameColumn,
                              *m_UrlColumn;

        std::vector<std::shared_ptr<Booru::Site>>& m_Sites;
        Glib::RefPtr<Gdk::Pixbuf> m_ErrorPixbuf;

        std::shared_ptr<Booru::Site> m_SiteCheckSite;
        Gtk::TreeIter m_SiteCheckIter;
        bool m_SiteCheckEdit, m_SiteCheckEditSuccess;

        Glib::Threads::Thread *m_SiteCheckThread;
        Glib::Dispatcher m_SignalSiteChecked;

        sigc::signal<void> m_SignalEdited;
    };
}

#endif /* _SITEEDITOR_H_ */
