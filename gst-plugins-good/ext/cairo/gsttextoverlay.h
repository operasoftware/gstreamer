
#ifndef __GST_TEXTOVERLAY_H__
#define __GST_TEXTOVERLAY_H__

#include <gst/gst.h>
#include <cairo.h>

G_BEGIN_DECLS

#define GST_TYPE_TEXTOVERLAY           (gst_textoverlay_get_type())
#define GST_TEXTOVERLAY(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                        GST_TYPE_TEXTOVERLAY, GstTextOverlay))
#define GST_TEXTOVERLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),\
                                        GST_TYPE_ULAW, GstTextOverlay))
#define GST_TEXTOVERLAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                        GST_TYPE_TEXTOVERLAY, GstTextOverlayClass))
#define GST_IS_TEXTOVERLAY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                        GST_TYPE_TEXTOVERLAY))
#define GST_IS_TEXTOVERLAY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                        GST_TYPE_TEXTOVERLAY))

typedef struct _GstTextOverlay      GstTextOverlay;
typedef struct _GstTextOverlayClass GstTextOverlayClass;

typedef enum _GstTextOverlayVAlign GstTextOverlayVAlign;
typedef enum _GstTextOverlayHAlign GstTextOverlayHAlign;

enum _GstTextOverlayVAlign {
    GST_TEXT_OVERLAY_VALIGN_BASELINE,
    GST_TEXT_OVERLAY_VALIGN_BOTTOM,
    GST_TEXT_OVERLAY_VALIGN_TOP
};

enum _GstTextOverlayHAlign {
    GST_TEXT_OVERLAY_HALIGN_LEFT,
    GST_TEXT_OVERLAY_HALIGN_CENTER,
    GST_TEXT_OVERLAY_HALIGN_RIGHT
};


struct _GstTextOverlay {
    GstElement            element;

    GstPad               *video_sinkpad;
    GstPad               *text_sinkpad;
    GstPad               *srcpad;
    gint                  width;
    gint                  height;

    GstTextOverlayVAlign  valign;
    GstTextOverlayHAlign  halign;
    gint                  x0;
    gint                  y0;
    gchar		 *default_text;

    cairo_t *cr;
    guchar *text_fill_image;
    guchar *text_outline_image;
    int text_height;

    GstBuffer		 *current_buffer;
    GstBuffer		 *next_buffer;
    gboolean		  need_render;

    gchar *font;
    int slant;
    int weight;
    double scale;
};

struct _GstTextOverlayClass {
    GstElementClass parent_class;
};

GType gst_textoverlay_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_TEXTOVERLAY_H */
