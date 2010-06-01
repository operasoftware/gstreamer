export PATH := $(PREFIX)/bin:$(PATH)
export PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig/

.PHONY: libogg
libogg:
	cd "../libogg"; make clean; \
	./configure --prefix="$(PREFIX)" \
	&& make install

.PHONY: libvorbis
libvorbis: libogg
	cd "../libvorbis"; make clean; \
	./configure --prefix="$(PREFIX)" \
	&& make install

.PHONY: libtheora
libtheora: libogg
	cd "../libtheora"; make clean; \
	./configure --prefix="$(PREFIX)" \
		--disable-spec \
		--disable-encode \
		--disable-examples \
		"$(LIBTHEORA_CONFIGURE)" \
	&& make install

.PHONY: libvpx
libvpx:
	cd "../libvpx"; make clean; \
	./configure --prefix="$(PREFIX)" \
		--target="$(LIBVPX_TARGET)" \
		--enable-pic \
		--enable-runtime-cpu-detect \
		--disable-vp8-encoder \
		--disable-multithread \
		--disable-postproc \
		--disable-examples \
		--disable-install-docs \
		--disable-install-bins \
		--disable-install-srcs \
	&& make install

.PHONY: glib
glib:
	cd "../glib"; make clean; \
	AUTOGEN_SUBDIR_MODE=1 ./autogen.sh && \
	./configure --prefix="$(PREFIX)" \
		--disable-regex \
		--disable-gtk-doc \
	&& make install

.PHONY: gstreamer
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
		--disable-registry \
		--disable-net \
		--disable-valgrind \
		--disable-examples \
		--disable-tests \
		--disable-failing-tests \
		--disable-check \
	&& make install

.PHONY: gst-plugins-base
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

.PHONY: gst-plugins-good
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
		--disable-imagefreeze \
		--disable-law \
		--disable-level \
		--disable-monoscope \
		--disable-multifile \
		--disable-multipart \
		--disable-qtdemux \
		--disable-replaygain \
		--disable-rtp \
		--disable-rtpmanager \
		--disable-rtsp \
		--disable-shapewipe \
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
		--disable-oss4 \
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

.PHONY: gst-plugins-bad
gst-plugins-bad: gst-plugins-base libvpx
	cd "../gst-plugins-bad"; make clean; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--disable-nls \
		--disable-rpath \
		--disable-examples \
		--disable-adpcmdec \
		--disable-adpcmenc \
		--disable-aiff \
		--disable-asfmux \
		--disable-audioparsers \
		--disable-autoconvert \
		--disable-camerabin \
		--disable-legacyresample \
		--disable-bayer \
		--disable-cdxaparse \
		--disable-dataurisrc \
		--disable-dccp \
		--disable-debugutils \
		--disable-dtmf \
		--disable-dvdspu \
		--disable-festival \
		--disable-freeze \
		--disable-frei0r \
		--disable-h264parse \
		--disable-hdvparse \
		--disable-id3tag \
		--disable-invtelecine \
		--disable-jpegformat \
		--disable-librfb \
		--disable-liveadder \
		--disable-mpegdemux \
		--disable-mpegtsmux \
		--disable-mpegpsmux \
		--disable-mpeg4videoparse \
		--disable-mpegvideoparse \
		--disable-mve \
		--disable-mxf \
		--disable-nsf \
		--disable-nuvdemux \
		--disable-pcapparse \
		--disable-pnm \
		--disable-qtmux \
		--disable-rawparse \
		--disable-real \
		--disable-rtpmux \
		--disable-scaletempo \
		--disable-sdp \
		--disable-segmentclip \
		--disable-selector \
		--disable-siren \
		--disable-speed \
		--disable-subenc \
		--disable-stereo \
		--disable-tta \
		--disable-valve \
		--disable-videomeasure \
		--disable-videosignal \
		--disable-vmnc \
		--disable-directsound \
		--disable-directdraw \
		--disable-osx_video \
		--disable-quicktime \
		--disable-vcd \
		--disable-alsa \
		--disable-assrender \
		--disable-amrwb \
		--disable-apexsink \
		--disable-bz2 \
		--disable-cdaudio \
		--disable-celt \
		--disable-cog \
		--disable-dc1394 \
		--disable-directfb \
		--disable-dirac \
		--disable-dts \
		--disable-divx \
		--disable-resindvd \
		--disable-metadata \
		--disable-faac \
		--disable-faad \
		--disable-fbdev \
		--disable-flite \
		--disable-gsm \
		--disable-jack \
		--disable-jp2k \
		--disable-kate \
		--disable-ladspa \
		--disable-lv2 \
		--disable-libmms \
		--disable-modplug \
		--disable-mimic \
		--disable-mpeg2enc \
		--disable-mplex \
		--disable-musepack \
		--disable-musicbrainz \
		--disable-mythtv \
		--disable-nas \
		--disable-neon \
		--disable-ofa \
		--disable-rsvg \
		--disable-timidity \
		--disable-wildmidi \
		--disable-sdl \
		--disable-sdltest \
		--disable-sndfile \
		--disable-soundtouch \
		--disable-spc \
		--disable-gme \
		--disable-swfdec \
		--disable-theoradec \
		--disable-xvid \
		--disable-dvb \
		--disable-wininet \
		--disable-acm \
		--disable-vdpau \
		--disable-schro \
		--disable-zbar \
		CPPFLAGS="-I$(PREFIX)/include" \
		LDFLAGS="-L$(PREFIX)/lib" \
	&& make install

