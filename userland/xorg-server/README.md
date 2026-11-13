# Xorg 21.1.25

Carried by oi-userland `components/x11/xorg-server`. Branch:
https://github.com/isoriano1968/oi-userland/tree/xorg-server-21

Xorg 1.19.7 does not work with Mesa 26 (glamor needs EGL_MESA_drm_image,
which Mesa dropped; OI's `29-glx-mesa.patch` breaks Mesa's megadriver
lookup). The component moves to 21.1.25, the version Oracle ships with
Solaris 11.4:

- Oracle's solaris-userland patch set for 21.1 and its `sun-src` (IA,
  dtlogin, osaudit, the mdb module), plus OI's own patches ported to 21.1
  (12-nvidia-autoconfig, 14-signal-handler, 18-hotkey, 22-record-regset,
  33 DRI2, 37-illumosfb-autoconfig).
- glamor on, DMX gone, input thread on.
- `39-no-console-fb.patch` (here): without VTs and without a visual_io
  `/dev/fb` (on amdgpu `/dev/fb` is the DRM node), X runs without a
  console device instead of failing or holding the DRM node as master.

Open before an oi-userland PR: pkglint (libdrm from the gfx-drm
publisher), the `/usr/X11/include/xorg` compatibility link, an obsolete
entry for `xdmx`, and every other `xf86-*` driver rebuilt for the ABI.
