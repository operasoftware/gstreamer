
#define BUFFER 20
#define VIDEO_DECODER "mpeg_play"

#include <gnome.h>
#include <gst/gst.h>

extern gboolean _gst_plugin_spew;
gboolean idle_func(gpointer data);

void eof(GstElement *src) {
  g_print("have eos, quitting\n");
  exit(0);
}

void new_pad_created(GstElement *parse,GstPad *pad,GstElement *pipeline) {
  GstElement *parse_audio, *parse_video, *decode, *decode_video, *play, *videoscale, *show;
  GstElement *audio_queue, *video_queue;
  GstElement *audio_thread, *video_thread;

  GtkWidget *appwindow;

  g_print("***** a new pad %s was created\n", gst_pad_get_name(pad));
  gst_element_set_state(GST_ELEMENT(pipeline),GST_STATE_PAUSED);

  // connect to audio pad
  //if (0) {
  if (strncmp(gst_pad_get_name(pad), "audio_", 6) == 0) {
    gst_plugin_load("mp3parse");
    gst_plugin_load("mpg123");
    // construct internal pipeline elements
    parse_audio = gst_elementfactory_make("mp3parse","parse_audio");
    g_return_if_fail(parse_audio != NULL);
    decode = gst_elementfactory_make("mpg123","decode_audio");
    g_return_if_fail(decode != NULL);
    play = gst_elementfactory_make("audiosink","play_audio");
    g_return_if_fail(play != NULL);

    // create the thread and pack stuff into it
    audio_thread = gst_thread_new("audio_thread");
    g_return_if_fail(audio_thread != NULL);
    gst_bin_add(GST_BIN(audio_thread),GST_ELEMENT(parse_audio));
    gst_bin_add(GST_BIN(audio_thread),GST_ELEMENT(decode));
    gst_bin_add(GST_BIN(audio_thread),GST_ELEMENT(play));

    // set up pad connections
    gst_element_add_ghost_pad(GST_ELEMENT(audio_thread),
                              gst_element_get_pad(parse_audio,"sink"));
    gst_pad_connect(gst_element_get_pad(parse_audio,"src"),
                    gst_element_get_pad(decode,"sink"));
    gst_pad_connect(gst_element_get_pad(decode,"src"),
                    gst_element_get_pad(play,"sink"));

    // construct queue and connect everything in the main pipelie
    audio_queue = gst_elementfactory_make("queue","audio_queue");
    gtk_object_set(GTK_OBJECT(audio_queue),"max_level",BUFFER,NULL);
    gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(audio_queue));
    gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(audio_thread));
    gst_pad_connect(pad,
                    gst_element_get_pad(audio_queue,"sink"));
    gst_pad_connect(gst_element_get_pad(audio_queue,"src"),
                    gst_element_get_pad(audio_thread,"sink"));

    // set up thread state and kick things off
    gtk_object_set(GTK_OBJECT(audio_thread),"create_thread",TRUE,NULL);
    g_print("setting to READY state\n");
    gst_element_set_state(GST_ELEMENT(audio_thread),GST_STATE_READY);
  } else if (strncmp(gst_pad_get_name(pad), "video_", 6) == 0) {
  //} else if (0) {

    gst_plugin_load("mp1videoparse");
    gst_plugin_load(VIDEO_DECODER);
    gst_plugin_load("videoscale");
    gst_plugin_load("videosink");
    // construct internal pipeline elements
    parse_video = gst_elementfactory_make("mp1videoparse","parse_video");
    g_return_if_fail(parse_video != NULL);
    decode_video = gst_elementfactory_make(VIDEO_DECODER,"decode_video");
    g_return_if_fail(decode_video != NULL);
    videoscale = gst_elementfactory_make("videoscale","videoscale");
    g_return_if_fail(videoscale != NULL);
    gtk_object_set(GTK_OBJECT(videoscale),"width",704, "height", 576,NULL);


    show = gst_elementfactory_make("videosink","show");
    g_return_if_fail(show != NULL);
    //gtk_object_set(GTK_OBJECT(show),"width",640, "height", 480,NULL);

    appwindow = gnome_app_new("MPEG1 player","MPEG1 player");
    gnome_app_set_contents(GNOME_APP(appwindow),
      	        gst_util_get_widget_arg(GTK_OBJECT(show),"widget"));
		gtk_widget_show_all(appwindow);

    // create the thread and pack stuff into it
    video_thread = gst_thread_new("video_thread");
    g_return_if_fail(video_thread != NULL);
    gst_bin_add(GST_BIN(video_thread),GST_ELEMENT(parse_video));
    gst_bin_add(GST_BIN(video_thread),GST_ELEMENT(decode_video));
    gst_bin_add(GST_BIN(video_thread),GST_ELEMENT(videoscale));
    gst_bin_add(GST_BIN(video_thread),GST_ELEMENT(show));

    // set up pad connections
    gst_element_add_ghost_pad(GST_ELEMENT(video_thread),
                              gst_element_get_pad(parse_video,"sink"));
    gst_pad_connect(gst_element_get_pad(parse_video,"src"),
                    gst_element_get_pad(decode_video,"sink"));
    gst_pad_connect(gst_element_get_pad(decode_video,"src"),
    //                gst_element_get_pad(videoscale,"sink"));
    //gst_pad_connect(gst_element_get_pad(videoscale,"src"),
                    gst_element_get_pad(show,"sink"));

    // construct queue and connect everything in the main pipeline
    video_queue = gst_elementfactory_make("queue","video_queue");
    gtk_object_set(GTK_OBJECT(video_queue),"max_level",BUFFER,NULL);
    gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(video_queue));
    gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(video_thread));
    gst_pad_connect(pad,
                    gst_element_get_pad(video_queue,"sink"));
    gst_pad_connect(gst_element_get_pad(video_queue,"src"),
                    gst_element_get_pad(video_thread,"sink"));

    // set up thread state and kick things off
    gtk_object_set(GTK_OBJECT(video_thread),"create_thread",TRUE,NULL);
    g_print("setting to READY state\n");
    gst_element_set_state(GST_ELEMENT(video_thread),GST_STATE_READY);
  }
  g_print("\n");
  gst_element_set_state(GST_ELEMENT(pipeline),GST_STATE_PLAYING);
}

