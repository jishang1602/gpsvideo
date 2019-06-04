#include "config.h"

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>

typedef struct _GeoImagePipe GeoImagePipe;
struct _GeoImagePipe {
	GstElement *pipe;
	GstElement *src;
	GstElement *queue;
	GstElement *imageenc;
	GstElement *metadata;
	GstElement *sink;
};

GeoImagePipe gis;
GMainLoop *loop;

static gboolean
set_tag(GstElement *gstjpegenc, gpointer data)
{
GeoImagePipe *gs=(GeoImagePipe *)data;
GstTagSetter *e=GST_TAG_SETTER(gs->metadata);
GstTagList *tl;
GstEvent *te;
gdouble lat, lon;
gchar *cmt;

lat=g_random_double_range(-90,90);
lon=g_random_double_range(-180,180);

g_debug("Geo: %f, %f\n", lat, lon);

cmt=g_strdup_printf("Geo: %f, %f", lat, lon);

gst_tag_setter_reset_tags(e);
gst_tag_setter_add_tags(e,
	GST_TAG_MERGE_REPLACE,
	GST_TAG_COMMENT, cmt,
	GST_TAG_GEO_LOCATION_LATITUDE, lat,
	GST_TAG_GEO_LOCATION_LONGITUDE, lon,
	GST_TAG_GEO_LOCATION_NAME, "Testing", NULL);

return TRUE;
}

static gboolean generate_geotag(gpointer data)
{
set_tag(gis.metadata, &gis);

return TRUE;
}

static void
geoimagepipe()
{
gis.pipe=gst_pipeline_new("pipeline");

gis.src=gst_element_factory_make("v4l2src", "video");
gis.queue=gst_element_factory_make("queue", "queue");
gis.imageenc=gst_element_factory_make("jpegenc", "jpeg");
gis.metadata=gst_element_factory_make("jifmux", "meta");
gis.sink=gst_element_factory_make("multifilesink", "sink");

gst_bin_add_many(GST_BIN(gis.pipe), gis.src, gis.queue, gis.imageenc, gis.metadata, gis.sink, NULL);
gst_element_link_many(gis.src, gis.queue, gis.imageenc, gis.metadata, gis.sink, NULL);

g_object_set(gis.src, "num-buffers", 25, NULL);
g_object_set(gis.imageenc, "quality", 65, NULL);
g_object_set(gis.sink, "location", "gps_%d.jpg", NULL);

set_tag(gis.metadata, &gis);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_print ("End of stream\n");
		g_main_loop_quit (loop);
	break;
	case GST_MESSAGE_WARNING:
	case GST_MESSAGE_ERROR: {
		gchar  *debug;
		GError *error;

		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);

		g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
		g_main_loop_quit (loop);
	}
	break;
}

return TRUE;
}

gint
main(gint argc, gchar **argv)
{
GstBus *bus;
int bus_watch_id;

g_type_init();
gst_init(&argc, &argv);

geoimagepipe();

bus = gst_pipeline_get_bus(GST_PIPELINE(gis.pipe));
bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
gst_object_unref(bus);

g_timeout_add(100, generate_geotag, NULL);

gst_element_set_state(gis.pipe, GST_STATE_PLAYING);

loop=g_main_loop_new(NULL, TRUE);
g_timeout_add(5000, g_main_loop_quit, loop);

g_main_loop_run(loop);

g_main_loop_unref(loop);
gst_object_unref(bus);
gst_element_set_state(gis.pipe, GST_STATE_NULL);
gst_object_unref(gis.pipe);

return 0;
}
