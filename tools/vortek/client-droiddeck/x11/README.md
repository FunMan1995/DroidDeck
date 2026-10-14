# Xlib headers for the client cross-build

`X11/Xlib.h`, `X.h`, `Xfuncproto.h` and `Xosdefs.h`, copied unmodified from Debian 13
`libx11-dev` 2:1.8.12-1 (upstream libX11 1.8.12) and `x11proto-dev` 2024.1-1 (upstream xorgproto
2024.1; `Xfuncproto.h` is the configured form of `Xfuncproto.h.in`). They are needed only at build
time, because `vulkan_xlib.h` and `src/dd_present.c` use the Xlib types and the cross image
`droiddeck-local-cross:24.04-v3` has no X11 headers. libX11 itself is loaded at runtime from the
guest (dlopen), so nothing from X.Org is linked or shipped. License: MIT/X11 (the notices are at
the top of each file). Sources: https://gitlab.freedesktop.org/xorg/lib/libx11 and
https://gitlab.freedesktop.org/xorg/proto/xorgproto
