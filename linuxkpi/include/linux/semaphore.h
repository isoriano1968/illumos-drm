/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's counting semaphores (amdgpu's task barrier for multi-GPU XGMI
 * resets): an implementation on an illumos mutex and condition variable.
 * down() sleeps uninterruptibly; down_trylock() returns 0 on success, as
 * Linux's does. The structure has no destructor in Linux; the mutex and
 * condition variable are only initialised (MUTEX_DRIVER/CV_DRIVER need no
 * destruction for memory that is freed).
 */
#ifndef _LINUX_SEMAPHORE_H
#define	_LINUX_SEMAPHORE_H
#include <linux/types.h>
#include <sys/ksynch.h>	/* illumos's own sema_init(), before the rename */

#define	sema_init(s, v)		lkpi_sema_init(s, v)
#define	down(s)			lkpi_down(s)
#define	down_trylock(s)		lkpi_down_trylock(s)
#define	up(s)			lkpi_up(s)

struct semaphore {
	kmutex_t lkpi_lock;
	kcondvar_t lkpi_cv;
	int count;
};

static inline void
lkpi_sema_init(struct semaphore *s, int val)
{
	(mutex_init)(&s->lkpi_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&s->lkpi_cv, NULL, CV_DRIVER, NULL);
	s->count = val;
}

static inline void
lkpi_down(struct semaphore *s)
{
	(mutex_enter)(&s->lkpi_lock);
	while (s->count <= 0)
		cv_wait(&s->lkpi_cv, &s->lkpi_lock);
	s->count--;
	(mutex_exit)(&s->lkpi_lock);
}

static inline int
lkpi_down_trylock(struct semaphore *s)
{
	int busy;

	(mutex_enter)(&s->lkpi_lock);
	busy = (s->count <= 0);
	if (!busy)
		s->count--;
	(mutex_exit)(&s->lkpi_lock);
	return (busy);
}

static inline void
lkpi_up(struct semaphore *s)
{
	(mutex_enter)(&s->lkpi_lock);
	s->count++;
	cv_signal(&s->lkpi_cv);
	(mutex_exit)(&s->lkpi_lock);
}
#endif
