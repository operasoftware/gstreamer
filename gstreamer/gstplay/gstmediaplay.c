#include <config.h>

#include <sys/stat.h>
#include <unistd.h>

#include <gnome.h>
#include "gstmediaplay.h"
#include "callbacks.h"

static void gst_media_play_class_init	    (GstMediaPlayClass *klass);
static void gst_media_play_init		    (GstMediaPlay *play);

static void gst_media_play_set_arg	    (GtkObject *object, GtkArg *arg, guint id);
static void gst_media_play_get_arg	    (GtkObject *object, GtkArg *arg, guint id);

static void gst_media_play_frame_displayed  (GstPlay *play, GstMediaPlay *mplay);
static void gst_media_play_state_changed    (GstPlay *play, GstPlayState state, GstMediaPlay *mplay);
static void gst_media_play_slider_changed   (GtkAdjustment   *adj, GstMediaPlay *mplay);

static void update_buttons		    (GstMediaPlay *mplay, GstPlayState state);
static void update_slider		    (GstMediaPlay *mplay, GtkAdjustment *adjustment, gfloat value);

/* signals and args */
enum {
	LAST_SIGNAL
};

enum {
	ARG_0,
};

static void
target_drag_data_received  (GtkWidget          *widget,
                            GdkDragContext     *context,
                            gint                x,
                            gint                y,
                            GtkSelectionData   *data,
                            guint               info,
                            guint               time,
			    GstMediaPlay       *play)
{
	g_print ("Got: %s\n", data->data);
	gdk_threads_leave ();
	gst_media_play_start_uri (play, g_strchomp (data->data));
	gdk_threads_enter ();

}

static GtkTargetEntry target_table[] = {
	{ "text/plain", 0, 0 }
};

static GtkObject *parent_class = NULL;
//static guint gst_media_play_signals[LAST_SIGNAL] = { 0 };

GtkType
gst_media_play_get_type (void)
{
	static GtkType play_type = 0;

	if (!play_type) {
		static const GtkTypeInfo play_info = {
			"GstMediaPlay",
			sizeof (GstMediaPlay),
			sizeof (GstMediaPlayClass),
			(GtkClassInitFunc) gst_media_play_class_init,
			(GtkObjectInitFunc) gst_media_play_init,
			NULL,
			NULL,
			(GtkClassInitFunc) NULL,
		};
		play_type = gtk_type_unique (gtk_object_get_type(), &play_info);
	}
	return play_type;
}

static void
gst_media_play_class_init (GstMediaPlayClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = (GtkObjectClass*) klass;

	object_class->set_arg = gst_media_play_set_arg;
	object_class->get_arg = gst_media_play_get_arg;
}

typedef struct {
	GstMediaPlay *play;
	GModule *symbols;
} connect_struct;

/* we need more control here so... */
static void
gst_media_play_connect_func (const gchar *handler_name,
                             GtkObject *object,
                             const gchar *signal_name,
                             const gchar *signal_data,
                             GtkObject *connect_object,
                             gboolean after,
                             gpointer user_data)
{
	GtkSignalFunc func;
	connect_struct *data = (connect_struct *) user_data;

	if (!g_module_symbol (data->symbols, handler_name, (gpointer *)&func))
		g_warning ("gsteditorproperty: could not find signal handler '%s'.", handler_name);
	else {
		if (after)
			gtk_signal_connect_after (object, signal_name, func, (gpointer) data->play);
		else
			gtk_signal_connect (object, signal_name, func, (gpointer) data->play);
	}
}


