export PATH := $(PREFIX)/bin:$(PATH)
export PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig/
#export CC=/usr/bin/gcc
#export CXX=/usr/bin/g++
MACSDK_32 = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk
MACSDK_64 = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk
#VPX_HEADERS = -I/Users/eirik/opera/desktop-dev/platforms/media_backends/libvpx

.PHONY: libogg
libogg:
	cd "../libogg"; $(MAKE) clean; \
	./configure --prefix="$(PREFIX)" \
	&& $(MAKE) install

.PHONY: libvorbis
libvorbis: libogg
	cd "../libvorbis"; $(MAKE) clean; \
	./configure --prefix="$(PREFIX)" \
	&& $(MAKE) install

.PHONY: libtheora
libtheora: libogg
	cd "../libtheora"; $(MAKE) clean; \
	./configure --prefix="$(PREFIX)" \
		--disable-spec \
		--disable-encode \
		--disable-examples \
		"$(LIBTHEORA_CONFIGURE)" \
	&& $(MAKE) install

.PHONY: glib
glib:
	cd "../glib"; $(MAKE) clean; \
	AUTOGEN_SUBDIR_MODE=1 ACLOCAL_FLAGS="-I /opt/local/share/aclocal" ./autogen.sh && \
	echo '#include "../macosx/config.h.macosx"' >> config.h.in && \
	./configure --prefix="$(PREFIX)" \
		--host=$(HOST) \
		--enable-shared \
		--disable-regex \
		--disable-gtk-doc \
		"$(GLIB_CONFIGURE)" \
	&& $(MAKE) install

.PHONY: gstreamer
gstreamer: glib
	cd "../gstreamer"; $(MAKE) clean; \
	 ACLOCAL_FLAGS="-I /opt/local/share/aclocal" ./autogen.sh --noconfigure && \
	echo '#include "../macosx/config.h.macosx"' >> config.h.in && \
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
	&& $(MAKE) install

.PHONY: gst-plugins-base
gst-plugins-base: libogg libvorbis libtheora glib
	cd "../gst-plugins-base"; $(MAKE) clean; \
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
	&& $(MAKE) install

.PHONY: gst-plugins-good
gst-plugins-good: gst-plugins-base
	cd "../gst-plugins-good"; $(MAKE) clean; \
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
	&& $(MAKE) install

.PHONY: gst-plugins-bad
gst-plugins-bad: gst-plugins-base
	cd "../gst-plugins-bad"; $(MAKE) clean; \
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
		CPPFLAGS="-I$(PREFIX)/include $(VPX_HEADERS)" \
		LDFLAGS="-L$(PREFIX)/lib" \
		ERROR_CFLAGS="" \
	&& $(MAKE) ERROR_CFLAGS="" install

.PHONY: debugintel
debugintel:
	PREFIX="$(PWD)/Debug" \
	CFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.5 -DMAC_OS_X_VERSION_MIN_REQUIRED=1050 -isysroot $(MACSDK_32)" \
	CXXFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.5 -DMAC_OS_X_VERSION_MIN_REQUIRED=1050 -isysroot $(MACSDK_32)" \
	LDFLAGS="-arch i386" \
	HOST=i686-darwin \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	$(MAKE) gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Debug/lib dist/debug_i386 "@executable_path/../Resources/gstreamer"
	rm -fR dist/debug_i386/plugins
	mv dist/debug_i386/gstreamer-0.10 dist/debug_i386/plugins

.PHONY: releaseintel
releaseintel:
	PREFIX="$(PWD)/Release" \
	CFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.5 -DMAC_OS_X_VERSION_MIN_REQUIRED=1050 -isysroot $(MACSDK_32)" \
	CXXFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch i386 -mmacosx-version-min=10.5 -DMAC_OS_X_VERSION_MIN_REQUIRED=1050 -isysroot $(MACSDK_32)" \
	LDFLAGS="-arch i386" \
	HOST=i686-darwin \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	$(MAKE) gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Release/lib dist/release_i386 "@executable_path/../Resources/gstreamer"
	rm -fR dist/release_i386/plugins
	mv dist/release_i386/gstreamer-0.10 dist/release_i386/plugins
#	strip -u -r dist/release_i386/*.dylib dist/release_i386/plugins/*.so

.PHONY: debugintel64
debugintel64:
	PREFIX="$(PWD)/Debug" \
	CFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch x86_64 -mmacosx-version-min=10.6 -DMAC_OS_X_VERSION_MIN_REQUIRED=1060 -isysroot $(MACSDK_64)" \
	CXXFLAGS="-D_DEBUG -DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch x86_64 -mmacosx-version-min=10.6 -DMAC_OS_X_VERSION_MIN_REQUIRED=1060 -isysroot $(MACSDK_64)" \
	LDFLAGS="-arch x86_64" \
	HOST=x86_64-darwin \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	$(MAKE) gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Debug/lib dist/debug_x86_64 "@executable_path/../Resources/gstreamer"
	rm -fR dist/debug_x86_64/plugins
	mv dist/debug_x86_64/gstreamer-0.10 dist/debug_x86_64/plugins

.PHONY: releaseintel64
releaseintel64:
	PREFIX="$(PWD)/Release" \
	CFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch x86_64 -mmacosx-version-min=10.6 -DMAC_OS_X_VERSION_MIN_REQUIRED=1060 -isysroot $(MACSDK_64)" \
	CXXFLAGS="-DOPERA_MINIMAL_GST -DGST_DISABLE_REGISTRY_CACHE -arch x86_64 -mmacosx-version-min=10.6 -DMAC_OS_X_VERSION_MIN_REQUIRED=1060 -isysroot $(MACSDK_64)" \
	LDFLAGS="-arch x86_64" \
	HOST=x86_64-darwin \
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	$(MAKE) gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
	python makedist.py Release/lib dist/release_x86_64 "@executable_path/../Resources/gstreamer"
	rm -fR dist/release_x86_64/plugins
	mv dist/release_x86_64/gstreamer-0.10 dist/release_x86_64/plugins
#	strip -u -r dist/release_x86_646/*.dylib dist/release_x86_64/plugins/*.so

.PHONY: release
release: releaseintel releaseintel64
	cd dist; perl build_universal_binaries.pl
	
.PHONY: debug
debug: debugintel debugintel64
	cd dist; perl build_universal_binaries.pl	
