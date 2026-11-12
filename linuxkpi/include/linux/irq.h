/* SPDX-License-Identifier: MIT */
/* Copyright 2026 The illumos-drm authors. Linux's IRQ domain/chip interface: not supported (no child interrupt controllers). */
#ifndef _LINUX_IRQ_H
#define	_LINUX_IRQ_H
#include <linux/interrupt.h>
struct irq_data;
struct irq_data;
struct irq_desc;
struct irq_domain;

/*
 * An interrupt controller's callbacks (amdgpu's IH as a parent of the
 * ACP/ISP interrupt domains). The layer creates no irq domains
 * (<linux/irqdomain.h>), so no chip is ever installed.
 */
struct irq_chip {
	const char *name;
	void (*irq_mask)(struct irq_data *data);
	void (*irq_unmask)(struct irq_data *data);
};
typedef void (*irq_flow_handler_t)(struct irq_desc *desc);
static inline void
handle_simple_irq(struct irq_desc *desc)
{
	(void) desc;
}
#define	irq_set_chip_and_handler(irq, chip, h) \
	((void)(irq), (void)(chip), (void)(h))
#define	irq_set_status_flags(irq, f)	((void)(irq))
#define	irq_clear_status_flags(irq, f)	((void)(irq))
#define	IRQ_NOAUTOEN			0
#endif