static void
gst_media_play_init (GstMediaPlay *mplay)
{
	GModule *symbols;
	connect_struct data;
	struct stat statbuf;


	/* load the interface */
	if (stat (DATADIR"gstmediaplay.glade", &statbuf) == 0) {
		mplay->xml = glade_xml_new (DATADIR"gstmediaplay.glade", "gstplay");
	}
	else {
		mplay->xml = glade_xml_new ("gstmediaplay.glade", "gstplay");
	}
	g_assert (mplay->xml != NULL);

	mplay->slider = glade_xml_get_widget (mplay->xml, "slider");
	g_assert (mplay->slider != NULL);
	{
		GtkArg arg;
		GtkRange *range;

		arg.name = "adjustment";
		gtk_object_getv (GTK_OBJECT (mplay->slider), 1, &arg);
		range = GTK_RANGE (GTK_VALUE_POINTER (arg));
		mplay->adjustment = gtk_range_get_adjustment (range);

		gtk_signal_connect (GTK_OBJECT (mplay->adjustment), "value_changed",
				    GTK_SIGNAL_FUNC (gst_media_play_slider_changed), mplay);
	}

	mplay->play_button = glade_xml_get_widget (mplay->xml, "toggle_play");
	g_assert (mplay->play_button != NULL);
	mplay->pause_button = glade_xml_get_widget (mplay->xml, "toggle_pause");
	g_assert (mplay->pause_button != NULL);
	mplay->stop_button = glade_xml_get_widget (mplay->xml, "toggle_stop");
	g_assert (mplay->stop_button != NULL);

	mplay->window = glade_xml_get_widget (mplay->xml, "gstplay");
	g_assert (mplay->window != NULL);

	gtk_drag_dest_set (mplay->window,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, 1,
			   GDK_ACTION_COPY);
	gtk_signal_connect (GTK_OBJECT (mplay->window), "drag_data_received",
			    GTK_SIGNAL_FUNC (target_drag_data_received),
			    mplay);

	mplay->play = gst_play_new();

	gtk_signal_connect (GTK_OBJECT (mplay->play), "frame_displayed",
			    GTK_SIGNAL_FUNC (gst_media_play_frame_displayed),
			    mplay);

	gtk_signal_connect (GTK_OBJECT (mplay->play), "audio_played",
			    GTK_SIGNAL_FUNC (gst_media_play_frame_displayed),
			    mplay);

	gtk_signal_connect (GTK_OBJECT (mplay->play), "playing_state_changed",
			    GTK_SIGNAL_FUNC (gst_media_play_state_changed),
			    mplay);

	gnome_dock_set_client_area (GNOME_DOCK (glade_xml_get_widget(mplay->xml, "dock1")),
				    GTK_WIDGET (mplay->play));

	gtk_widget_show (GTK_WIDGET (mplay->play));

	mplay->status = (GstStatusArea *) glade_xml_get_widget (mplay->xml, "status_area");
	gst_status_area_set_state (mplay->status, GST_STATUS_AREA_STATE_INIT);
	gst_status_area_set_playtime (mplay->status, "00:00 / 00:00");

	symbols = g_module_open (NULL, 0);

	data.play = mplay;
	data.symbols = symbols;

	glade_xml_signal_autoconnect_full (mplay->xml, gst_media_play_connect_func, &data);

	mplay->last_time = 0;
}

GstMediaPlay *
gst_media_play_new ()
{
	return GST_MEDIA_PLAY (gtk_type_new (GST_TYPE_MEDIA_PLAY));
}

static void
gst_media_play_update_status_area (GstMediaPlay *play,
				   gulong current_time,
				   gulong total_time)
{
	gchar time[14];

	sprintf (time, "%02lu:%02lu / %02lu:%02lu",
		 current_time / 60, current_time % 60,
		 total_time / 60, total_time % 60);

	gst_status_area_set_playtime (play->status, time);
}

void
gst_media_play_start_uri (GstMediaPlay *play,
		          const guchar *uri)
{
	GstPlayReturn ret;

	g_return_if_fail (play != NULL);
	g_return_if_fail (GST_IS_MEDIA_PLAY (play));

	if (uri != NULL) {
		ret = gst_play_set_uri (play->play, uri);

		if (!gst_play_media_can_seek (play->play)) {
			gtk_widget_set_sensitive (play->slider, FALSE);
		}

		gtk_window_set_title (GTK_WINDOW (play->window),
				      g_strconcat ( "Gstplay - ", uri, NULL));

		gst_play_play (play->play);
	}
}

typedef struct {
	GtkWidget *selection;
	GstMediaPlay *play;
} file_select;

static void
on_load_file_selected (GtkWidget *button,
		       file_select *data)
{
	GtkWidget *selector = data->selection;
	GstMediaPlay *play = data->play;

	gchar *file_name = gtk_file_selection_get_filename (GTK_FILE_SELECTION (selector));
	gdk_threads_leave();
	gst_media_play_start_uri (play, file_name);
	gdk_threads_enter();

	g_free (data);
}

void
on_open2_activate (GtkWidget *widget,
                   GstMediaPlay *play)
{
	GtkWidget *file_selector;
	file_select *file_data = g_new0 (file_select, 1);

	file_selector = gtk_file_selection_new ("Please select a file to load.");

	file_data->selection = file_selector;
	file_data->play = play;

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (on_load_file_selected),
			    file_data);

	/* Ensure that the dialog box is destroyed when the user clicks a button. */
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   (gpointer) file_selector);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   (gpointer) file_selector);

	/* Display that dialog */
	gtk_widget_show (file_selector);
}


static void
gst_media_play_set_arg (GtkObject *object,
		        GtkArg *arg,
		        guint id)
{
	GstMediaPlay *play;
	play = GST_MEDIA_PLAY (object);

	switch (id) {
	default:
		g_warning ("GstMediaPlay: unknown arg!");
		break;
	}
}

