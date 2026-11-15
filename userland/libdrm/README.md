# libdrm 2.4.134

Carried by OpenIndiana's gfx-drm in `usr/src/common/libdrm/patches/`
(every `*.patch` there is applied with `patch -p1`). Branch:
https://github.com/isoriano1968/gfx-drm/tree/libdrm-amdgpu

- `amdgpu-bo-import-size.patch`: `amdgpu_bo_import()` takes an imported
  dma-buf's size from `lseek(fd, 0, SEEK_END)`. The DRM core's dma-buf
  file descriptors (clone minors) return 0 there, so every imported BO had
  size 0 and its GPU mapping failed (`AMDGPU_GEM_VA` map_size 0 ->
  EINVAL); Xorg's glamor answered every DRI3 PixmapFromBuffers with
  BadAlloc. Falls back to `AMDGPU_GEM_OP_GET_GEM_CREATE_INFO`. Stays as a
  fallback once the core reports the size (planned). Upstream: libdrm.
- `libsync-h.patch`: `libsync.h` needs `<sys/ioccom.h>` for `_IOWR()` on
  illumos. Upstream: libdrm.

Still to do in gfx-drm: install `data/amdgpu.ids` at
`/usr/share/libdrm/amdgpu.ids` (marketing names).
