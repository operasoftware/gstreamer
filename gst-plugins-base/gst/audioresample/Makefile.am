plugin_LTLIBRARIES = libgstaudioresample.la

if AUDIORESAMPLE_NEEDS_LIBOIL
COND_LIBOIL_CFLAGS=$(LIBOIL_CFLAGS)
COND_LIBOIL_LIBS=$(LIBOIL_LIBS)
else
COND_LIBOIL_CFLAGS=
COND_LIBOIL_LIBS=
endif

libgstaudioresample_la_SOURCES = \
	gstaudioresample.c \
	speex_resampler_int.c \
	speex_resampler_float.c \
	speex_resampler_double.c

libgstaudioresample_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(COND_LIBOIL_CFLAGS)

libgstaudioresample_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(COND_LIBOIL_LIBS) \
	$(LIBM)

libgstaudioresample_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudioresample_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	arch.h \
	fixed_arm4.h \
	fixed_arm5e.h \
	fixed_bfin.h \
	fixed_debug.h \
	fixed_generic.h \
	gstaudioresample.h \
	resample.c \
	resample_sse.h \
	speex_resampler.h \
	speex_resampler_wrapper.h

