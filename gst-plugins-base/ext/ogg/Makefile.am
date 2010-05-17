plugin_LTLIBRARIES = libgstogg.la

libgstogg_la_SOURCES = \
	gstogg.c \
	gstoggdemux.c \
	gstoggstream.c \
	vorbis_parse.c

noinst_HEADERS = \
	gstogg.h \
	gstoggdemux.h \
	gstoggmux.h \
	gstoggstream.h \
	dirac_parse.h \
	vorbis_parse.h

libgstogg_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(OGG_CFLAGS)
libgstogg_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(OGG_LIBS)
libgstogg_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstogg_la_LIBTOOLFLAGS = --tag=disable-static

