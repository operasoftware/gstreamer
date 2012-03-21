== Linux ==

$ cd unix
$ make

== Mac ==

Build on 10.5 Prefered

pkg-config and yasm from MacPorts are needed.

$ cd macosx
$ make release (or debug)

== Windows ==

Yasm must be installed to compile libvpx. Get Yasm from http://yasm.tortall.net/
and follow instructions for Visual Studio 20XX on the Download page.

To build gstdirectsoundsink, DirectX 9.0 SDK October 2004 Update must
be unpacked (not installed) in dxsdk_oct2004. Download URL:
http://www.microsoft.com/downloads/details.aspx?FamilyId=B7BC31FA-2DF1-44FD-95A4-C2555446AED4&displaylang=en
