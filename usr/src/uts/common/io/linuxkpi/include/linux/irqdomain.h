/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's IRQ domains. amdgpu uses one as a child interrupt controller
 * for sources another driver handles (ACP, ISP: amdgpu_irq_create_mapping).
 * The IH blocks of Volcanic Islands and later (tonga_ih, cz_ih,
 * iceland_ih, ih_v6_1) create the domain in early_init and fail the
 * attach with -ENODEV without it (seen on Polaris 11, 2026-11-17); Southern
 * Islands' si_ih never creates one.
 *
 * Here a domain is only a handle: it is allocated and freed, but no
 * virtual interrupt is ever mapped (irq_create_mapping returns 0, so
 * adev->irq.virq[] stays 0 and amdgpu_irq_dispatch never forwards to
 * generic_handle_domain_irq). The GPUs this port covers have no ACP or
 * ISP child driver on illumos.
 */
#ifndef _LINUX_IRQDOMAIN_H
#define	_LINUX_IRQDOMAIN_H
#include <linux/types.h>
#include <linux/irq.h>
#include <linux/slab.h>

typedef unsigned long irq_hw_number_t;
struct irq_domain;
struct fwnode_handle;

struct irq_domain_ops {
	int (*map)(struct irq_domain *d, unsigned int virq, irq_hw_number_t hw);
	void (*unmap)(struct irq_domain *d, unsigned int virq);
};

struct irq_domain {
	const struct irq_domain_ops	*ops;
	void				*host_data;
	unsigned int			size;
};

static inline struct irq_domain *
irq_domain_create_linear(struct fwnode_handle *fwnode, unsigned int size,
    const struct irq_domain_ops *ops, void *host_data)
{
	struct irq_domain *d = kzalloc(sizeof (*d), GFP_KERNEL);

	if (d != NULL) {
		d->ops = ops;
		d->host_data = host_data;
		d->size = size;
	}
	return (d);
}
static inline void
irq_domain_remove(struct irq_domain *d)
{
	kfree(d);
}
static inline unsigned int
irq_create_mapping(struct irq_domain *d, irq_hw_number_t hw)
{
	return (0);
}
static inline unsigned int
irq_find_mapping(struct irq_domain *d, irq_hw_number_t hw)
{
	return (0);
}
static inline void irq_dispose_mapping(unsigned int virq) {}
static inline int
generic_handle_domain_irq(struct irq_domain *d, irq_hw_number_t hw)
{
	return (-EINVAL);
}
#endif
