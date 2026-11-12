/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's kfifo: a byte FIFO of a power-of-two size with element-typed
 * macros (amdgpu: VM fault and RAS event queues). An illumos
 * implementation of the interface, not Linux's code: struct kfifo holds a
 * buffer, its size in bytes, the element size and free-running in/out
 * counters; the macros copy whole elements. One reader and one writer
 * need no lock; the *_spinlocked variants take the given spinlock.
 */
#ifndef _LINUX_KFIFO_H
#define	_LINUX_KFIFO_H
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/log2.h>
#include <linux/string.h>
#include <linux/minmax.h>

struct __kfifo {
	unsigned char *data;
	unsigned int mask;	/* size in bytes - 1 */
	unsigned int esize;	/* element size */
	unsigned int in, out;	/* free-running byte counters */
};

/*
 * As in Linux, every fifo has a "kfifo" member (the state) and a "type"
 * pointer member whose target type is the element type; struct kfifo is
 * a fifo of bytes.
 */
#define	DECLARE_KFIFO_PTR(name, eltype) \
	struct { union { struct __kfifo kfifo; eltype *type; }; } name
#define	DECLARE_KFIFO(name, eltype, size) \
	struct { union { struct __kfifo kfifo; eltype *type; }; \
	    eltype buf[((size) < 2) || ((size) & ((size) - 1)) ? -1 : (size)]; } name
#define	INIT_KFIFO(fifo) \
	lkpi_kfifo_init_buf(&(fifo).kfifo, (fifo).buf, sizeof ((fifo).buf), \
	    sizeof (*(fifo).type))
struct kfifo {
	union {
		struct __kfifo kfifo;
		unsigned char *type;
	};
};
#define	__kfifo_of(fifo)	(&(fifo)->kfifo)

static inline void
lkpi_kfifo_init_buf(struct __kfifo *f, void *buf, size_t size, size_t esize)
{
	f->data = buf;
	f->mask = (unsigned int)rounddown_pow_of_two(size) - 1;
	f->esize = (unsigned int)esize;
	f->in = f->out = 0;
}

static inline int
lkpi_kfifo_alloc(struct __kfifo *f, unsigned int nelem, size_t esize, gfp_t gfp)
{
	size_t size = (size_t)roundup_pow_of_two(nelem ? nelem : 1) * esize;

	f->data = kmalloc(size, gfp);
	if (f->data == NULL) {
		f->mask = 0;
		return (-ENOMEM);
	}
	f->mask = (unsigned int)size - 1;
	if (!is_power_of_2(size))	/* esize not a power of two */
		f->mask = (unsigned int)rounddown_pow_of_two(size) - 1;
	f->esize = (unsigned int)esize;
	f->in = f->out = 0;
	return (0);
}

static inline void
lkpi_kfifo_free(struct __kfifo *f)
{
	kfree(f->data);
	f->data = NULL;
	f->mask = f->in = f->out = 0;
}

static inline unsigned int
lkpi_kfifo_len(const struct __kfifo *f)
{
	return ((f->in - f->out) / (f->esize ? f->esize : 1));
}

static inline unsigned int
lkpi_kfifo_avail(const struct __kfifo *f)
{
	return ((f->mask + 1 - (f->in - f->out)) / (f->esize ? f->esize : 1));
}

static inline void
lkpi_kfifo_copy(struct __kfifo *f, unsigned int off, void *dst, const void *src,
    unsigned int len, bool to_fifo)
{
	unsigned int size = f->mask + 1, o = off & f->mask;
	unsigned int l = min(len, size - o);

	if (to_fifo) {
		memcpy(f->data + o, src, l);
		memcpy(f->data, (const unsigned char *)src + l, len - l);
	} else {
		memcpy(dst, f->data + o, l);
		memcpy((unsigned char *)dst + l, f->data, len - l);
	}
}

/* Copies n elements in; returns the number copied. */
static inline unsigned int
lkpi_kfifo_in(struct __kfifo *f, const void *buf, unsigned int n)
{
	n = min(n, lkpi_kfifo_avail(f));
	lkpi_kfifo_copy(f, f->in, NULL, buf, n * f->esize, true);
	f->in += n * f->esize;
	return (n);
}

static inline unsigned int
lkpi_kfifo_out(struct __kfifo *f, void *buf, unsigned int n, bool peek)
{
	n = min(n, lkpi_kfifo_len(f));
	lkpi_kfifo_copy(f, f->out, buf, NULL, n * f->esize, false);
	if (!peek)
		f->out += n * f->esize;
	return (n);
}

#define	kfifo_alloc(fifo, size, gfp) \
	lkpi_kfifo_alloc(__kfifo_of(fifo), (size), sizeof (*(fifo)->type), (gfp))
#define	kfifo_free(fifo)	lkpi_kfifo_free(__kfifo_of(fifo))
#define	kfifo_reset(fifo)	((void)(__kfifo_of(fifo)->in = __kfifo_of(fifo)->out = 0))
#define	kfifo_len(fifo)		lkpi_kfifo_len(__kfifo_of(fifo))
#define	kfifo_avail(fifo)	lkpi_kfifo_avail(__kfifo_of(fifo))
#define	kfifo_is_empty(fifo)	(kfifo_len(fifo) == 0)
#define	kfifo_is_full(fifo)	(kfifo_avail(fifo) == 0)
#define	kfifo_in(fifo, buf, n)	lkpi_kfifo_in(__kfifo_of(fifo), (buf), (n))
#define	kfifo_out(fifo, buf, n)	lkpi_kfifo_out(__kfifo_of(fifo), (buf), (n), false)
#define	kfifo_out_peek(fifo, buf, n) \
	lkpi_kfifo_out(__kfifo_of(fifo), (buf), (n), true)
#define	kfifo_put(fifo, val) ({ \
	__typeof__(*(fifo)->type) _kv = (val); \
	lkpi_kfifo_in(__kfifo_of(fifo), &_kv, 1); })
#define	kfifo_get(fifo, valp)	lkpi_kfifo_out(__kfifo_of(fifo), (valp), 1, false)
#define	kfifo_peek(fifo, valp)	lkpi_kfifo_out(__kfifo_of(fifo), (valp), 1, true)
#define	kfifo_in_spinlocked(fifo, buf, n, lock) ({ \
	unsigned long _kfl; unsigned int _kr; \
	spin_lock_irqsave((lock), _kfl); \
	_kr = kfifo_in((fifo), (buf), (n)); \
	spin_unlock_irqrestore((lock), _kfl); _kr; })
#define	kfifo_out_spinlocked(fifo, buf, n, lock) ({ \
	unsigned long _kfl; unsigned int _kr; \
	spin_lock_irqsave((lock), _kfl); \
	_kr = kfifo_out((fifo), (buf), (n)); \
	spin_unlock_irqrestore((lock), _kfl); _kr; })
#endif
