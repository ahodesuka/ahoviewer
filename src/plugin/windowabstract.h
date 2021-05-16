#pragma once

#include <glib-object.h>

#ifdef _WIN32
#define AHOVIEWER_EXPORT __declspec(dllexport)
#else // !_WIN32
#define AHOVIEWER_EXPORT
#endif // !_WIN32

G_BEGIN_DECLS

#define AHOVIEWER_WINDOW_TYPE_ABSTRACT         (ahoviewer_window_abstract_get_type())
#define AHOVIEWER_WINDOW_ABSTRACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), AHOVIEWER_WINDOW_TYPE_ABSTRACT, AhoviewerWindowAbstract))
#define AHOVIEWER_WINDOW_ABSTRACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), AHOVIEWER_WINDOW_TYPE_ABSTRACT, AhoviewerWindowAbstractClass))
#define AHOVIEWER_IS_WINDOW_ABSTRACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), AHOVIEWER_WINDOW_TYPE_ABSTRACT))
#define AHOVIEWER_IS_WINDOW_ABSTRACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((o), AHOVIEWER_WINDOW_TYPE_ABSTRACT))
#define AHOVIEWER_WINDOW_ABSTRACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), AHOVIEWER_WINDOW_TYPE_ABSTRACT, AhoviewerWindowAbstractClass))

typedef struct _AhoviewerWindowAbstract          AhoviewerWindowAbstract;
typedef struct _AhoviewerWindowAbstractPrivate   AhoviewerWindowAbstractPrivate;
typedef struct _AhoviewerWindowAbstractClass     AhoviewerWindowAbstractClass;

struct _AhoviewerWindowAbstract
{
    GObject parent_instance;

    AhoviewerWindowAbstractPrivate* priv;
};

struct _AhoviewerWindowAbstractClass
{
    GObjectClass parent_class;

    void (*activate)(AhoviewerWindowAbstract* abstract);
    void (*deactivate)(AhoviewerWindowAbstract* abstract);

    void (*open_file)(AhoviewerWindowAbstract* abstract, const gchar* path);
};

AHOVIEWER_EXPORT GType ahoviewer_window_abstract_get_type(void) G_GNUC_CONST;

AHOVIEWER_EXPORT void ahoviewer_window_abstract_open_file(AhoviewerWindowAbstract* abstract, const gchar* path);

G_END_DECLS
