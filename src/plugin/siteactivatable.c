#include "siteactivatable.h"

G_DEFINE_BOXED_TYPE(AhoviewerPosts, ahoviewer_posts, ahoviewer_posts_ref, ahoviewer_posts_unref)

/**
 * ahoviewer_posts_new:
 * @posts: (element-type AhoviewerPost) (transfer full)
 * @count: A #guint64.
 * @error: (transfer full): A #utf8.
 *
 * Allocates a new #AhoviewerPosts and initializes it's elements to zero.
 *
 * Returns: a new #AhoviewerPosts with the given posts. The newly allocated #AhoviewerPosts should
 *   be freed with ahoviewer_posts_unref().
 */
AhoviewerPosts* ahoviewer_posts_new(GPtrArray* posts, guint64 count, gchar* error)
{
    AhoviewerPosts* s_posts = g_slice_new0(AhoviewerPosts);

    s_posts->ref_count = 1;
    s_posts->posts = posts;
    s_posts->count = count;
    s_posts->error = error;

    g_ptr_array_set_free_func(posts, (GDestroyNotify)ahoviewer_post_unref);

    return s_posts;
}

/**
 * ahoviewer_posts_ref:
 * @post: A #AhoviewerPosts
 *
 * Copies a #AhoviewerPosts.
 *
 * Returns: A copy of @post
 */
AhoviewerPosts* ahoviewer_posts_ref(AhoviewerPosts* posts)
{
    g_return_val_if_fail(posts != NULL, NULL);

    posts->ref_count += 1;

    return posts;
}

/**
 * ahoviewer_posts_unref:
 * @post: a #AhoviewerPosts
 *
 * Decrements the reference count of the #AhoviewerPosts.
 */
void ahoviewer_posts_unref(AhoviewerPosts* posts)
{
    g_return_if_fail(posts != NULL);

    posts->ref_count -= 1;
    if (posts->ref_count > 0)
        return;

    g_ptr_array_unref(posts->posts);
    if (posts->error)
        g_free(posts->error);

    g_slice_free(AhoviewerPosts, posts);
}

G_DEFINE_BOXED_TYPE(AhoviewerPost, ahoviewer_post, ahoviewer_post_ref, ahoviewer_post_unref)

/**
 * ahoviewer_post_new:
 * @tags: (element-type AhoviewerTag) (transfer full)
 *
 * Allocates a new #AhoviewerPost and initializes it's elements to zero.
 *
 * Returns: a new #AhoviewerPost with the given tags. The newly allocated #AhoviewerPost should
 *   be freed with ahoviewer_post_unref().
 */
AhoviewerPost* ahoviewer_post_new(GPtrArray* tags)
{
    AhoviewerPost* post = g_slice_new0(AhoviewerPost);

    post->ref_count = 1;
    post->tags = tags;

    g_ptr_array_set_free_func(post->tags, (GDestroyNotify)ahoviewer_tag_unref);

    return post;
}

/**
 * ahoviewer_post_ref:
 * @post: A #AhoviewerPost
 *
 * Copies a #AhoviewerPost.
 *
 * Returns: A copy of @post
 */
AhoviewerPost* ahoviewer_post_ref(AhoviewerPost* post)
{
    g_return_val_if_fail(post != NULL, NULL);

    post->ref_count += 1;

    return post;
}

/**
 * ahoviewer_post_unref:
 * @post: a #AhoviewerPost
 *
 * Decrements the reference count of the #AhoviewerPost.
 */
void ahoviewer_post_unref(AhoviewerPost* post)
{
    g_return_if_fail(post != NULL);

    post->ref_count -= 1;
    if (post->ref_count > 0)
        return;

    g_ptr_array_unref(post->tags);
    g_slice_free(AhoviewerPost, post);
}

G_DEFINE_BOXED_TYPE(AhoviewerNote, ahoviewer_note, ahoviewer_note_ref, ahoviewer_note_unref)

/**
 * ahoviewer_note_new:
 * @body: (transfer full): A #utf8.
 * @w: A #gint32.
 * @h: A #gint32.
 * @x: A #gint32.
 * @y: A #gint32.
 *
 * Allocates a new #AhoviewerNote-struct.
 *
 * Returns: a new #AhoviewerNote. The newly allocated #AhoviewerNote should
 *   be freed with ahoviewer_note_unref().
 */
AhoviewerNote* ahoviewer_note_new(gchar* body, gint32 w, gint32 h, gint32 x, gint32 y)
{
    AhoviewerNote* note = g_slice_new0(AhoviewerNote);

    note->ref_count = 1;
    note->body = body;
    note->w = w;
    note->h = h;
    note->x = x;
    note->y = y;

    return note;
}

/**
 * ahoviewer_note_ref:
 * @note: A #AhoviewerNote
 *
 * Copies a #AhoviewerNote.
 *
 * Returns: A copy of @note
 */
AhoviewerNote* ahoviewer_note_ref(AhoviewerNote* note)
{
    g_return_val_if_fail(note != NULL, NULL);

    note->ref_count += 1;

    return note;
}

/**
 * ahoviewer_note_unref:
 * @note: a #AhoviewerNote
 *
 * Decrements the reference count of the #AhoviewerNote.
 */
void ahoviewer_note_unref(AhoviewerNote* note)
{
    g_return_if_fail(note != NULL);

    note->ref_count -= 1;
    if (note->ref_count > 0)
        return;

    if (G_LIKELY(note->body))
        g_free(note->body);

    g_slice_free(AhoviewerNote, note);
}

G_DEFINE_BOXED_TYPE(AhoviewerTag, ahoviewer_tag, ahoviewer_tag_ref, ahoviewer_tag_unref)

