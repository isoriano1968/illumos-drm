// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! File descriptors for dma-bufs, sync objects and sync files.
//!
//! Linux hands these objects to user space as anonymous files. Here each
//! object that gets a file descriptor gets a minor number of the DRM
//! driver of its own, an *anonymous minor*, and the descriptor is a specfs
//! file for that device number, created by `ddi::private::fd` from inside
//! the ioctl (`makespecvp`, `VOP_OPEN`, `falloc`, `setf`, as `pipe(2)`
//! does). Every later operation on the file reaches the driver's own entry
//! points with that minor: `close(9E)` when the last file of it is closed
//! drops the object, `chpoll(9E)` polls a sync file, `devmap(9E)` maps a
//! dma-buf, `ioctl(9E)` serves the sync file and dma-buf ioctls. Passing
//! the descriptor with `SCM_RIGHTS` or inheriting it through `fork(2)`
//! shares the file as it would any other, and importing it
//! (`PRIME_FD_TO_HANDLE`, `SYNCOBJ_FD_TO_HANDLE`, `IMPORT_SYNC_FILE`)
//! looks the descriptor up with `getf`, checks that it is a character
//! device of this driver, and finds the object by its minor. The driver
//! cannot detach while such a file is open, because specfs holds the
//! device node open. `docs/anon-fd.md` records the choice and the
//! alternatives.
//!
//! # Minor numbers
//!
//! An anonymous minor has bit 30 set, the device index (the instance) in
//! bits 24 to 29 and a number from 1 to 2^24 - 1 in the low 24 bits. DRM
//! file minors use the low 18 bits only ([`crate::minor`]), so the ranges
//! are disjoint, and [`crate::minor::instance_of_minor`] maps both kinds
//! to the instance. The numbers exceed 18 bits, so a 32-bit process that
//! receives such a descriptor gets `EOVERFLOW` from `fstat(2)`; only the
//! LP64 ABI is supported anyway.
//!
//! # Opens
//!
//! The only way to open an anonymous minor is the export path: the entry
//! is created with one pending open, the driver's `open(9E)` accepts the
//! minor only while an open is pending, and the pending count drops when
//! the export returns. A device node made with `mknod(2)` for the number
//! therefore cannot be opened. Exporting a GEM object again while its
//! dma-buf file is open opens the same minor again (one dma-buf per
//! object, as Linux keeps `obj->dma_buf`); if that races with the last
//! close, the booked open keeps the entry and its number ([`AnonTable`]),
//! and the export opens the same object again.
//!
//! Sync object files and sync files of one device can be imported into
//! another device of the same driver (see `fence_dev::SyncCore::lookup`);
//! dma-bufs cannot (`ENOTSUP`).
//!
//! # Descriptors of driver ioctls
//!
//! The core's own ioctls copy a new descriptor's number out to the caller
//! before they make the descriptor visible, so an ioctl whose copy fails
//! leaves no descriptor behind. A driver's handler (`dma_buf_fd`,
//! `drm_sync_file_install`) gets the number while its argument is still
//! the kernel's copy, which the facade driver copies out only after the
//! handler has returned. Such descriptors are therefore only reserved
//! ([`defer_fd`]) and kept in the ioctl's [`PendingFds`]; [`install_after`]
//! installs them once the handler succeeded and the argument was copied
//! out, and gives them up otherwise. This is the order of `pipe(2)`
//! (`falloc`, `copyout`, `setf`). It is stricter than Linux, whose i915
//! installs its out-fence inside the handler (`fd_install` in
//! `i915_gem_do_execbuffer`). Closing an installed descriptor after a
//! failed copy instead would race with the other threads of the process,
//! which may already have used, duplicated or replaced it.

use core::ptr::NonNull;

use ddi::{Errno, Sleepable};

use crate::idmap::{IdAlloc, IdMap};
use crate::minor::MAX_DEVICES;

/// The bit that marks an anonymous minor.
pub const ANON_BIT: u32 = 1 << 30;
/// The shift of the device index.
pub const INDEX_SHIFT: u32 = 24;
/// The mask of the object number.
pub const ID_MASK: u32 = (1 << INDEX_SHIFT) - 1;

/// Returns the anonymous minor of object number `id` of device `index`.
pub const fn minor(index: u32, id: u32) -> Option<u32> {
    if index >= MAX_DEVICES || id == 0 || id > ID_MASK {
        return None;
    }
    Some(ANON_BIT | (index << INDEX_SHIFT) | id)
}

/// Takes an anonymous minor apart into the device index and the object
/// number; `None` for any other minor.
pub const fn decode(minor: u32) -> Option<(u32, u32)> {
    if minor & !(ANON_BIT | (0x3f << INDEX_SHIFT) | ID_MASK) != 0 || minor & ANON_BIT == 0 {
        return None;
    }
    let id = minor & ID_MASK;
    if id == 0 {
        return None;
    }
    Some(((minor >> INDEX_SHIFT) & 0x3f, id))
}

/// One anonymous minor's entry: the object and the opens in flight.
struct Entry<V> {
    obj: V,
    /// Opens the export path has booked and not finished.
    pending: u32,
    /// Whether the minor is open, so that its `close(9E)` will come.
    opened: bool,
}

