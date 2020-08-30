#include "windowabstract.h"
#include "mainwindowinterface.h"

#include <libpeas/peas-activatable.h>

struct _AhoviewerWindowAbstractPrivate
{
    CMainWindow* main_window;
};

enum
{
    PROP_0,
    PROP_MAIN_WINDOW,
    N_PROPERTIES,
    PROP_OBJECT,
};

static GParamSpec *props[N_PROPERTIES] = { NULL };

static void peas_activatable_iface_init(PeasActivatableInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(AhoviewerWindowAbstract,
                                 ahoviewer_window_abstract,
                                 G_TYPE_OBJECT,
                                 G_ADD_PRIVATE(AhoviewerWindowAbstract)
                                 G_IMPLEMENT_INTERFACE(PEAS_TYPE_ACTIVATABLE, peas_activatable_iface_init))

static void ahoviewer_window_abstract_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);

static void ahoviewer_window_abstract_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);

static void ahoviewer_window_abstract_class_init(AhoviewerWindowAbstractClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    obj_class->set_property = ahoviewer_window_abstract_set_property;
    obj_class->get_property = ahoviewer_window_abstract_get_property;

    klass->open_file = ahoviewer_window_abstract_open_file;

    props[PROP_MAIN_WINDOW] = g_param_spec_pointer(
            "main-window", "Main Window", "Main Window", G_PARAM_READWRITE);

    g_object_class_install_properties(obj_class, N_PROPERTIES, props);
    g_object_class_override_property(obj_class, PROP_OBJECT, "object");
}

static void ahoviewer_window_abstract_init(AhoviewerWindowAbstract* abstract)
{
    abstract->priv = ahoviewer_window_abstract_get_instance_private(abstract);

    g_object_set(abstract,
            "main-window", NULL,
            NULL);
}

/**
 * ahoviewer_window_abstract_open_file
 * @abstract: A #AhoviewerWindowAbstract.
 * @path: A #utf8.
 *
 * Opens a file in the ahoviewer Main Window.
 */
void ahoviewer_window_abstract_open_file(AhoviewerWindowAbstract* abstract, const gchar* path)
{
#ifndef GIR_DUMMY
    if (G_LIKELY(abstract->priv->main_window))
        mainwindow_open_file(abstract->priv->main_window, path);
#endif // !GIR_DUMMY
}

static void ahoviewer_window_abstract_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    AhoviewerWindowAbstract* abstract = AHOVIEWER_WINDOW_ABSTRACT(object);

    switch (prop_id)
    {
        case PROP_MAIN_WINDOW:
            abstract->priv->main_window = g_value_get_pointer(value);
            break;
        case PROP_OBJECT:
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ahoviewer_window_abstract_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    AhoviewerWindowAbstract* abstract = AHOVIEWER_WINDOW_ABSTRACT(object);

    switch (prop_id)
    {
        case PROP_MAIN_WINDOW:
            g_value_set_pointer(value, abstract->priv->main_window);
            break;
        case PROP_OBJECT:
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void impl_activate(PeasActivatable* activatable)
{
    g_return_if_fail(AHOVIEWER_IS_WINDOW_ABSTRACT(activatable));

    AHOVIEWER_WINDOW_ABSTRACT_GET_CLASS(activatable)->activate(AHOVIEWER_WINDOW_ABSTRACT(activatable));
}

static void impl_deactivate(PeasActivatable* activatable)
{
    g_return_if_fail(AHOVIEWER_IS_WINDOW_ABSTRACT(activatable));

    AHOVIEWER_WINDOW_ABSTRACT_GET_CLASS(activatable)->deactivate(AHOVIEWER_WINDOW_ABSTRACT(activatable));
}

static void peas_activatable_iface_init(PeasActivatableInterface *iface)
{
    iface->activate = impl_activate;
    iface->deactivate = impl_deactivate;
}
