# Userland pieces for amdgpu on illumos

The kernel side (LinuxKPI, the DRM core, `drivers/amdgpu`) is in this
repository. Running X and GNOME on it also needs a few changes outside
it, collected here so they are in one place. Each directory names the
upstream version, the OpenIndiana component that carries the change, and
where the change should go upstream. The full component changes live on
branches of forks (links below); the patches here are copies.

| Piece | Version | Carried by | Branch | Upstream target |
|---|---|---|---|---|
| libdrm | 2.4.134 | OpenIndiana gfx-drm (`usr/src/common/libdrm/patches`) | [isoriano1968/gfx-drm `libdrm-amdgpu`](https://github.com/isoriano1968/gfx-drm/tree/libdrm-amdgpu) | gfx-drm, then libdrm |
| Mesa | 26.2.6 | oi-userland `components/x11/mesa` | [isoriano1968/oi-userland `mesa-26`](https://github.com/isoriano1968/oi-userland/tree/mesa-26) | oi-userland, then Mesa |
| Xorg | 21.1.25 | oi-userland `components/x11/xorg-server` | [isoriano1968/oi-userland `xorg-server-21`](https://github.com/isoriano1968/oi-userland/tree/xorg-server-21) | oi-userland |
| xf86-input-keyboard, -mouse | 1.9.0, 1.9.5 | oi-userland `components/x11/xf86-input-*` | same branch | oi-userland |

Licence: each patch keeps the licence of the code it changes (libdrm, Mesa and
the X server are MIT/X11-licensed), not this repository's MPL-2.0.