.PHONY: debug
debug:
	PREFIX="$(PWD)/Debug" \
	CFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	CXXFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	LDFLAGS="-arch i386" \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	LIBVPX_TARGET="x86-darwin8-gcc" \
	make gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Debug/lib dist/debug_i386 "@executable_path/../Resources/gstreamer"
	rm -fR dist/debug_i386/plugins
	mv dist/debug_i386/gstreamer-0.10 dist/debug_i386/plugins

.PHONY: release
release:
	PREFIX="$(PWD)/Release" \
	CFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	CXXFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	LDFLAGS="-arch i386" \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	LIBVPX_TARGET="x86-darwin8-gcc" \
	make gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Release/lib dist/release_i386 "@executable_path/../Resources/gstreamer"
	rm -fR dist/release_i386/plugins
	mv dist/release_i386/gstreamer-0.10 dist/release_i386/plugins
#	strip -u -r dist/release_i386/*.dylib dist/release_i386/plugins/*.so

.PHONY: debugppc
debugppc:
	PREFIX="$(PWD)/Debugppc" \
	CFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch ppc -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	CXXFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch ppc -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	LDFLAGS="-arch ppc" \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	LIBVPX_TARGET="ppc32-darwin8-gcc" \
	LIBTHEORA_CONFIGURE="--disable-asm" \
	make gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Debugppc/lib dist/debug_ppc "@executable_path/../Resources/gstreamer"
	rm -fR dist/debug_ppc/plugins
	mv dist/debug_ppc/gstreamer-0.10 dist/debug_ppc/plugins

.PHONY: releaseppc
releaseppc:
	PREFIX="$(PWD)/Releaseppc" \
	CFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch ppc -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	CXXFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch ppc -mmacosx-version-min=10.4 -DMAC_OS_X_VERSION_MIN_REQUIRED=1040 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" \
	LDFLAGS="-arch ppc" \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	LIBVPX_TARGET="ppc32-darwin8-gcc" \
	LIBTHEORA_CONFIGURE="--disable-asm" \
	make gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Releaseppc/lib dist/release_ppc "@executable_path/../Resources/gstreamer"
	rm -fR dist/release_ppc/plugins
	mv dist/release_ppc/gstreamer-0.10 dist/release_ppc/plugins
#	strip -u -r dist/release_ppc/*.dylib dist/release_ppc/plugins/*.so

.PHONY: all
all: debug release
