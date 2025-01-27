#include "ledwand.h"
#include <stdio.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

static GMainLoop *loop;
static Ledwand ledwand;

static void on_new_buffer (GstElement* object, gpointer user_data){
    GstAppSink *app_sink = (GstAppSink*) object;
    GstBuffer *buffer = gst_app_sink_pull_buffer(app_sink);
    ledwand_draw_image(&ledwand, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
}

static gboolean playbin2_bus_callback (GstBus *bus,GstMessage *message, gpointer data) {
  //g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }

    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_main_loop_quit (loop);
      break;

    default:
      /* unhandled message */
      break;
  }

  return TRUE;
}

int main(int argc, char **argv)
{
    if(ledwand_init(&ledwand)){
        perror("FEHLER beim init\n");
        return -1;
    }
    char c;
    char movie[PATH_MAX], absmovie[PATH_MAX];
    char *text = "playing";
    int len = strnlen(text, 255);
    int mute = 1;

    gst_init(&argc, &argv);

    while (-1 != (c = getopt(argc, argv, "a")))
	switch (c) {
	    case 'a':
		mute = 0;
		break;
	    default:
		fprintf(stderr, "syntax error! usage: %s [-a] filename_or_URL\n", argv[0]);
		return 1;
	}

    if (optind >= argc) {
	fprintf(stderr, "must specify filename or URL parameter!\n");
	return 1;
    }
    if (strstr(argv[optind], "://"))
	snprintf(movie, sizeof(movie), "%s", argv[optind]);
    else {
	if (!realpath(argv[optind], absmovie)) {
	    fprintf(stderr, "file '%s' not found!\n", argv[optind]);
	    return 1;
	} else
	    snprintf(movie, sizeof(movie), "file://%s", absmovie);
    }
    printf("playing '%s'...\n", movie);

    ledwand_clear(&ledwand);
    ledwand_send(&ledwand, ASCII, 0, 0, len, 1, (uint8_t*)text, len);

    GstElement *playbin2 = gst_element_factory_make("playbin2", "playbin2");
    GstElement *ledsink = gst_element_factory_make("appsink", "ledsink");
    GstCaps *caps = gst_caps_from_string("video/x-raw-gray, width=448, height=240");
    if(!playbin2 || !ledsink || !caps){
        perror("Konnte Elemente nicht erzeugen\n");
        return -1;
    }
    GstBus *omnibus = gst_pipeline_get_bus(GST_PIPELINE(playbin2));
    if(!omnibus){
        perror("Konnte Bus nicht erzeugen\n");
        return -1;
    }

    g_object_set(G_OBJECT(ledsink), "caps", caps, NULL);
    gst_app_sink_set_emit_signals ((GstAppSink*) ledsink, TRUE);
    g_object_set(playbin2, "uri", movie, NULL);
    g_object_set(playbin2, "video-sink", ledsink, NULL);
    if (mute)
	g_object_set(playbin2, "flags", 0x01, NULL);
    gst_bus_add_watch (omnibus, playbin2_bus_callback, NULL);
    g_signal_connect (ledsink, "new-buffer",  G_CALLBACK (on_new_buffer), NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_print("playing!\n");
    gst_element_set_state (playbin2, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    // cleanup
    close(ledwand.s_sock);
    gst_element_set_state(playbin2, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(ledsink));
    gst_object_unref(GST_OBJECT(omnibus));
    gst_object_unref(GST_OBJECT(playbin2));
    gst_object_unref(GST_OBJECT(loop));
    return 0;
}
