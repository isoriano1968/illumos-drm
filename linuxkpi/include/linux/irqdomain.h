/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's IRQ domains (amdgpu's child interrupt controller for ACP and
 * ISP, which Southern Islands and the other GPUs this port covers do not
 * have: si_ih.c never calls amdgpu_irq_add_domain()). Not supported:
 * irq_domain_create_linear() returns NULL, so amdgpu_irq_add_domain()
 * fails with -ENODEV for a GPU that needs it, and no virtual interrupt
 * exists.
 */
#ifndef _LINUX_IRQDOMAIN_H
#define	_LINUX_IRQDOMAIN_H
#include <linux/types.h>
#include <linux/irq.h>

typedef unsigned long irq_hw_number_t;
struct irq_domain;
struct fwnode_handle;

struct irq_domain_ops {
	int (*map)(struct irq_domain *d, unsigned int virq, irq_hw_number_t hw);
	void (*unmap)(struct irq_domain *d, unsigned int virq);
};

static inline struct irq_domain *
irq_domain_create_linear(struct fwnode_handle *fwnode, unsigned int size,
    const struct irq_domain_ops *ops, void *host_data)
{
	return (NULL);
}
static inline void irq_domain_remove(struct irq_domain *d) {}
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
