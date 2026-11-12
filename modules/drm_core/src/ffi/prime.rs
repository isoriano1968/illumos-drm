// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! `dma_buf.h`: the C handle of a dma-buf. In the core a dma-buf is its GEM
//! object (exported through an anonymous file); a `struct dma_buf` here is
//! a counted handle that holds a reference to that object.

use alloc::boxed::Box;
use core::ffi::c_void;
use core::ptr::{self, NonNull};
use core::sync::atomic::{AtomicU32, Ordering};

use ddi::{DmaAttr, Errno, sys};
use drm_core::gem::{GemObject, GemRef, KernelBinding};
use drm_facade_sys as f;

use crate::devcore::{DevCore, neg};
use crate::fobj::{FObj, gem_ref};

/// The structure behind a `struct dma_buf *`.
#[repr(C)]
pub(crate) struct DmaBuf {
    buf: f::dma_buf,
    obj: GemRef<FObj>,
    refs: AtomicU32,
}

fn err_ptr<T>(e: Errno) -> *mut T {
    neg(e) as isize as *mut T
}

impl DmaBuf {
    /// A new handle for `obj` with one reference.
    pub(crate) fn make(obj: GemRef<FObj>) -> *mut f::dma_buf {
        let name = obj.object().core().info().name.as_ptr();
        // SAFETY: plain data; zero is valid.
        let mut buf: f::dma_buf = unsafe { core::mem::zeroed() };
        buf.size = obj.object().size();
        buf.priv_ = obj.object().base().cast();
        // SAFETY: the object's C structure is live (obj holds it).
        buf.resv = unsafe { (*obj.object().base()).resv };
        buf.exp_name = name;
        let b = Box::into_raw(Box::new(DmaBuf { buf, obj, refs: AtomicU32::new(1) }));
        // SAFETY: just made.
        unsafe {
            let l = ptr::addr_of_mut!((*b).buf.attachments);
            (*l).next = l;
            (*l).prev = l;
            (*b).buf.__core[0] = b as u64;
            (*b).buf.__core[1] = (b as u64) ^ crate::MAGIC_DMABUF;
        }
        b.cast()
    }
}