/**
 * ahoviewer_tag_new:
 * @tag: (transfer full): A #utf8.
 * @type: A #AhoviewerTagType.
 *
 * Allocates a new #AhoviewerTag-struct.
 *
 * Returns: a new #AhoviewerTag. The newly allocated #AhoviewerTag should
 *   be freed with ahoviewer_tag_unref().
 */
AhoviewerTag* ahoviewer_tag_new(gchar* tag, AhoviewerTagType type)
{
    AhoviewerTag* n_tag = g_slice_new0(AhoviewerTag);

    n_tag->ref_count = 1;
    n_tag->tag = tag;
    n_tag->type = type;

    return n_tag;
}

/**
 * ahoviewer_tag_ref:
 * @tag: A #AhoviewerTag
 *
 * Copies a #AhoviewerTag.
 *
 * Returns: A copy of @tag
 */
AhoviewerTag* ahoviewer_tag_ref(AhoviewerTag* tag)
{
    g_return_val_if_fail(tag != NULL, NULL);

    tag->ref_count += 1;

    return tag;
}

/**
 * ahoviewer_tag_unref:
 * @tag: a #AhoviewerTag
 *
 * Decrements the reference count of the #AhoviewerTag.
 */
void ahoviewer_tag_unref(AhoviewerTag* tag)
{
    g_return_if_fail(tag != NULL);

    tag->ref_count -= 1;
    if (tag->ref_count > 0)
        return;

    if (G_LIKELY(tag->tag))
        g_free(tag->tag);

    g_slice_free(AhoviewerTag, tag);
}

G_DEFINE_INTERFACE(AhoviewerSiteActivatable, ahoviewer_site_activatable, G_TYPE_OBJECT)

static void ahoviewer_site_activatable_default_init(AhoviewerSiteActivatableInterface *iface)
{
    static gboolean initialized = FALSE;

    if (!initialized)
        initialized = TRUE;
}

/**
 * ahoviewer_site_activatable_get_test_uri:
 * @activatable: A #AhoviewerSiteActivatable.
 *
 * Returns: A #utf8 containing the uri to test.
 */
const gchar* ahoviewer_site_activatable_get_test_uri(AhoviewerSiteActivatable* activatable)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->get_test_uri))
        return iface->get_test_uri(activatable);

    return NULL;
}

/**
 * ahoviewer_site_activatable_get_posts_uri:
 * @activatable: A #AhoviewerSiteActivatable.
 * @tags: A #utf8.
 * @page: A #guint64.
 * @limit: A #guint64.
 *
 * Returns: A #utf8 containing the posts uri.
 */
const gchar* ahoviewer_site_activatable_get_posts_uri(AhoviewerSiteActivatable* activatable, const gchar* tags, size_t page, size_t limit)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->get_test_uri))
        return iface->get_posts_uri(activatable, tags, page, limit);

    return NULL;
}

/**
 * ahoviewer_site_activatable_get_register_url:
 * @activatable: A #AhoviewerSiteActivatable.
 * @url: A #utf8.
 *
 * Returns: A #utf8 containing the url for registration.
 */
const gchar* ahoviewer_site_activatable_get_register_url(AhoviewerSiteActivatable* activatable, const gchar* url)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->get_register_url))
        return iface->get_register_url(activatable, url);

    return "";
}

/**
 * ahoviewer_site_activatable_get_icon_url:
 * @activatable: A #AhoviewerSiteActivatable.
 * @url: A #utf8.
 *
 * Returns: A #utf8 containing the domain where the icon should be looked for.
 */
const gchar* ahoviewer_site_activatable_get_icon_url(AhoviewerSiteActivatable* activatable, const gchar* url)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->get_icon_url))
        return iface->get_icon_url(activatable, url);

    return url;
}

/**
 * ahoviewer_site_activatable_parse_post_data:
 * @activatable: A #AhoviewerSiteActivatable.
 * @data: (array length=size): A #guint8.
 * @url: A #utf8.
 * @samples: A #gboolean.
 *
 * Returns: (transfer full): A #AhoviewerPosts that should be freed with ahoviewer_posts_unref.
 */
AhoviewerPosts* ahoviewer_site_activatable_parse_post_data(AhoviewerSiteActivatable* activatable,
                                                           const unsigned char* data,
                                                           const size_t size,
                                                           const gchar* url,
                                                           const gboolean samples)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->parse_post_data))
        return iface->parse_post_data(activatable, data, size, url, samples);

    return NULL;
}

/**
 * ahoviewer_site_activatable_parse_note_data:
 * @activatable: A #AhoviewerSiteActivatable.
 * @data: (array length=size): A #guint8.
 *
 * Returns: (element-type AhoviewerNote) (transfer full): Array of notes, should be freed with
 *   g_ptr_array_unref.
 */
GPtrArray* ahoviewer_site_activatable_parse_note_data(AhoviewerSiteActivatable* activatable,
                                                      const unsigned char* data,
                                                      const size_t size)
{
    AhoviewerSiteActivatableInterface* iface;

    g_return_val_if_fail(AHOVIEWER_IS_SITE_ACTIVATABLE(activatable), NULL);

    iface = AHOVIEWER_SITE_ACTIVATABLE_GET_IFACE(activatable);
    if (G_LIKELY(iface->parse_note_data))
    {
        GPtrArray* notes = iface->parse_note_data(activatable, data, size);

        if (G_LIKELY(notes))
            g_ptr_array_set_free_func(notes, (GDestroyNotify)ahoviewer_note_unref);

        return notes;
    }

    return NULL;
}
