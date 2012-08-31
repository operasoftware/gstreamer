
== Submodules ==

To check out submodules, type:

$ git submodule update --init

== Building on Linux ==

To build gstvp8dec, headers for libvpx must be installed in a system search path.

$ cd unix
$ make

== Building on Mac ==

Build on 10.5 Prefered

pkg-config and yasm from MacPorts are needed.

To build gstvp8dec, headers for libvpx must be installed in a system search path.

$ cd macosx
$ make release (or debug)

If this fails, here are a couple of things to try:

- Replace glib/autogen.sh with autogen.sh from a current glib.
  The current (2012-08-31) glib/autogen.sh is a handcoded version
  that among other things do not support automake 1.12.  Current
  glib has a much shorter, standard version that works much better.

- At the top of macosx/Makefile there are two variables called
  MACSDK_32 and MACSDK_64.  Check that they point to working installs
  of the sdk.

- At the top of macosx/Makefile there are two variables called CC and
  CXX.  It should be fine to have them commented out, but if you need
  to specify the location of the compiler, these are the ones to use.

== Building on Windows ==

Headers for libvpx must either be installed in a system search path, or libvpx
must be checked out at ../libvpx (alongside gst-opera). To check out libvpx,
type:

$ git clone http://git.chromium.org/webm/libvpx.git

To build gstdirectsoundsink, DirectX 9.0 SDK October 2004 Update must
be unpacked (not installed) in dxsdk_oct2004. Download URL:
http://www.microsoft.com/downloads/details.aspx?FamilyId=B7BC31FA-2DF1-44FD-95A4-C2555446AED4&displaylang=en
