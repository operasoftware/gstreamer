TOP ?= $(shell pwd)
PREFIX = $(TOP)/tmp

.PHONY: all
all: plugins

.libvpx.configure:
	cd ../libvpx; \
	./configure --prefix="$(PREFIX)" \
		--enable-pic \
		--enable-runtime-cpu-detect \
		--disable-vp8-encoder \
		--disable-multithread \
		--disable-postproc \
		--disable-examples \
		--disable-install-docs \
		--disable-install-bins \
		--disable-install-srcs
	touch $@

.PHONY: .libvpx
.libvpx: .libvpx.configure
	$(MAKE) -C ../libvpx -j install

GST_CFLAGS=$(shell pkg-config --cflags gstreamer-video-0.10) -fPIC
GST_LIBS=$(shell pkg-config --libs gstreamer-video-0.10) -lm
GST_OPERA_CFLAGS=-DOPERA_MINIMAL_GST -DVERSION=\"0.10\" -DPACKAGE=\"gst-opera\" -DGST_PACKAGE_NAME=\"GStreamer\ Opera\ Plug-ins\" -DGST_PACKAGE_ORIGIN=\"http://sourcecode.opera.com/gstreamer/\"
VPX_CFLAGS=-I"$(PREFIX)/include"
VPX_LDFLAGS=-L"$(PREFIX)/lib"
VPX_LIBS=-lvpx

src/gst/matroska:
	mkdir -p $@
	cp ../gst-plugins-good/gst/matroska/*.c ../gst-plugins-good/gst/matroska/*.h $@

src/ext/vp8:
	mkdir -p $@
	cp ../gst-plugins-bad/ext/vp8/*.c ../gst-plugins-bad/ext/vp8/*.h $@

src/gst-libs/gst/video:
	mkdir -p $@
	cp ../gst-plugins-bad/ext/vp8/gst/video/*.c ../gst-plugins-bad/ext/vp8/gst/video/*.h $@

GST_MATROSKA_SOURCES = \
	../gst-plugins-good/gst/matroska/ebml-read.c \
	../gst-plugins-good/gst/matroska/matroska-ids.c \
	../gst-plugins-good/gst/matroska/matroska-demux.c

plugins/libgstoperamatroska.so: $(GST_MATROSKA_SOURCES)
	mkdir -p plugins
	gcc -shared -Wl,--no-undefined -o $@ $^ $(GST_CFLAGS) $(GST_OPERA_CFLAGS) $(GST_LIBS)

GST_VP8_SOURCES = \
	../gst-plugins-bad/ext/vp8/plugin.c \
	../gst-plugins-bad/ext/vp8/gstvp8dec.c \
	../gst-plugins-bad/ext/vp8/gstvp8utils.c

GST_VP8_CFLAGS = -DHAVE_VP8_DECODER

GST_BASE_VIDEO_SOURCES = \
	../gst-plugins-bad/gst-libs/gst/video/gstbasevideocodec.c \
	../gst-plugins-bad/gst-libs/gst/video/gstbasevideodecoder.c \
	../gst-plugins-bad/gst-libs/gst/video/gstbasevideoutils.c

GST_BASE_VIDEO_CFLAGS = -DGST_USE_UNSTABLE_API -I../gst-plugins-bad/gst-libs

plugins/libgstoperavp8.so: $(GST_VP8_SOURCES) $(GST_BASE_VIDEO_SOURCES) | .libvpx
	mkdir -p plugins
	gcc -shared -Wl,--no-undefined -o $@ $^ $(GST_VP8_CFLAGS) $(GST_BASE_VIDEO_CFLAGS)  $(GST_CFLAGS) $(GST_OPERA_CFLAGS) $(GST_LIBS) $(VPX_CFLAGS) $(VPX_LDFLAGS) $(VPX_LIBS)

.PHONY: plugins
plugins: plugins/libgstoperamatroska.so plugins/libgstoperavp8.so

.PHONY: clean
clean:
	rm -rf "$(PREFIX)" plugins/ .libvpx.configure
	$(MAKE) -C ../libvpx clean || :
