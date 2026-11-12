/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's IOMMU API. illumos has no IOMMU domains for drivers (the DDI's
 * DMA handles hide any remapping), so no device has a domain: amdgpu then
 * treats system memory as directly mapped, as Linux with the IOMMU off or
 * in passthrough.
 */
#ifndef _LINUX_IOMMU_H
#define	_LINUX_IOMMU_H
#include <linux/types.h>
struct device;
struct iommu_domain {
	unsigned int type;
};
#define	IOMMU_DOMAIN_IDENTITY	0x1
#define	IOMMU_DOMAIN_DMA	0x2
static inline struct iommu_domain *
iommu_get_domain_for_dev(struct device *dev)
{
	return (NULL);
}
static inline phys_addr_t
iommu_iova_to_phys(struct iommu_domain *d, dma_addr_t iova)
{
	return ((phys_addr_t)iova);
}
#endif
