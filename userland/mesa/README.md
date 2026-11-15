# Mesa 26.2.6

Carried by oi-userland `components/x11/mesa` (patches are `-p0`, as the
component's others). Branch: https://github.com/isoriano1968/oi-userland/tree/mesa-26
(gallium drivers softpipe, llvmpipe, i915, r600, radeonsi; LLVM 21;
GBM, EGL; no Vulkan yet).

- `patch-src_gallium_auxiliary_util_u__screen.c.patch`: the dma-buf
  (PRIME) capability is read from the kernel only on Linux, the BSDs and
  Managarm; on illumos it stayed 0, GBM's DRI backend fell back to dumb
  buffers (XRGB scanout and ARGB cursors only) and Xorg modesetting with
  glamor failed in ScreenInit (ARGB8888 front buffer). Adds
  `DETECT_OS_SOLARIS`. Upstream: Mesa.

The branch also has the illumos build patches and a swrast MIT-SHM fix
(`dri_sw_winsys.c`).

Still to do: `os_same_file_description()` (`src/util/os_file.c`) for
illumos: two fds share a file description when `fstat()` gives the same
`st_rdev`, since the DRM core gives every open its own clone minor.