static void
gst_media_play_get_arg (GtkObject *object,
		        GtkArg *arg,
		        guint id)
{
	GstMediaPlay *play;

	play = GST_MEDIA_PLAY (object);

	switch (id) {
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gst_media_play_state_changed (GstPlay *play, 
			      GstPlayState state, 
			      GstMediaPlay *mplay)
{
	GstStatusAreaState area_state;

	g_return_if_fail (GST_IS_PLAY (play));
	g_return_if_fail (GST_IS_MEDIA_PLAY (mplay));


	gdk_threads_enter ();
	update_buttons (mplay, state);

	switch (state) {
	case GST_PLAY_STOPPED:
		area_state =  GST_STATUS_AREA_STATE_STOPPED;
		break;
	case GST_PLAY_PLAYING:
		area_state =  GST_STATUS_AREA_STATE_PLAYING;
		break;
	case GST_PLAY_PAUSED:
		area_state =  GST_STATUS_AREA_STATE_PAUSED;
		break;
	default:
		area_state =  GST_STATUS_AREA_STATE_INIT;
	}
	gst_status_area_set_state (mplay->status, area_state);
	gdk_threads_leave ();
}

void
on_gst_media_play_destroy (GtkWidget *widget, GstMediaPlay *mplay)
{
	gst_main_quit ();
}

void
on_exit_menu_activate (GtkWidget *widget, GstMediaPlay *mplay)
{
	gdk_threads_leave ();
	gst_play_stop (mplay->play);
	gdk_threads_enter ();
	gst_main_quit ();
}

gint
on_gst_media_play_delete_event (GtkWidget *widget,
		                GdkEvent *event,
			        GstMediaPlay *mplay)
{
	gdk_threads_leave ();
	gst_play_stop (mplay->play);
	gdk_threads_enter ();
	return FALSE;
}

void
on_extended1_activate (GtkCheckMenuItem *item, GstMediaPlay *mplay)
{
	gdk_threads_leave ();
	gst_status_area_show_extended (mplay->status, item->active);
	gdk_threads_enter ();
}

static void
gst_media_play_frame_displayed (GstPlay *play, GstMediaPlay *mplay)
{
	gulong current_time;
	gulong total_time;
	gulong size, current_offset;

	g_return_if_fail (GST_IS_PLAY (play));
	g_return_if_fail (GST_IS_MEDIA_PLAY (mplay));

	current_time   = gst_play_get_media_current_time (play);
	total_time     = gst_play_get_media_total_time (play);
	size           = gst_play_get_media_size (play);
	current_offset = gst_play_get_media_offset (play);

	//g_print ("%lu %lu %lu %lu\n", current_time, total_time, size, current_offset);

	if (current_time != mplay->last_time) {
		gdk_threads_enter ();
		gst_media_play_update_status_area (mplay, current_time, total_time);
		update_slider (mplay, mplay->adjustment, current_offset*100.0/size);
		mplay->last_time = current_time;
		gdk_threads_leave ();
	}
}

static void
gst_media_play_slider_changed (GtkAdjustment *adj, GstMediaPlay *mplay)
{
	gulong size;

	g_return_if_fail (GST_IS_MEDIA_PLAY (mplay));

	size   = gst_play_get_media_size (mplay->play);

	gst_play_media_seek (mplay->play, (int)(adj->value*size/100.0));
}

void
on_toggle_play_toggled (GtkToggleButton *togglebutton, GstMediaPlay    *play)
{
	gdk_threads_leave ();
	gst_play_play (play->play);
	gdk_threads_enter ();
	update_buttons (play, GST_PLAY_STATE(play->play));
}

void
on_toggle_pause_toggled (GtkToggleButton *togglebutton, GstMediaPlay    *play)
{
	gdk_threads_leave ();
	gst_play_pause (play->play);
	gdk_threads_enter ();
	update_buttons (play, GST_PLAY_STATE(play->play));
}

void
on_toggle_stop_toggled (GtkToggleButton *togglebutton, GstMediaPlay    *play)
{
	gdk_threads_leave ();
	gst_play_stop (play->play);
	gdk_threads_enter ();
	update_buttons (play, GST_PLAY_STATE(play->play));
}

static void
update_buttons (GstMediaPlay *mplay, GstPlayState state)
{
	gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->play_button),
					  GTK_SIGNAL_FUNC (on_toggle_play_toggled),
					  mplay);
	gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->pause_button),
					  GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
					  mplay);
	gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->stop_button),
					  GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
					  mplay);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->play_button), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->pause_button), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->stop_button), FALSE);

	if (state == GST_PLAY_PLAYING) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->play_button), TRUE);
	}
	else if (state == GST_PLAY_PAUSED) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->pause_button), TRUE);
	}
	else if (state == GST_PLAY_STOPPED) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->stop_button), TRUE);
	}
	
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->play_button),
					    GTK_SIGNAL_FUNC (on_toggle_play_toggled),
					    mplay);
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->pause_button),
					    GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
					    mplay);
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->stop_button),
					    GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
					    mplay);
}

static void
update_slider (GstMediaPlay *mplay,
	       GtkAdjustment *adjustment,
	       gfloat value)
{
	gtk_signal_handler_block_by_func (GTK_OBJECT (adjustment),
					  GTK_SIGNAL_FUNC (gst_media_play_slider_changed),
					  mplay);
	gtk_adjustment_set_value (adjustment, value);
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (adjustment),
					    GTK_SIGNAL_FUNC (gst_media_play_slider_changed),
					    mplay);
}
