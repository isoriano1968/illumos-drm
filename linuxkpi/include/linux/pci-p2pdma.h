/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's PCI peer-to-peer DMA. Not supported: no two devices are
 * reachable from each other, so amdgpu shares buffers between GPUs
 * through system memory.
 */
#ifndef _LINUX_PCI_P2PDMA_H
#define	_LINUX_PCI_P2PDMA_H
#include <linux/pci.h>
static inline int
pci_p2pdma_distance(struct pci_dev *provider, struct device *client, bool verbose)
{
	return (-1);
}
static inline int
pci_p2pdma_distance_many(struct pci_dev *provider, struct device **clients,
    int num_clients, bool verbose)
{
	return (-1);
}
#endif
