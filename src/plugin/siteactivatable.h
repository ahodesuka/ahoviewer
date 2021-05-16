#pragma once

#include <glib-object.h>

#ifdef _WIN32
#define AHOVIEWER_EXPORT __declspec(dllexport)
#else // !_WIN32
#define AHOVIEWER_EXPORT
#endif // !_WIN32

G_BEGIN_DECLS

#define AHOVIEWER_TYPE_POST                (ahoviewer_post_get_type())
#define AHOVIEWER_TYPE_NOTE                (ahoviewer_note_get_type())
#define AHOVIEWER_TYPE_TAG                 (ahoviewer_tag_get_type())

typedef struct _AhoviewerPosts AhoviewerPosts;
typedef struct _AhoviewerPost AhoviewerPost;
typedef struct _AhoviewerNote AhoviewerNote;
typedef struct _AhoviewerTag AhoviewerTag;

// These match the AhoViewer::Booru::Tag::Type enum class values in src/util.h
typedef enum
{
    AHOVIEWER_TAG_ARTIST     = 0,
    AHOVIEWER_TAG_CHARACTER  = 1,
    AHOVIEWER_TAG_COPYRIGHT  = 2,
    AHOVIEWER_TAG_METADATA   = 3,
    AHOVIEWER_TAG_GENERAL    = 4,
    AHOVIEWER_TAG_DEPRECATED = 5,
    AHOVIEWER_TAG_UNKNOWN    = 6,
} AhoviewerTagType;

struct _AhoviewerPosts
{
    gint ref_count;

    GPtrArray* posts;
    guint64 count;
    gchar* error;
};

struct _AhoviewerPost
{
    gint ref_count;

    gchar* image_url;
    gchar* thumb_url;
    gchar* post_url;
    gchar* notes_url;
    GPtrArray* tags;

    gint64 date;
    gchar* source;
    gchar* rating;
    gchar* score;
};

struct _AhoviewerNote
{
    gint ref_count;

    gchar* body;
    gint32 w, h, x, y;
};

struct _AhoviewerTag
{
    gint ref_count;

    gchar* tag;
    AhoviewerTagType type;
};

AHOVIEWER_EXPORT GType ahoviewer_posts_get_type(void) G_GNUC_CONST;
AHOVIEWER_EXPORT AhoviewerPosts* ahoviewer_posts_new(GPtrArray* posts, guint64 count, gchar* error);
AHOVIEWER_EXPORT AhoviewerPosts* ahoviewer_posts_ref(AhoviewerPosts* post);
AHOVIEWER_EXPORT void ahoviewer_posts_unref(AhoviewerPosts* post);

AHOVIEWER_EXPORT GType ahoviewer_post_get_type(void) G_GNUC_CONST;
AHOVIEWER_EXPORT AhoviewerPost* ahoviewer_post_new(GPtrArray* tags);
AHOVIEWER_EXPORT AhoviewerPost* ahoviewer_post_ref(AhoviewerPost* post);
AHOVIEWER_EXPORT void ahoviewer_post_unref(AhoviewerPost* post);

AHOVIEWER_EXPORT GType ahoviewer_note_get_type(void) G_GNUC_CONST;
AHOVIEWER_EXPORT AhoviewerNote* ahoviewer_note_new(gchar* body, gint32 w, gint32 h, gint32 x, gint32 y);
AHOVIEWER_EXPORT AhoviewerNote* ahoviewer_note_ref(AhoviewerNote* note);
AHOVIEWER_EXPORT void ahoviewer_note_unref(AhoviewerNote* note);

AHOVIEWER_EXPORT GType ahoviewer_tag_get_type(void) G_GNUC_CONST;
AHOVIEWER_EXPORT AhoviewerTag* ahoviewer_tag_new(gchar* tag, AhoviewerTagType type);
AHOVIEWER_EXPORT AhoviewerTag* ahoviewer_tag_ref(AhoviewerTag* tag);
AHOVIEWER_EXPORT void ahoviewer_tag_unref(AhoviewerTag* tag);

#define AHOVIEWER_SITE_TYPE_ACTIVATABLE         (ahoviewer_site_activatable_get_type())
#define AHOVIEWER_SITE_ACTIVATABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), AHOVIEWER_SITE_TYPE_ACTIVATABLE, AhoviewerSiteActivatable))
#define AHOVIEWER_SITE_ACTIVATABLE_IFACE(k)     (G_TYPE_CHECK_CLASS_CAST((k), AHOVIEWER_SITE_TYPE_ACTIVATABLE, AhoviewerSiteActivatableInterface))
#define AHOVIEWER_IS_SITE_ACTIVATABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), AHOVIEWER_SITE_TYPE_ACTIVATABLE))
#define AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), AHOVIEWER_SITE_TYPE_ACTIVATABLE, AhoviewerSiteActivatableInterface))

typedef struct _AhoviewerSiteActivatable AhoviewerSiteActivatable;
typedef struct _AhoviewerSiteActivatableInterface AhoviewerSiteActivatableInterface;

struct _AhoviewerSiteActivatableInterface
{
    const gchar* (*get_test_uri)(AhoviewerSiteActivatable* activatable);
    const gchar* (*get_posts_uri)(AhoviewerSiteActivatable* activatable, const gchar* tags, size_t page, size_t limit);
    const gchar* (*get_register_url)(AhoviewerSiteActivatable* activatable, const gchar* url);
    const gchar* (*get_icon_url)(AhoviewerSiteActivatable* activatable, const gchar* url);
    AhoviewerPosts* (*parse_post_data)(AhoviewerSiteActivatable* activatable, const unsigned char* data, const size_t size, const gchar* url, const gboolean samples);
    GPtrArray* (*parse_note_data)(AhoviewerSiteActivatable* activatable, const unsigned char* data, const size_t size);
};

AHOVIEWER_EXPORT GType ahoviewer_site_activatable_get_type(void) G_GNUC_CONST;

AHOVIEWER_EXPORT
const gchar* ahoviewer_site_activatable_get_test_uri(AhoviewerSiteActivatable* activatable);

AHOVIEWER_EXPORT
const gchar* ahoviewer_site_activatable_get_posts_uri(AhoviewerSiteActivatable* activatable, const gchar* tags, size_t page, size_t limit);

AHOVIEWER_EXPORT
const gchar* ahoviewer_site_activatable_get_register_url(AhoviewerSiteActivatable* activatable, const gchar* url);

AHOVIEWER_EXPORT
const gchar* ahoviewer_site_activatable_get_icon_url(AhoviewerSiteActivatable* activatable, const gchar* url);

AHOVIEWER_EXPORT
AhoviewerPosts* ahoviewer_site_activatable_parse_post_data(
    AhoviewerSiteActivatable* activatable, const unsigned char* data, const size_t size, const gchar* url, const gboolean samples);

AHOVIEWER_EXPORT
GPtrArray* ahoviewer_site_activatable_parse_note_data(AhoviewerSiteActivatable* activatable, const unsigned char* data, const size_t size);

G_END_DECLS