/// The table of a device's anonymous minors, as plain logic (the kernel
/// layer keeps it behind a mutex and adds the file descriptors).
///
/// An entry is created by an export with one booked open, and its number
/// stays taken until the entry goes. `open(9E)` succeeds only while an
/// open is booked. `close(9E)` of the last file removes the entry, except
/// while an export has booked another open of the same number: then the
/// entry and its number stay, so that the booked open (which specfs runs
/// after the close) finds the same object, and [`AnonTable::settle`]
/// removes it if that open fails. Without this, a close racing with a
/// second export of the same dma-buf could free the number, a third export
/// could take it for another object, and the second export's open would
/// land on that object.
pub struct AnonTable<V, A: IdAlloc> {
    map: IdMap<Entry<V>, A>,
}

impl<V, A: IdAlloc> AnonTable<V, A> {
    /// An empty table drawing numbers from `ids`.
    pub const fn new(ids: A) -> AnonTable<V, A> {
        AnonTable { map: IdMap::new(ids) }
    }

    /// Creates an entry for `obj` with one booked open and returns its
    /// number; the object comes back with the error if no number is left.
    pub fn insert(&mut self, obj: V, ctx: &Sleepable) -> Result<u32, (V, Errno)> {
        self.map.insert(Entry { obj, pending: 1, opened: false }, ctx).map_err(|(e, err)| (e.obj, err))
    }

    /// Books another open of entry `id` if it exists and `same` accepts its
    /// object (a second export of the same dma-buf).
    pub fn book(&mut self, id: u32, same: impl FnOnce(&V) -> bool) -> bool {
        match self.map.get_mut(id) {
            Some(e) if same(&e.obj) => {
                e.pending = e.pending.saturating_add(1);
                true
            }
            _ => false,
        }
    }

    /// `open(9E)` of `id`: allowed only while an open is booked.
    pub fn open(&mut self, id: u32) -> Result<(), Errno> {
        match self.map.get_mut(id) {
            Some(e) if e.pending > 0 => {
                e.opened = true;
                Ok(())
            }
            _ => Err(Errno::ENXIO),
        }
    }

    /// Ends a booked open of `id` that succeeded (`ok`) or failed; returns
    /// the object if the entry went because no file has it open and no
    /// other open is booked.
    pub fn settle(&mut self, id: u32, ok: bool) -> Option<V> {
        let e = self.map.get_mut(id)?;
        e.pending = e.pending.saturating_sub(1);
        if !ok && !e.opened && e.pending == 0 {
            return self.map.remove(id).map(|e| e.obj);
        }
        None
    }

    /// `close(9E)` of `id`, the last file of the minor: returns the object
    /// of the entry, which goes, unless an open is booked (see the type
    /// documentation).
    pub fn close(&mut self, id: u32) -> Option<V> {
        let e = self.map.get_mut(id)?;
        if e.pending > 0 {
            e.opened = false;
            return None;
        }
        self.map.remove(id).map(|e| e.obj)
    }

    /// Returns the object of entry `id`.
    pub fn get(&self, id: u32) -> Option<&V> {
        self.map.get(id).map(|e| &e.obj)
    }

    /// Removes any entry, for teardown.
    pub fn pop_first(&mut self) -> Option<(u32, V)> {
        self.map.pop_first().map(|(id, e)| (id, e.obj))
    }

    /// Returns the number of entries.
    pub fn len(&self) -> usize {
        self.map.len()
    }

    /// Reports whether the table is empty.
    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }
}

/// A descriptor reserved in the calling process but not visible to it
/// (`ddi::private::fd::NewFd`). Dropping it gives the descriptor up again,
/// with its file and the object reference the file holds.
pub trait ReservedFd {
    /// Returns the descriptor number.
    fn fd(&self) -> i32;

    /// Makes the descriptor visible to the process, which owns it from
    /// then on.
    fn install(self);
}

/// The most descriptors one driver ioctl can ask for. The drivers ask for
/// one at most (an out-fence or an exported buffer); a further request
/// fails with `EMFILE`, as Linux's `get_unused_fd_flags` does when no
/// descriptor is left.
pub const MAX_PENDING_FDS: usize = 4;

/// The descriptors a driver ioctl in progress asked for, in the order of
/// the requests. Dropping the list gives them all up;
/// [`PendingFds::commit`] installs them.
pub struct PendingFds<F: ReservedFd> {
    fds: [Option<F>; MAX_PENDING_FDS],
}

impl<F: ReservedFd> PendingFds<F> {
    /// An empty list.
    pub const fn new() -> PendingFds<F> {
        PendingFds { fds: [const { None }; MAX_PENDING_FDS] }
    }

    /// Returns the number of descriptors asked for.
    pub fn len(&self) -> usize {
        self.fds.iter().filter(|f| f.is_some()).count()
    }

    /// Reports whether no descriptor was asked for.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Installs the descriptors, in the order they were asked for.
    pub fn commit(mut self) {
        for f in self.fds.iter_mut().filter_map(Option::take) {
            f.install();
        }
    }
}

