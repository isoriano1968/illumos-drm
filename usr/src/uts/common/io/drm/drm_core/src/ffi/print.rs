// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! `drm_print.h`: the core's printing function and printers, over
//! `drm_core::print`'s rules, `vsnprintf(9F)` and `dev_err`/`cmn_err`.
//! The variadic entry points (`drm_printf`, `drm_dev_printk`, the macros)
//! are static functions of the header that build a `struct va_format`, so
//! nothing here is variadic; the `va_list` is passed on by pointer (amd64:
//! the array's address; sparcv9: the pointer it holds, see below).

use core::ffi::{CStr, c_char};
use core::ptr;
use core::sync::atomic::Ordering;

use ddi::sys;
use drm_core::print::{self, Msg};
use drm_facade_sys as f;

use super::version::drm_debug;

fn bytes<'a>(p: *const c_char) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: the callers pass NUL-terminated strings (the header's
    // prefixes, a driver's printer prefix).
    unsafe { CStr::from_ptr(p) }.to_bytes()
}

/// Formats and logs one message. Any context: no lock, no allocation.
///
/// # Safety
/// `dev` is NULL or a live node; `vaf` is NULL or a format with its
/// arguments.
unsafe fn vprintk(dev: *mut f::dev_info_t, level: u8, category: i32, prefixes: [&[u8]; 2], vaf: *mut f::va_format) {
    if !print::enabled(drm_debug.load(Ordering::Relaxed), level, category) {
        return;
    }
    // SAFETY: the caller's contract.
    let (fmt, va) = unsafe {
        if vaf.is_null() || (*vaf).fmt.is_null() || (*vaf).va.is_null() {
            return;
        }
        ((*vaf).fmt, (*vaf).va)
    };
    let mut m = Msg::new();
    for p in prefixes {
        m.prefix(p);
    }
    // amd64: va_list is an array (__va_list_tag[1]), so the C side's
    // `va_list *` is the address of its first element, which vsnprintf(9F)
    // takes. sparcv9: va_list is a pointer to the argument area, which
    // vsnprintf(9F) takes by value, so the C side's `va_list *` is read.
    #[cfg(target_arch = "sparc64")]
    // SAFETY: `va` points at the caller's live va_list (`struct va_format`).
    let ap: sys::va_list = unsafe { *(va as *const sys::va_list) };
    #[cfg(not(target_arch = "sparc64"))]
    let ap = va as sys::va_list;
    m.format(|dst, room| {
        // SAFETY: `room` bytes at `dst`; the driver's format and arguments.
        unsafe { sys::vsnprintf(dst as *mut c_char, room, fmt, ap) }
    });
    let r = print::route(level);
    let ce = if r.warn { sys::CE_WARN } else { sys::CE_NOTE };
    let how = if r.log_only { c"!%s" } else { c"%s" };
    // SAFETY: a "%s" format with the composed, NUL-terminated message; dev
    // is a live node or NULL.
    unsafe {
        if dev.is_null() {
            sys::cmn_err(ce, how.as_ptr(), m.as_ptr());
        } else {
            sys::dev_err(dev as sys::dev_info_t, ce, how.as_ptr(), m.as_ptr());
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn drm_illumos_vprintk(
    dev: *mut f::dev_info_t,
    level: *const c_char,
    category: i32,
    prefix: *const c_char,
    vaf: *mut f::va_format,
) {
    let lvl = print::parse_level((!level.is_null()).then(|| bytes(level)));
    // SAFETY: the caller's arguments.
    unsafe { vprintk(dev, lvl, category, [bytes(prefix), &[]], vaf) };
}

/// The node of a printer's device (`arg`, a `struct drm_device`).
///
/// # Safety
/// `p` is a live printer whose `arg` is NULL or a live device.
///
/// The node comes from the core's state, not from `drm_device.dev`: since
/// facade 1.1 a compatibility layer may keep its own device there
/// (`DRM_FACADE_DEVICE_T`).
unsafe fn printer_dev(p: *mut f::drm_printer) -> *mut f::dev_info_t {
    // SAFETY: the caller's contract: arg is NULL or a live device.
    unsafe {
        let d = (*p).arg as *const f::drm_device;
        crate::devcore::DevCore::of(d).map_or(ptr::null_mut(), |c| c.dip.as_raw() as *mut f::dev_info_t)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn __drm_printfn_info(p: *mut f::drm_printer, vaf: *mut f::va_format) {
    if p.is_null() {
        return;
    }
    // SAFETY: an info printer's arg is the node.
    unsafe { vprintk((*p).arg as *mut f::dev_info_t, print::LEVEL_INFO, -1, [b"[drm]", &[]], vaf) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn __drm_printfn_dbg(p: *mut f::drm_printer, vaf: *mut f::va_format) {
    if p.is_null() {
        return;
    }
    // SAFETY: a debug printer's arg is the device.
    unsafe {
        vprintk(printer_dev(p), print::LEVEL_DEBUG, (*p).category as i32, [b"[drm:dbg]", bytes((*p).prefix)], vaf)
    };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn __drm_printfn_err(p: *mut f::drm_printer, vaf: *mut f::va_format) {
    if p.is_null() {
        return;
    }
    // SAFETY: an error printer's arg is the device.
    unsafe { vprintk(printer_dev(p), 3, -1, [b"[drm] *ERROR*", bytes((*p).prefix)], vaf) };
}
