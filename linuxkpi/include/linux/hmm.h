/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's heterogeneous memory management (mirroring user page tables for
 * userptr BOs and KFD SVM). Not supported (CONFIG_HMM_MIRROR off): the
 * structure exists so that amdgpu_hmm.h compiles; amdgpu's inline stubs
 * then refuse userptr.
 */
#ifndef _LINUX_HMM_H
#define	_LINUX_HMM_H
#include <linux/types.h>
struct mmu_interval_notifier;
struct hmm_range {
	struct mmu_interval_notifier *notifier;
	unsigned long notifier_seq;
	unsigned long start;
	unsigned long end;
	unsigned long *hmm_pfns;
	unsigned long default_flags;
	unsigned long pfn_flags_mask;
	void *dev_private_owner;
};

/*
 * HMM is not supported (no hmm_range_fault()), so no HMM pfn is ever
 * produced; a conversion would be a driver bug.
 */
#include <linux/bug.h>
struct page;
static inline struct page *
hmm_pfn_to_page(unsigned long hmm_pfn)
{
	(void) hmm_pfn;
	WARN_ON_ONCE(1);
	return (NULL);
}
#endif
