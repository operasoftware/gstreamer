TARGET ?= "generic-gnu"

KERNEL = $(shell uname -s)
MACHINE = $(shell uname -m)
ifeq ($(KERNEL),Linux)
  ifeq ($(MACHINE),i686)
    TARGET = "x86-linux-gcc"
  else
    ifeq ($(MACHINE),x86_64)
      TARGET = "x86_64-linux-gcc"
    endif
  endif
endif

.PHONY: all
all: plugins

.libvpx.configure:
	cd ../libvpx; \
	./configure --target="$(TARGET)" \
		--enable-pic \
		--enable-runtime-cpu-detect \
		--disable-vp8-encoder
	touch $@

.PHONY: .libvpx
.libvpx: .libvpx.configure
	make -C ../libvpx -j libvpx.a

.gst-plugins-good.configure:
	cd ../gst-plugins-good; \
	./autogen.sh --noconfigure && \
	./configure --prefix="$(PWD)/gst-plugins-good" \
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
gst-plugins-good: .gst-plugins-good.configure .libvpx
	make -C ../gst-plugins-good -j install

plugins: gst-plugins-good
	mkdir -p plugins/
	cp gst-plugins-good/lib/gstreamer-0.10/libgstvp8.so plugins/libgstoperavp8.so
	cp gst-plugins-good/lib/gstreamer-0.10/libgstmatroska.so plugins/libgstoperamatroska.so
	strip -g plugins/*.so

.PHONY: clean
clean:
	rm -rf gst-plugins-good/ plugins/ .libvpx.configure .gst-plugins-good.configure
	make -C ../libvpx clean
	make -C ../gst-plugins-good clean