/// The handle behind a `struct dma_buf *` the facade made.
///
/// # Safety
/// `buf` is NULL or a handle with a reference the caller holds.
unsafe fn handle<'a>(buf: *const f::dma_buf) -> Option<&'a DmaBuf> {
    if buf.is_null() {
        return None;
    }
    // SAFETY: the caller's contract.
    let (p, check) = unsafe { ((*buf).__core[0], (*buf).__core[1]) };
    if p != buf as u64 || p ^ crate::MAGIC_DMABUF != check {
        return None;
    }
    // SAFETY: the structure is the first field of a live DmaBuf.
    Some(unsafe { &*(buf as *const DmaBuf) })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_gem_prime_export(obj: *mut f::drm_gem_object, _flags: i32) -> *mut f::dma_buf {
    // SAFETY: the caller holds the object.
    let Some(r) = (unsafe { gem_ref(obj) }) else { return err_ptr(Errno::EINVAL) };
    // An import exports the buffer it imported, as Linux re-exports an
    // imported object's dma-buf: importing that again anywhere finds the
    // original object (and its existing handle).
    let r = match r.object().exporter() {
        Some(e) => {
            let e = e.clone();
            // SAFETY: drm_gem_prime_export is documented for thread context.
            r.put_in(drm_core::defer::Ctx::Sleep(&unsafe { super::sleepable() }));
            e
        }
        None => r,
    };
    DmaBuf::make(r)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_gem_prime_import(
    dev: *mut f::drm_device,
    dma_buf: *mut f::dma_buf,
) -> *mut f::drm_gem_object {
    // SAFETY: documented for thread context.
    let ctx = unsafe { super::sleepable() };
    // SAFETY: the caller's device and dma-buf.
    let (Some(core), Some(h)) = (unsafe { DevCore::of(dev) }, unsafe { handle(dma_buf) }) else {
        return err_ptr(Errno::EINVAL);
    };
    let Some(gem) = core.device().and_then(|d| d.gem().cloned()) else { return err_ptr(Errno::ENODEV) };
    let r = if drm_core::karc::KArc::ptr_eq(h.obj.core(), &gem) {
        // This device's own buffer: the object itself.
        h.obj.clone()
    } else {
        match crate::fobj::import_object(core, &h.obj, &ctx) {
            Ok(r) => r,
            Err(e) => return err_ptr(e),
        }
    };
    let base = r.object().base();
    // The caller's reference.
    let _ = r.into_raw();
    base
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn dma_buf_get(fd: i32) -> *mut f::dma_buf {
    // SAFETY: documented for ioctl handlers, thread context.
    let ctx = unsafe { super::sleepable() };
    let r = crate::ctxt::with_current(|frame, arg| {
        // SAFETY: the ioctl's device, registered while it runs.
        let core = unsafe { frame.core.as_ref() };
        let d = core.device().ok_or(Errno::ENODEV)?;
        d.sync().dmabuf_object(fd, arg, &ctx)
    });
    match r {
        Some(Ok(obj)) => DmaBuf::make(obj),
        Some(Err(e)) => err_ptr(e),
        None => err_ptr(Errno::EINVAL),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn dma_buf_fd(dmabuf: *mut f::dma_buf, flags: i32) -> i32 {
    // SAFETY: documented for ioctl handlers, thread context.
    let ctx = unsafe { super::sleepable() };
    // SAFETY: the caller holds the handle.
    let Some(h) = (unsafe { handle(dmabuf) }) else { return neg(Errno::EINVAL) };
    let flags = flags as u32;
    let write = flags & drm_core::prime::DRM_RDWR != 0;
    let cloexec = flags & drm_core::prime::DRM_CLOEXEC != 0;
    let r = crate::ctxt::with_current(|_frame, arg| {
        // The dma-buf file belongs to the exporting object's device.
        let core = h.obj.object().core();
        let d = core.device().ok_or(Errno::ENODEV)?;
        let mut out = -1;
        d.sync().export_dmabuf(
            h.obj.clone(),
            write,
            cloexec,
            arg,
            |fd| {
                out = fd;
                Ok(())
            },
            &ctx,
        )?;
        Ok(out)
    });
    match r {
        Some(Ok(fd)) => {
            // As on Linux, the descriptor takes over the caller's
            // reference (its file holds its own object reference).
            // SAFETY: the caller's reference, given up on success.
            unsafe { dma_buf_put(dmabuf) };
            fd
        }
        Some(Err(e)) => neg(e),
        None => neg(Errno::EINVAL),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn get_dma_buf(dmabuf: *mut f::dma_buf) {
    // SAFETY: the caller holds the handle.
    if let Some(h) = unsafe { handle(dmabuf) } {
        h.refs.fetch_add(1, Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn dma_buf_put(dmabuf: *mut f::dma_buf) {
    // SAFETY: the caller gives up its reference.
    let Some(h) = (unsafe { handle(dmabuf) }) else { return };
    if h.refs.fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }
    // SAFETY: the last reference: the handle and its object reference go
    // (in the caller's context: the object defers its release if needed).
    let b = unsafe {
        (*dmabuf).__core[0] = 0;
        (*dmabuf).__core[1] = 0;
        Box::from_raw(dmabuf as *mut DmaBuf)
    };
    let DmaBuf { obj, .. } = *b;
    // SAFETY: dma_buf.h: an interrupt handler or a thread that may block.
    unsafe { super::with_ctx(|c| obj.put_in(c)) };
}

// --- another device's dma-buf mapped for this device (PRIME imports) ---

/// The structure behind a `struct drm_illumos_prime_map *`.
struct PrimeMap {
    magic: u64,
    /// Dropped (unbound) before the exporter may unmap the memory.
    binding: KernelBinding,
    exporter: GemRef<FObj>,
    /// The exporter's unpin, if its driver pinned the memory for this map.
    unpin: Option<unsafe extern "C" fn(*mut f::drm_gem_object)>,
}

const MAGIC_PRIME_MAP: u64 = 0x6472_6d5f_706d_6170; // "drm_pmap"

/// # Safety
/// `p` is NULL or a map from `drm_illumos_prime_map` not yet unmapped.
unsafe fn prime_map<'a>(p: *const f::drm_illumos_prime_map) -> Option<&'a PrimeMap> {
    if p.is_null() {
        return None;
    }
    // SAFETY: the caller's contract.
    let m = unsafe { &*(p as *const PrimeMap) };
    (m.magic == MAGIC_PRIME_MAP ^ (p as u64)).then_some(m)
}

/// Maps `dma_buf`'s memory for DMA by `dev` (`dma_buf.h`): the exporter's
/// core memory, or the pages its driver pins with its
/// `drm_illumos_prime_export_funcs` (`-ENOTSUP` without them), bound
/// against `dev`'s node with `attr` (NULL: the device's own attributes,
/// `-ENOTSUP` if it has none). 0 and the map in `*mapp`, or a negative
/// errno. Thread context; the caller holds `dma_buf->resv` (the exporter's
/// pin runs under it, as Linux's map_dma_buf does).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_illumos_prime_map(
    dev: *mut f::drm_device,
    dma_buf: *mut f::dma_buf,
    attr: *const f::ddi_dma_attr_t,
    mapp: *mut *mut f::drm_illumos_prime_map,
) -> i32 {
    // SAFETY: documented for thread context.
    let ctx = unsafe { super::sleepable() };
    if mapp.is_null() {
        return neg(Errno::EINVAL);
    }
    // SAFETY: the caller's device and dma-buf.
    let (Some(core), Some(h)) = (unsafe { DevCore::of(dev) }, unsafe { handle(dma_buf) }) else {
        return neg(Errno::EINVAL);
    };
    let Some(gem) = core.device().and_then(|d| d.gem().cloned()) else { return neg(Errno::ENODEV) };
    let attr = if attr.is_null() {
        None
    } else {
        // SAFETY: the caller's attributes, of the DDI's layout (opaque in
        // the facade's bindings), copied.
        Some(DmaAttr::from_raw(unsafe { *(attr as *const sys::ddi_dma_attr_t) }))
    };
    let exporter = match h.obj.object().exporter() {
        Some(e) => e.clone(),
        None => h.obj.clone(),
    };
    let base = exporter.object().base();
    let (kaddr, unpin) = match exporter.object().kernel_memory() {
        Some(k) => (k, None),
        None => {
            let funcs = exporter.object().core().setup.lock().prime_export;
            let (Some(pin), Some(unpin)) = (funcs.and_then(|x| x.pin), funcs.and_then(|x| x.unpin)) else {
                exporter.put(&ctx);
                return neg(Errno::ENOTSUP);
            };
            let mut k: *mut c_void = ptr::null_mut();
            // SAFETY: the exporting driver's callback with its live object
            // (the reference held here), from thread context.
            let r = unsafe { pin(base, &mut k) };
            if r != 0 {
                exporter.put(&ctx);
                return if r < 0 { r } else { neg(Errno::EIO) };
            }
            match NonNull::new(k.cast::<u8>()) {
                Some(k) => (k, Some(unpin)),
                None => {
                    // SAFETY: the pin above succeeded.
                    unsafe { unpin(base) };
                    exporter.put(&ctx);
                    return neg(Errno::EINVAL);
                }
            }
        }
    };
    let size = exporter.object().size();
    // SAFETY: core memory lives as long as the object, which the reference
    // keeps; pinned memory stays mapped until unpin, which only
    // drm_illumos_prime_unmap calls, after the binding is dropped.
    let binding = match unsafe { gem.bind_kernel(kaddr, size, attr.as_ref(), &ctx) } {
        Ok(b) => b,
        Err(e) => {
            if let Some(u) = unpin {
                // SAFETY: the pin above succeeded.
                unsafe { u(base) };
            }
            exporter.put(&ctx);
            return neg(e);
        }
    };
    let b = Box::into_raw(Box::new(PrimeMap { magic: 0, binding, exporter, unpin }));
    // SAFETY: just made; checked above.
    unsafe {
        (*b).magic = MAGIC_PRIME_MAP ^ (b as u64);
        *mapp = b.cast();
    }
    0
}

/// The number of DMA segments of a map; 0 for an unknown map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_illumos_prime_map_nsegs(map: *const f::drm_illumos_prime_map) -> u32 {
    // SAFETY: the caller's map.
    match unsafe { prime_map(map) } {
        Some(m) => u32::try_from(m.binding.sg_len()).unwrap_or(u32::MAX),
        None => 0,
    }
}

/// Copies the first `n` DMA segments of a map (bus address and length) to
/// `addr` and `len`; returns how many it copied, or a negative errno.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_illumos_prime_map_segs(
    map: *const f::drm_illumos_prime_map,
    n: u32,
    addr: *mut u64,
    len: *mut usize,
) -> i32 {
    // SAFETY: the caller's map.
    let Some(m) = (unsafe { prime_map(map) }) else { return neg(Errno::EINVAL) };
    if n > 0 && (addr.is_null() || len.is_null()) {
        return neg(Errno::EINVAL);
    }
    let mut copied = 0usize;
    for (i, c) in m.binding.sg_iter().take(n as usize).enumerate() {
        // SAFETY: the caller's arrays have room for n entries; i < n.
        unsafe {
            *addr.add(i) = c.address;
            *len.add(i) = c.size;
        }
        copied = i + 1;
    }
    i32::try_from(copied).unwrap_or(i32::MAX)
}

/// Unbinds a map, lets the exporter unpin its memory and drops the
/// exporter's object. Thread context, with `dma_buf->resv` held as for
/// `drm_illumos_prime_map`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_illumos_prime_unmap(map: *mut f::drm_illumos_prime_map) {
    // SAFETY: documented for thread context.
    let ctx = unsafe { super::sleepable() };
    // SAFETY: the caller gives up its map.
    if unsafe { prime_map(map) }.is_none() {
        return;
    }
    // SAFETY: a box from drm_illumos_prime_map; the mark goes first.
    let b = unsafe {
        (*(map as *mut PrimeMap)).magic = 0;
        Box::from_raw(map as *mut PrimeMap)
    };
    let PrimeMap { binding, exporter, unpin, .. } = *b;
    drop(binding);
    if let Some(u) = unpin {
        // SAFETY: pinned by drm_illumos_prime_map for this map.
        unsafe { u(exporter.object().base()) };
    }
    exporter.put(&ctx);
}