impl<F: ReservedFd> Default for PendingFds<F> {
    fn default() -> PendingFds<F> {
        PendingFds::new()
    }
}

/// Asks for a descriptor for the driver ioctl whose list is `pending`:
/// `open` reserves it, and its number is returned for the handler to put
/// in the kernel copy of the argument. The descriptor becomes visible only
/// when the list is committed. Without a list (outside a driver ioctl, or
/// in one whose argument is not copied out through [`install_after`])
/// `EINVAL`, and with a full list `EMFILE`; `open` is not called then, so
/// nothing is ever installed at once.
pub fn defer_fd<F: ReservedFd>(
    pending: Option<&mut PendingFds<F>>,
    open: impl FnOnce() -> Result<F, Errno>,
) -> Result<i32, Errno> {
    let p = pending.ok_or(Errno::EINVAL)?;
    let slot = p.fds.iter_mut().find(|f| f.is_none()).ok_or(Errno::EMFILE)?;
    let f = open()?;
    let fd = f.fd();
    *slot = Some(f);
    Ok(fd)
}

/// Runs a driver ioctl: `handler` calls the driver with the kernel copy of
/// the argument and the list recorded where [`defer_fd`] finds it, then
/// `copy_out` copies the argument out to the caller. The descriptors the
/// handler asked for are installed only if both succeed; on any error they
/// are given up and the error is returned, so a failed ioctl leaves none
/// behind.
///
/// `handler` must not use the list after it has returned.
pub fn install_after<F: ReservedFd>(
    handler: impl FnOnce(NonNull<PendingFds<F>>) -> Result<(), Errno>,
    copy_out: impl FnOnce() -> Result<(), Errno>,
) -> Result<(), Errno> {
    let mut pending = PendingFds::new();
    handler(NonNull::from(&mut pending))?;
    copy_out()?;
    pending.commit();
    Ok(())
}

#[cfg(not(test))]
pub(crate) use imp::*;

#[cfg(not(test))]
mod imp {
    use core::sync::atomic::Ordering;

    use ddi::prelude::*;
    use ddi::private::fd::{FdFlags, NewFd};

    use crate::fence::SyncStats;
    use crate::gem::{GemCore, GemObject, GemRef, KIdSpace, NameBuf};
    use crate::karc::KArc;
    use crate::ksync::KMutex;
    use crate::sync_file::SyncFile;
    use crate::syncobj::Syncobj;

    impl super::ReservedFd for NewFd<'_> {
        fn fd(&self) -> i32 {
            NewFd::fd(self)
        }

