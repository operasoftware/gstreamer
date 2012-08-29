
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

== Building on Windows ==

Headers for libvpx must either be installed in a system search path, or libvpx
must be checked out at ../libvpx (alongside gst-opera). To check out libvpx,
type:

$ git clone http://git.chromium.org/webm/libvpx.git

To build gstdirectsoundsink, DirectX 9.0 SDK October 2004 Update must
be unpacked (not installed) in dxsdk_oct2004. Download URL:
http://www.microsoft.com/downloads/details.aspx?FamilyId=B7BC31FA-2DF1-44FD-95A4-C2555446AED4&displaylang=en
