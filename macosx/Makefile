export PATH := $(PREFIX)/bin:$(PATH)
export PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig/

.PHONY: all debug release clean libogg libvorbis libtheora glib gstreamer gst-plugins-base gst-plugins-good

libogg:
	cd "../libogg"; make clean; \
	./configure --prefix="$(PREFIX)" \
	&& make install

libvorbis: libogg
	cd "../libvorbis"; make clean; \
	./configure --prefix="$(PREFIX)" \
	&& make install

libtheora: libogg
	cd "../libtheora"; make clean; \
	./configure --prefix="$(PREFIX)" \
		--disable-spec \
		--disable-encode \
		--disable-examples \
	&& make install

glib:
	cd "../glib"; make clean; \
	AUTOGEN_SUBDIR_MODE=1 ./autogen.sh && \
	./configure --prefix="$(PREFIX)" \
		--disable-regex \
	&& make install

gstreamer: glib
	cd "../gstreamer"; make clean; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--disable-docbook \
		--disable-nls \
		--disable-rpath \
		--disable-gst-debug \
		--disable-loadsave \
		--disable-parse \
		--disable-option-parsing \
		--disable-trace \
		--disable-alloc-trace \
		--disable-net \
		--disable-valgrind \
		--disable-examples \
		--disable-tests \
		--disable-failing-tests \
		--disable-check \
	&& make install

gst-plugins-base: libogg libvorbis libtheora glib
	cd "../gst-plugins-base"; make clean; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--with-audioresample-format=float \
		--disable-nls \
		--disable-rpath \
		--disable-adder \
		--disable-app \
		--disable-audiorate \
		--disable-audiotestsrc \
		--disable-gdp \
		--disable-subparse \
		--disable-tcp \
		--disable-videotestsrc \
		--disable-videorate \
		--disable-videoscale \
		--disable-volume \
		--disable-x \
		--disable-xvideo \
		--disable-xshm \
		--disable-gst_v4l \
		--disable-alsa \
		--disable-cdparanoia \
		--disable-gnome_vfs \
		--disable-gio \
		--disable-libvisual \
		--disable-oggtest \
		--disable-pango \
		--disable-vorbistest \
		--disable-freetypetest \
	&& make install

gst-plugins-good: gst-plugins-base
	cd "../gst-plugins-good"; make clean; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--disable-nls \
		--disable-rpath \
		--disable-examples \
		--disable-schemas-install \
		--disable-gconftool \
		--disable-videofilter \
		--disable-alpha \
		--disable-apetag \
		--disable-audiofx \
		--disable-auparse \
		--disable-autodetect \
		--disable-avi \
		--disable-cutter \
		--disable-debugutils \
		--disable-deinterlace \
		--disable-effectv \
		--disable-equalizer \
		--disable-flv \
		--disable-id3demux \
		--disable-icydemux \
		--disable-interleave \
		--disable-flx \
		--disable-goom \
		--disable-goom2k1 \
		--disable-law \
		--disable-level \
		--disable-matroska \
		--disable-monoscope \
		--disable-multifile \
		--disable-multipart \
		--disable-qtdemux \
		--disable-replaygain \
		--disable-rtp \
		--disable-rtpmanager \
		--disable-rtsp \
		--disable-smpte \
		--disable-spectrum \
		--disable-udp \
		--disable-videobox \
		--disable-videocrop \
		--disable-videomixer \
		--disable-wavenc \
		--disable-y4m \
		--disable-directsound \
		--disable-oss \
		--disable-sunaudio \
		--disable-osx_video \
		--disable-gst_v4l2 \
		--disable-x \
		--disable-xshm \
		--disable-xvideo \
		--disable-aalib \
		--disable-aalibtest \
		--disable-annodex \
		--disable-cairo \
		--disable-esd \
		--disable-esdtest \
		--disable-flac \
		--disable-gconf \
		--disable-gdk_pixbuf \
		--disable-hal \
		--disable-jpeg \
		--disable-libcaca \
		--disable-libdv \
		--disable-libpng \
		--disable-pulse \
		--disable-dv1394 \
		--disable-shout2 \
		--disable-shout2test \
		--disable-soup \
		--disable-speex \
		--disable-taglib \
		--disable-wavpack \
		--disable-zlib \
		--disable-bz2 \
	&& make install

debug:
	PREFIX="$(PWD)/Debug" \
	CFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE" \
	make gstreamer gst-plugins-base gst-plugins-good
	python makedist.py Debug/lib dist/debug "@executable_path/../Resources/gstreamer"
	mv dist/debug/gstreamer-0.10 dist/debug/plugins-dbg

release:
	PREFIX="$(PWD)/Release" \
	CFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE" \
	make gstreamer gst-plugins-base gst-plugins-good
	python makedist.py Release/lib dist/release "@executable_path/../Resources/gstreamer"
	mv dist/release/gstreamer-0.10 dist/release/plugins
	strip -u -r dist/release/*.dylib dist/release/plugins/*.so

all: debug release