        fn install(self) {
            NewFd::install(self)
        }
    }

    /// Copies the number of `nfd` out with `write_fd` and installs the
    /// descriptor if that succeeded; drops it, which undoes the export,
    /// otherwise.
    pub(crate) fn install_written(
        nfd: NewFd<'_>,
        write_fd: impl FnOnce(i32) -> Result<(), Errno>,
    ) -> Result<(), Errno> {
        write_fd(nfd.fd())?;
        nfd.install();
        Ok(())
    }

    /// What an anonymous file stands for.
    pub(crate) enum AnonObj<O: GemObject> {
        /// A dma-buf: an exported GEM object.
        DmaBuf(GemRef<O>),
        /// An exported sync object.
        Syncobj(Syncobj),
        /// A sync file.
        SyncFile(KArc<SyncFile>),
    }

    impl<O: GemObject> AnonObj<O> {
        fn same(&self, other: &AnonObj<O>) -> bool {
            match (self, other) {
                (AnonObj::DmaBuf(a), AnonObj::DmaBuf(b)) => crate::objects::ObjRef::same(a, b),
                _ => false,
            }
        }
        fn clone_ref(&self) -> AnonObj<O> {
            match self {
                AnonObj::DmaBuf(g) => AnonObj::DmaBuf(g.clone()),
                AnonObj::Syncobj(s) => AnonObj::Syncobj(s.clone()),
                AnonObj::SyncFile(f) => AnonObj::SyncFile(f.clone()),
            }
        }
        /// Drops the reference in thread context.
        pub(crate) fn put(self, ctx: &Sleepable) {
            match self {
                AnonObj::DmaBuf(g) => g.put(ctx),
                AnonObj::Syncobj(s) => drop(s),
                AnonObj::SyncFile(f) => SyncFile::put(f, ctx),
            }
        }
    }

    /// The anonymous files of one device.
    pub(crate) struct AnonFiles<O: GemObject> {
        index: u32,
        table: KMutex<super::AnonTable<AnonObj<O>, KIdSpace>>,
    }

    impl<O: GemObject> AnonFiles<O> {
        /// The table of device number `index`, the device of `dev`. The
        /// identifier space is named after the driver instance, like the
        /// device's other arenas and its kstats.
        pub(crate) fn new(dev: &DevInfo, index: u32, ctx: &Sleepable) -> Result<AnonFiles<O>, Errno> {
            let drv = dev.driver_name().to_str().map_err(|_| Errno::EINVAL)?;
            let name = NameBuf::new(format_args!("{drv}{}.anon", dev.instance().0))?;
            let ids = KIdSpace::with_range(name.as_cstr(), 1, super::ID_MASK as i32 + 1, ctx)?;
            Ok(AnonFiles { index, table: KMutex::new(super::AnonTable::new(ids)) })
        }

        /// Hands `obj` to the calling process as a new file descriptor
        /// with `flags`. `write_fd` receives the descriptor number before
        /// it becomes visible (to copy it out); if it fails, everything is
        /// undone. For a dma-buf the object's existing minor is reused while
        /// its file is open.
        #[allow(clippy::too_many_arguments, reason = "the export needs all of them")]
        pub(crate) fn export(
            &self,
            dev: &DevInfo,
            obj: AnonObj<O>,
            flags: FdFlags,
            arg: &IoctlArg<'_>,
            write_fd: impl FnOnce(i32) -> Result<(), Errno>,
            stats: &SyncStats,
            ctx: &Sleepable,
        ) -> Result<(), Errno> {
            install_written(self.open_fd(dev, obj, flags, arg, stats, ctx)?, write_fd)
        }

        /// Opens `obj` as a new file with `flags` and reserves a descriptor
        /// for it in the calling process, not visible to the process until
        /// it is installed; dropping it undoes everything, the file's
        /// object reference included. The first half of
        /// [`AnonFiles::export`], for descriptors that a driver's handler
        /// asks for and the facade driver installs after its copy-out.
        pub(crate) fn open_fd<'a>(
            &self,
            dev: &DevInfo,
            obj: AnonObj<O>,
            flags: FdFlags,
            arg: &'a IoctlArg<'_>,
            stats: &SyncStats,
            ctx: &Sleepable,
        ) -> Result<NewFd<'a>, Errno> {
            // A dma-buf reuses its object's open minor (booked, so a racing
            // last close keeps it); anything else gets a new one.
            let (id, _fresh, spare) = match self.reserve(obj, true, stats, ctx) {
                Ok(v) => v,
                Err((o, e)) => {
                    o.put(ctx);
                    return Err(e);
                }
            };
            if let Some(s) = spare {
                s.put(ctx);
            }
            let m = super::minor(self.index, id).ok_or(Errno::ENXIO);
            let r = m.and_then(|m| NewFd::open(dev, Minor(m), flags, arg, ctx));
            let leftover = self.table.lock().settle(id, r.is_ok());
            if let Some(lo) = leftover {
                self.dropped(id, lo, stats, ctx);
            }
            r
        }

        /// Finds or creates the entry for `obj` and books one open. Returns
        /// the number, whether the entry is new, and the reference that was
        /// not needed because an entry existed.
        #[allow(clippy::type_complexity, reason = "private helper")]
        fn reserve(
            &self,
            obj: AnonObj<O>,
            reuse: bool,
            stats: &SyncStats,
            ctx: &Sleepable,
        ) -> Result<(u32, bool, Option<AnonObj<O>>), (AnonObj<O>, Errno)> {
            let mut t = self.table.lock();
            if reuse && let AnonObj::DmaBuf(g) = &obj {
                let id = g.dmabuf().load(Ordering::Acquire);
                if id != 0 && t.book(id, |o| o.same(&obj)) {
                    return Ok((id, false, Some(obj)));
                }
            }
            let dmabuf = match &obj {
                AnonObj::DmaBuf(g) => Some(g.clone()),
                _ => None,
            };
            let id = match t.insert(obj, ctx) {
                Ok(id) => id,
                Err((o, err)) => {
                    drop(t);
                    if let Some(g) = dmabuf {
                        g.put(ctx);
                    }
                    return Err((o, err));
                }
            };
            if let Some(g) = dmabuf {
                g.dmabuf().store(id, Ordering::Release);
                SyncStats::inc(&stats.dmabufs);
                drop(t);
                g.put(ctx);
            }
            SyncStats::inc(&stats.anon_files);
            Ok((id, true, None))
        }

        /// Accounts for an entry that went away and drops its object.
        fn dropped(&self, id: u32, obj: AnonObj<O>, stats: &SyncStats, ctx: &Sleepable) {
            SyncStats::dec(&stats.anon_files);
            if let AnonObj::DmaBuf(g) = &obj {
                let _ = g.dmabuf().compare_exchange(id, 0, Ordering::AcqRel, Ordering::Acquire);
                SyncStats::dec(&stats.dmabufs);
                g.core().dmabuf_closed(id, ctx);
            }
            obj.put(ctx);
        }

        /// `open(9E)` of anonymous minor `id`: allowed only while an export
        /// is opening it.
        pub(crate) fn open(&self, id: u32) -> Result<(), Errno> {
            self.table.lock().open(id)
        }

        /// `close(9E)` of anonymous minor `id`: the last file is gone, the
        /// object reference with it.
        pub(crate) fn close(&self, id: u32, stats: &SyncStats, ctx: &Sleepable) {
            let e = self.table.lock().close(id);
            if let Some(o) = e {
                self.dropped(id, o, stats, ctx);
            }
        }

        /// Returns a reference to the object of anonymous minor `id`.
        pub(crate) fn get(&self, id: u32) -> Option<AnonObj<O>> {
            self.table.lock().get(id).map(AnonObj::clone_ref)
        }

        /// The size in bytes of the dma-buf of anonymous minor `id`, for
        /// the "Size" property specfs reads for `stat(2)` and
        /// `lseek(SEEK_END)` (Linux's `dma_buf_llseek` returns the size;
        /// GStreamer and GTK size an imported buffer that way). `None`
        /// for a sync object, a sync file or a free minor.
        pub(crate) fn dmabuf_size(&self, id: u32, ctx: &Sleepable) -> Option<u64> {
            match self.get(id) {
                Some(AnonObj::DmaBuf(g)) => {
                    let size = g.size() as u64;
                    g.put(ctx);
                    Some(size)
                }
                Some(o) => {
                    o.put(ctx);
                    None
                }
                None => None,
            }
        }

        /// Drops every entry (detach, after every file has closed: nothing
        /// is left then; this only guards against a leak).
        pub(crate) fn clear(&self, stats: &SyncStats, ctx: &Sleepable) {
            loop {
                let e = self.table.lock().pop_first();
                let Some((id, o)) = e else { break };
                self.dropped(id, o, stats, ctx);
            }
        }

        /// Maps the dma-buf of anonymous minor `id` (`devmap(9E)`).
        pub(crate) fn devmap(
            &self,
            id: u32,
            gem: &KArc<GemCore<O>>,
            dhp: &DevmapHandle<'_>,
            off: u64,
            len: usize,
            ctx: &Sleepable,
        ) -> Result<usize, Errno> {
            match self.get(id) {
                Some(AnonObj::DmaBuf(g)) => gem.devmap_dmabuf(id, g, dhp, off, len, ctx),
                Some(o) => {
                    o.put(ctx);
                    Err(Errno::ENXIO)
                }
                None => Err(Errno::ENXIO),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    extern crate std;
    use super::*;
    use crate::minor;

    use crate::fence::{Fence, FenceDomain};
    use crate::idmap::FirstFit;
    use crate::karc::KArc;
    use core::cell::{Cell, RefCell};
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::{Arc, Mutex};
    use std::thread;
    use std::vec::Vec;

    fn sleep() -> Sleepable {
        // SAFETY: host test threads may block.
        unsafe { Sleepable::assume() }
    }

    #[test]
    fn booking_rules() {
        let ctx = sleep();
        let mut t: AnonTable<u32, FirstFit> = AnonTable::new(FirstFit::new(1, 100));
        let a = t.insert(7, &ctx).ok().unwrap();
        assert_eq!(t.open(a), Ok(()));
        assert_eq!(t.settle(a, true), None);
        assert_eq!(t.open(a), Err(Errno::ENXIO), "no open booked: a mknod'd node cannot open");
        // A second export books an open; the last close races it.
        assert!(t.book(a, |o| *o == 7));
        assert!(!t.book(a, |o| *o == 8), "another object");
        assert_eq!(t.close(a), None, "the booked open keeps the entry and its number");
        let b = t.insert(8, &ctx).ok().unwrap();
        assert_ne!(a, b, "the number is still taken");
        assert_eq!(t.open(a), Ok(()), "the booked open finds the same object");
        assert_eq!(t.get(a), Some(&7));
        assert_eq!(t.settle(a, true), None);
        assert_eq!(t.close(a), Some(7));
        // A booked open that fails after the last close removes the entry.
        assert!(t.book(b, |_| true));
        assert_eq!(t.close(b), None);
        assert_eq!(t.settle(b, false), None, "the export's own booking is still open");
        assert_eq!(t.settle(b, false), Some(8));
        assert!(t.is_empty());
        // A failed first open of a new entry removes it.
        let c = t.insert(9, &ctx).ok().unwrap();
        assert_eq!(t.settle(c, false), Some(9));
        assert!(t.is_empty());
    }

    /// Exports race last closes as in the kernel: specfs serialises open(9E)
    /// and close(9E) of one number (a mutex per number here) and calls
    /// close(9E) when the last file goes. Eight threads export three objects
    /// again and again, re-using an object's number while its file is open,
    /// and close their files at random. Every file must refer to the object
    /// it was exported for, and every entry must go exactly once.
    #[test]
    fn exports_race_last_close() {
        const OBJECTS: usize = 3;
        struct Spec {
            files: usize,
        }
        let table: Arc<Mutex<AnonTable<usize, FirstFit>>> = Arc::new(Mutex::new(AnonTable::new(FirstFit::new(1, 64))));
        // Per number: the specfs open count (the common snode's lock).
        let spec: Arc<Vec<Mutex<Spec>>> = Arc::new((0..64).map(|_| Mutex::new(Spec { files: 0 })).collect());
        // Per object: its current number, 0 for none (the GEM object's
        // dma-buf field).
        let current: Arc<Vec<AtomicUsize>> = Arc::new((0..OBJECTS).map(|_| AtomicUsize::new(0)).collect());
        let removed = Arc::new(AtomicUsize::new(0));
        let created = Arc::new(AtomicUsize::new(0));
        let hs: Vec<_> = (0..8u64)
            .map(|t| {
                let (table, spec, current, removed, created) =
                    (table.clone(), spec.clone(), current.clone(), removed.clone(), created.clone());
                thread::spawn(move || {
                    let ctx = sleep();
                    let mut seed = t * 31 + 7;
                    let mut files: Vec<(u32, usize)> = Vec::new();
                    for _ in 0..3000 {
                        seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                        if !files.is_empty() && (files.len() >= 2 || (seed >> 40) % 2 == 0) {
                            let (id, _) = files.swap_remove(((seed >> 20) as usize) % files.len());
                            let mut sp = spec[id as usize].lock().unwrap();
                            sp.files -= 1;
                            if sp.files == 0
                                && let Some(o) = table.lock().unwrap().close(id)
                            {
                                let _ =
                                    current[o].compare_exchange(id as usize, 0, Ordering::AcqRel, Ordering::Acquire);
                                removed.fetch_add(1, Ordering::SeqCst);
                            }
                            continue;
                        }
                        let obj = ((seed >> 33) as usize) % OBJECTS;
                        // Book an open of the object's number, or a new one.
                        let id = {
                            let mut tb = table.lock().unwrap();
                            let cur = current[obj].load(Ordering::Acquire) as u32;
                            if cur != 0 && tb.book(cur, |o| *o == obj) {
                                cur
                            } else {
                                let id = tb.insert(obj, &ctx).ok().unwrap();
                                current[obj].store(id as usize, Ordering::Release);
                                created.fetch_add(1, Ordering::SeqCst);
                                id
                            }
                        };
                        thread::yield_now();
                        // spec_open: serialised with close on the number.
                        let ok = {
                            let mut sp = spec[id as usize].lock().unwrap();
                            let ok = table.lock().unwrap().open(id).is_ok();
                            if ok {
                                sp.files += 1;
                                assert_eq!(table.lock().unwrap().get(id), Some(&obj), "the file is the object's");
                            }
                            ok
                        };
                        assert!(ok, "a booked open always succeeds");
                        if let Some(o) = table.lock().unwrap().settle(id, ok) {
                            let _ = current[o].compare_exchange(id as usize, 0, Ordering::AcqRel, Ordering::Acquire);
                            removed.fetch_add(1, Ordering::SeqCst);
                        }
                        files.push((id, obj));
                    }
                    for (id, obj) in files {
                        let mut sp = spec[id as usize].lock().unwrap();
                        assert_eq!(table.lock().unwrap().get(id), Some(&obj));
                        sp.files -= 1;
                        if sp.files == 0
                            && let Some(o) = table.lock().unwrap().close(id)
                        {
                            let _ = current[o].compare_exchange(id as usize, 0, Ordering::AcqRel, Ordering::Acquire);
                            removed.fetch_add(1, Ordering::SeqCst);
                        }
                    }
                })
            })
            .collect();
        for h in hs {
            h.join().unwrap();
        }
        assert!(table.lock().unwrap().is_empty(), "every entry went");
        assert_eq!(removed.load(Ordering::SeqCst), created.load(Ordering::SeqCst), "exactly once");
        assert!(created.load(Ordering::SeqCst) > OBJECTS, "numbers were recycled");
    }

    #[test]
    fn anonymous_minors_are_disjoint_from_file_minors() {
        let m = minor(3, 77).unwrap();
        assert_eq!(decode(m), Some((3, 77)));
        assert_eq!(minor(63, ID_MASK).and_then(decode), Some((63, ID_MASK)));
        assert_eq!(minor(64, 1), None);
        assert_eq!(minor(0, 0), None);
        assert_eq!(minor(0, ID_MASK + 1), None);
        assert!(minor::decode(m).is_none(), "not a DRM file minor");
        for f in [
            0,
            128,
            0x3fe00 | 5,
            minor::file_minor(minor::NodeKind::Render, 2, minor::CloneId::new(511).unwrap()).unwrap(),
        ] {
            assert_eq!(decode(f), None, "{f:#x}");
        }
        assert_eq!(decode(ANON_BIT), None, "number 0 is never used");
        assert_eq!(decode(m | (1 << 31)), None);
        // The instance comes from the device number's holder in this
        // driver (number 10: no other test uses it).
        let m = minor(10, 77).unwrap();
        assert_eq!(minor::instance_of_minor(ddi::Minor(m)), None);
        assert!(minor::bind(10, ddi::Instance(2)));
        assert_eq!(minor::instance_of_minor(ddi::Minor(m)), Some(ddi::Instance(2)));
        assert!(minor::unbind(10, ddi::Instance(2)));
    }

    /// One slot of a fake descriptor table.
    enum Slot<T> {
        Free,
        /// Reserved by falloc, no file yet (uf_busy set, uf_file NULL).
        Reserved,
        /// Installed by setf; the file holds the object reference.
        Open(T),
    }

    /// A process's descriptor table as falloc, setf and unfalloc leave it.
    struct FakeTable<T> {
        slots: RefCell<Vec<Slot<T>>>,
    }

    impl<T> FakeTable<T> {
        fn new() -> FakeTable<T> {
            FakeTable { slots: RefCell::new(Vec::new()) }
        }

        /// falloc: reserves the lowest free descriptor for a file that
        /// holds `obj`.
        fn reserve(&self, obj: T) -> FakeFd<'_, T> {
            let mut s = self.slots.borrow_mut();
            let fd = match s.iter().position(|x| matches!(x, Slot::Free)) {
                Some(i) => i,
                None => {
                    s.push(Slot::Free);
                    s.len() - 1
                }
            };
            s[fd] = Slot::Reserved;
            FakeFd { table: self, fd: fd as i32, obj: Some(obj) }
        }

        fn state(&self, fd: i32) -> &'static str {
            match self.slots.borrow().get(fd as usize) {
                None | Some(Slot::Free) => "free",
                Some(Slot::Reserved) => "reserved",
                Some(Slot::Open(_)) => "open",
            }
        }

        fn all_free(&self) -> bool {
            self.slots.borrow().iter().all(|x| matches!(x, Slot::Free))
        }

        /// close(2) of an installed descriptor: returns the file's object
        /// reference.
        fn close(&self, fd: i32) -> Option<T> {
            let mut s = self.slots.borrow_mut();
            match core::mem::replace(&mut s[fd as usize], Slot::Free) {
                Slot::Open(o) => Some(o),
                other => {
                    s[fd as usize] = other;
                    None
                }
            }
        }
    }

    /// A reserved descriptor of a FakeTable, as NewFd is one of the
    /// process.
    struct FakeFd<'t, T> {
        table: &'t FakeTable<T>,
        fd: i32,
        obj: Option<T>,
    }

    impl<T> ReservedFd for FakeFd<'_, T> {
        fn fd(&self) -> i32 {
            self.fd
        }

        fn install(mut self) {
            let o = self.obj.take().unwrap();
            self.table.slots.borrow_mut()[self.fd as usize] = Slot::Open(o);
        }
    }

    impl<T> Drop for FakeFd<'_, T> {
        /// setf(fd, NULL) and unfalloc: the descriptor is free again, and
        /// the file goes with its object reference.
        fn drop(&mut self) {
            if let Some(o) = self.obj.take() {
                self.table.slots.borrow_mut()[self.fd as usize] = Slot::Free;
                drop(o);
            }
        }
    }

    /// The facade driver's copy-out: the kernel copy of the argument to the
    /// caller's memory, `EFAULT` if that is not writable.
    fn copy_out(kernel: &Cell<i32>, user: &Cell<i32>, fault: bool) -> Result<(), Errno> {
        if fault {
            return Err(Errno::EFAULT);
        }
        user.set(kernel.get());
        Ok(())
    }

    /// What the facade's installers do from inside a handler: ask the
    /// frame's list for a descriptor whose file holds `obj`.
    fn request<'t, T>(p: NonNull<PendingFds<FakeFd<'t, T>>>, table: &'t FakeTable<T>, obj: T) -> Result<i32, Errno> {
        // SAFETY: the list of the running install_after, used on its
        // thread while the handler runs, as through the frame.
        defer_fd(Some(unsafe { &mut *p.as_ptr() }), || Ok(table.reserve(obj)))
    }

    /// An out-fence's sync file (drm_sync_file_install): `fault` makes the
    /// final copy-out fail.
    fn sync_file_ioctl(fault: bool) {
        let ctx = sleep();
        let d = FenceDomain::new_test(&ctx);
        let f = Fence::new(&d, d.alloc_contexts(1), 1, &ctx).unwrap();
        let before = f.refs();
        let table = FakeTable::new();
        let (kernel, user) = (Cell::new(-1), Cell::new(-1));
        let r = install_after(
            |p| {
                let fd = request(p, &table, f.clone())?;
                assert_eq!(table.state(fd), "reserved", "not visible while the handler runs");
                assert_eq!(f.refs(), before + 1, "the reserved file holds the fence");
                kernel.set(fd);
                Ok(())
            },
            || copy_out(&kernel, &user, fault),
        );
        if fault {
            assert_eq!(r, Err(Errno::EFAULT));
            assert_eq!(user.get(), -1);
            assert!(table.all_free(), "no descriptor installed, the slot free again");
            assert_eq!(f.refs(), before, "the file's fence reference went");
            assert_eq!(table.reserve(f.clone()).fd(), kernel.get(), "the number is free for reuse");
        } else {
            assert_eq!(r, Ok(()));
            assert_eq!(user.get(), kernel.get(), "the caller learns the number");
            assert_eq!(table.state(user.get()), "open");
            assert_eq!(f.refs(), before + 1, "the installed file holds the fence");
            drop(table.close(user.get()));
        }
        assert_eq!(f.refs(), before);
        drop(f);
        d.drain(&ctx);
        assert_eq!(d.stats.live(), [0; 8]);
    }

    #[test]
    fn sync_file_installed_after_copy_out() {
        sync_file_ioctl(false);
    }

    #[test]
    fn sync_file_copy_out_fault_installs_nothing() {
        sync_file_ioctl(true);
    }

    /// A dma-buf export (dma_buf_fd): the descriptor's file holds its own
    /// object reference and takes over the caller's dma-buf reference on
    /// success, as on Linux.
    fn dmabuf_ioctl(fault: bool) {
        let ctx = sleep();
        let obj = KArc::new(7u32, &ctx).unwrap();
        let before = KArc::strong_count(&obj);
        let table = FakeTable::new();
        let (kernel, user) = (Cell::new(-1), Cell::new(-1));
        let r = install_after(
            |p| {
                // drm_gem_prime_export: the caller's dma-buf reference.
                let dmabuf = obj.clone();
                let fd = request(p, &table, obj.clone())?;
                drop(dmabuf);
                assert_eq!(table.state(fd), "reserved");
                kernel.set(fd);
                Ok(())
            },
            || copy_out(&kernel, &user, fault),
        );
        if fault {
            assert_eq!(r, Err(Errno::EFAULT));
            assert!(table.all_free());
            assert_eq!(KArc::strong_count(&obj), before, "the object is back to its references before the ioctl");
        } else {
            assert_eq!(r, Ok(()));
            assert_eq!(user.get(), kernel.get());
            assert_eq!(table.state(user.get()), "open");
            assert_eq!(KArc::strong_count(&obj), before + 1, "the installed file holds the object");
            drop(table.close(user.get()));
            assert_eq!(KArc::strong_count(&obj), before);
        }
    }

    #[test]
    fn dmabuf_installed_after_copy_out() {
        dmabuf_ioctl(false);
    }

    #[test]
    fn dmabuf_copy_out_fault_installs_nothing() {
        dmabuf_ioctl(true);
    }

    #[test]
    fn handler_error_after_request_installs_nothing() {
        let ctx = sleep();
        let obj = KArc::new(1u32, &ctx).unwrap();
        let table = FakeTable::new();
        let copied = Cell::new(false);
        let r = install_after(
            |p| {
                let fd = request(p, &table, obj.clone())?;
                assert_eq!(table.state(fd), "reserved");
                Err(Errno::EIO)
            },
            || {
                copied.set(true);
                Ok(())
            },
        );
        assert_eq!(r, Err(Errno::EIO));
        assert!(!copied.get(), "a failed handler's argument is not copied out");
        assert!(table.all_free());
        assert_eq!(KArc::strong_count(&obj), 1);
    }

    #[test]
    fn two_requests_in_one_ioctl() {
        let ctx = sleep();
        let obj = KArc::new(2u32, &ctx).unwrap();
        for fault in [false, true] {
            let table = FakeTable::new();
            let (k1, k2, u1, u2) = (Cell::new(-1), Cell::new(-1), Cell::new(-1), Cell::new(-1));
            let r = install_after(
                |p| {
                    k1.set(request(p, &table, obj.clone())?);
                    k2.set(request(p, &table, obj.clone())?);
                    assert_ne!(k1.get(), k2.get(), "two numbers");
                    // SAFETY: as in request.
                    assert_eq!(unsafe { p.as_ref() }.len(), 2);
                    Ok(())
                },
                || copy_out(&k1, &u1, false).and_then(|()| copy_out(&k2, &u2, fault)),
            );
            if fault {
                assert_eq!(r, Err(Errno::EFAULT));
                assert!(table.all_free(), "both given up");
                assert_eq!(KArc::strong_count(&obj), 1);
            } else {
                assert_eq!(r, Ok(()));
                assert_eq!((u1.get(), u2.get()), (k1.get(), k2.get()));
                assert_eq!((table.state(u1.get()), table.state(u2.get())), ("open", "open"));
                assert_eq!(KArc::strong_count(&obj), 3);
                drop((table.close(u1.get()), table.close(u2.get())));
                assert_eq!(KArc::strong_count(&obj), 1);
            }
        }
    }

    #[test]
    fn requests_beyond_the_bound_fail_without_opening() {
        let ctx = sleep();
        let obj = KArc::new(3u32, &ctx).unwrap();
        let table = FakeTable::new();
        let opened = Cell::new(0);
        let r = install_after(
            |p| {
                for _ in 0..MAX_PENDING_FDS {
                    request(p, &table, obj.clone())?;
                }
                // SAFETY: as in request.
                let more = defer_fd(Some(unsafe { &mut *p.as_ptr() }), || {
                    opened.set(opened.get() + 1);
                    Ok(table.reserve(obj.clone()))
                });
                assert_eq!(more, Err(Errno::EMFILE));
                assert_eq!(opened.get(), 0, "no file opened for the refused request");
                more.map(|_| ())
            },
            || Ok(()),
        );
        assert_eq!(r, Err(Errno::EMFILE));
        assert!(table.all_free());
        assert_eq!(KArc::strong_count(&obj), 1);
    }

    #[test]
    fn request_outside_an_ioctl_fails_without_opening() {
        let table: FakeTable<u32> = FakeTable::new();
        let opened = Cell::new(false);
        let r = defer_fd::<FakeFd<'_, u32>>(None, || {
            opened.set(true);
            Ok(table.reserve(0))
        });
        assert_eq!(r, Err(Errno::EINVAL));
        assert!(!opened.get());
        assert!(table.all_free());
    }
}
