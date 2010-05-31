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
		--disable-examples \
		--disable-install-docs \
		--disable-install-bins \
		--disable-install-srcs
	touch $@

.PHONY: .libvpx
.libvpx: .libvpx.configure
	make -C ../libvpx -j install

.gst-plugins-good.configure:
	cd ../gst-plugins-good; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--with-package-origin="http://www.opera.com/" \
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
		--disable-wavparse \
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
		CFLAGS="$(CFLAGS) -DOPERA_MINIMAL_GST"
	touch $@

.PHONY: gst-plugins-good
gst-plugins-good: .gst-plugins-good.configure
	make -C ../gst-plugins-good -j install

.gst-plugins-bad.configure: .libvpx
	cd ../gst-plugins-bad; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PREFIX)" \
		--with-package-origin="http://www.opera.com/" \
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
		CFLAGS="$(CFLAGS) -DOPERA_MINIMAL_GST" \
		CPPFLAGS="-I$(PREFIX)/include" \
		LDFLAGS="-L$(PREFIX)/lib"
	touch $@

.PHONY: gst-plugins-bad
gst-plugins-bad: .gst-plugins-bad.configure
	make -C ../gst-plugins-bad -j install

plugins/libgstoperamatroska.so: gst-plugins-good
	mkdir -p plugins
	cp "$(PREFIX)/lib/gstreamer-0.10/libgstmatroska.so" $@
	strip -g $@

plugins/libgstoperavp8.so: gst-plugins-bad
	mkdir -p plugins
	cp "$(PREFIX)/lib/gstreamer-0.10/libgstvp8.so" $@
	strip -g $@

.PHONY: plugins
plugins: plugins/libgstoperamatroska.so plugins/libgstoperavp8.so

.PHONY: clean
clean:
	rm -rf tmp/ plugins/ .libvpx.configure .gst-plugins-good.configure .gst-plugins-bad.configure
	make -C ../libvpx clean || :
	make -C ../gst-plugins-good clean || :
	make -C ../gst-plugins-bad clean || :