int main(int argc,char *argv[]) {
  GstElement *pipeline, *src, *parse;

  g_print("have %d args\n",argc);

  //_gst_plugin_spew = TRUE;
  g_thread_init(NULL);
  gst_init(&argc,&argv);
	gnome_init("MPEG1 Video player","0.0.1",argc,argv);
  gst_plugin_load("mpeg1parse");

  pipeline = gst_pipeline_new("pipeline");
  g_return_val_if_fail(pipeline != NULL, -1);

  //src = gst_elementfactory_make("asyncdisksrc","src");
  src = gst_elementfactory_make("disksrc","src");
  g_return_val_if_fail(src != NULL, -1);
  gtk_object_set(GTK_OBJECT(src),"location",argv[1],NULL);
  g_print("should be using file '%s'\n",argv[1]);

  parse = gst_elementfactory_make("mpeg1parse","parse");
  g_return_val_if_fail(parse != NULL, -1);

  gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(src));
  gst_bin_add(GST_BIN(pipeline),GST_ELEMENT(parse));

  gtk_signal_connect(GTK_OBJECT(parse),"new_pad",
                      GTK_SIGNAL_FUNC(new_pad_created),pipeline);

  gtk_signal_connect(GTK_OBJECT(src),"eos",
                      GTK_SIGNAL_FUNC(eof),NULL);

  gst_pad_connect(gst_element_get_pad(src,"src"),
                  gst_element_get_pad(parse,"sink"));

  g_print("setting to READY state\n");
  gst_element_set_state(GST_ELEMENT(pipeline),GST_STATE_READY);
  gst_element_set_state(GST_ELEMENT(pipeline),GST_STATE_PLAYING);

  g_print("about to enter loop\n");

  g_idle_add(idle_func,pipeline);

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}

gboolean idle_func(gpointer data) {
  gst_bin_iterate(GST_BIN(data));
  return TRUE;
}
