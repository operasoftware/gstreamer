#include <stdio.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <sys/soundcard.h>

static gint quit_live(GtkWidget *window, GdkEventAny *e, gpointer data) {
  gtk_main_quit();
  return FALSE;
}

//static void volume_changed(GtkAdjustment *adj,GstElement *element) {
  //fprintf(stderr,"setting volume to %f\n",adj->value);
//  gtk_object_set(GTK_OBJECT(element),"volume",adj->value,NULL);
//}

static void dynparm_value_changed(GtkAdjustment *adj,GstDparam *dparam) {
  GstDparamPoint *point;
  GValue *value;
  g_assert(dparam != NULL);
  g_assert(GST_IS_DPARAM (dparam));

  point = GST_DPARAM_GET_POINT(dparam, 0LL);
  
  GST_DPARAM_LOCK(dparam);
  value = point->values[0];
  g_print("setting value from %f to %f\n", g_value_get_float(value), adj->value);  
  g_value_set_float(value, adj->value);
  GST_DPARAM_READY_FOR_UPDATE(dparam) = TRUE;
  GST_DPARAM_UNLOCK(dparam);
}


int main(int argc,char *argv[]) {
  GtkWidget *window;
  GtkWidget *hbox;
  GtkAdjustment *volume_adj;
  GtkAdjustment *freq_adj;
  GtkWidget *volume_slider;
  GtkWidget *freq_slider;

  GstElement *thread, *sinesrc, *osssink;
  GstDparamManager *dpman;
  GstDparam *volume, *freq;

  gtk_init(&argc,&argv);
  gst_init(&argc,&argv);

  /***** set up the GUI *****/
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(window,"delete_event",GTK_SIGNAL_FUNC(quit_live),NULL);
  hbox = gtk_hbox_new(TRUE,0);
  gtk_container_add(GTK_CONTAINER(window),hbox);

  volume_adj = (GtkAdjustment*)gtk_adjustment_new(1.0, 0.0, 1.0, 0.01, 0.01, 0.01);
  volume_slider = gtk_vscale_new(volume_adj);
  gtk_box_pack_start(GTK_BOX(hbox),volume_slider,TRUE,TRUE,0);

  freq_adj = (GtkAdjustment*)gtk_adjustment_new(440.0, 0.0, 2000.0, 1.0, 5.0, 5.0);
  freq_slider = gtk_vscale_new(freq_adj);
  gtk_box_pack_start(GTK_BOX(hbox),freq_slider,TRUE,TRUE,0);

  /***** construct the pipeline *****/
  
  g_print("creating elements\n");
  thread = gst_thread_new("live-example");
  sinesrc = gst_elementfactory_make("sinesrc","sine-source");
  osssink = gst_elementfactory_make("osssink","sound-sink");
  gst_bin_add(GST_BIN(thread),sinesrc);
  gst_bin_add(GST_BIN(thread),osssink);
  gst_element_connect(sinesrc,"src",osssink,"sink");
  gtk_object_set(GTK_OBJECT(sinesrc),"GstSineSrc::buffersize",64,NULL);
 
  dpman = GST_ELEMENT_DPARAM_MANAGER(sinesrc);
  volume = gst_dparam_new();
  freq = gst_dparam_new();

  g_assert(gst_dpman_attach_dparam (dpman, "volume", volume));
  g_assert(gst_dpman_attach_dparam (dpman, "freq", freq));
  
  gst_dpman_set_mode(dpman, "synchronous");

  /***** set up the handlers and such *****/
  //gtk_signal_connect(volume_adj,"value-changed",GTK_SIGNAL_FUNC(volume_changed),sinesrc);
  g_signal_connect(volume_adj,"value-changed",
					 GTK_SIGNAL_FUNC(dynparm_value_changed),
					 volume);

  g_signal_connect(freq_adj,"value-changed",
					 GTK_SIGNAL_FUNC(dynparm_value_changed),
					 freq);
  gtk_adjustment_value_changed(volume_adj);
  gtk_adjustment_value_changed(freq_adj);
  
  g_print("starting pipeline\n");
 
  /***** start everything up *****/
  gst_element_set_state(thread,GST_STATE_PLAYING);


  gtk_widget_show_all(window);
  gtk_main();
  
  return 0;
}
