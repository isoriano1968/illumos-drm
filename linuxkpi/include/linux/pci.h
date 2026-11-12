/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's PCI interface over the illumos DDI (include/linux/pci.h,
 * GPL-2.0; independent implementation in src/lkpi_pci.c).
 *
 * A struct pci_dev is made by the layer's attach glue for the driver's
 * own node: identity from the "vendor-id"/"device-id"/... properties,
 * config space through pci_config_setup(9F), BARs from "reg" and
 * "assigned-addresses" (pci_iomap() maps a BAR with
 * ddi_regs_map_setup(9F)), interrupts through ddi_intr_*(9F)
 * (<linux/interrupt.h>). Other functions' config space
 * (pci_get_domain_bus_and_slot(), e.g. the host bridge) is reached by
 * finding their node in the device tree; such devices are read-only
 * wrappers without BARs or interrupts (a host bridge accepts writes to
 * the MCHBAR and DEVEN registers i915 programs). While a layer device
 * attaches or detaches, only the siblings of its node are searched
 * (src/lkpi_pci.c). Devices are reference counted (pci_dev_get(),
 * pci_dev_put(), get_device(), put_device()); pci_get_*() return a
 * reference. Config accesses are limited to the function's config space
 * (cfg_size: 4096 bytes for PCI Express, else 256). Power states and
 * save/restore of config space are not supported (always D0; 0
 * returned).
 */
#ifndef _LINUX_PCI_H
#define	_LINUX_PCI_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/pci_ids.h>
#include <linux/pci_regs.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#define	PCI_NUM_RESOURCES	6
#define	PCI_STD_NUM_BARS	6
#define	PCI_ROM_RESOURCE	6
#define	PCI_DEVFN(slot, func)	((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define	PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define	PCI_FUNC(devfn)		((devfn) & 0x07)
#define	PCI_ANY_ID		(~0)
#define	PCI_D0			0
#define	PCI_D1			1
#define	PCI_D2			2
#define	PCI_D3hot		3
#define	PCI_D3cold		4
#define	PCI_UNKNOWN		5
#define	PCI_POWER_ERROR		(-1)
#define	PCI_IRQ_INTX		(1 << 0)
#define	PCI_IRQ_LEGACY		PCI_IRQ_INTX
#define	PCI_IRQ_MSI		(1 << 1)
#define	PCI_IRQ_MSIX		(1 << 2)
#define	PCI_IRQ_AFFINITY	(1 << 3)
#define	PCI_IRQ_ALL_TYPES	(PCI_IRQ_INTX | PCI_IRQ_MSI | PCI_IRQ_MSIX)

typedef unsigned int pci_channel_state_t;
typedef unsigned short pci_dev_flags_t;

/* A config read of a device that is gone returns all ones. */
#define	PCI_ERROR_RESPONSE	(~0ULL)
#define	PCI_POSSIBLE_ERROR(v)	((v) == ((typeof(v))PCI_ERROR_RESPONSE))

/*
 * PCI Express link widths and speeds, numbered as Linux numbers them
 * (the widths are the Link Capabilities register's encoding).
 */
enum pcie_link_width {
	PCIE_LNK_WIDTH_RESRV	= 0x00,
	PCIE_LNK_X1		= 0x01,
	PCIE_LNK_X2		= 0x02,
	PCIE_LNK_X4		= 0x04,
	PCIE_LNK_X8		= 0x08,
	PCIE_LNK_X12		= 0x0c,
	PCIE_LNK_X16		= 0x10,
	PCIE_LNK_X32		= 0x20,
	PCIE_LNK_WIDTH_UNKNOWN	= 0xff
};

enum pci_bus_speed {
	PCI_SPEED_33MHz		= 0x00,
	PCI_SPEED_66MHz		= 0x01,
	PCIE_SPEED_2_5GT	= 0x14,
	PCIE_SPEED_5_0GT	= 0x15,
	PCIE_SPEED_8_0GT	= 0x16,
	PCIE_SPEED_16_0GT	= 0x17,
	PCIE_SPEED_32_0GT	= 0x18,
	PCIE_SPEED_64_0GT	= 0x19,
	PCI_SPEED_UNKNOWN	= 0xff
};

/*
 * PCI error recovery (AER). illumos reports PCIe errors through FMA, not
 * to the driver: the layer never calls a driver's pci_error_handlers, so
 * the table is kept for the driver's own reset code only.
 */
typedef unsigned int pci_ers_result_t;
enum pci_ers_result {
	PCI_ERS_RESULT_NONE		= 1,
	PCI_ERS_RESULT_CAN_RECOVER	= 2,
	PCI_ERS_RESULT_NEED_RESET	= 3,
	PCI_ERS_RESULT_DISCONNECT	= 4,
	PCI_ERS_RESULT_RECOVERED	= 5,
	PCI_ERS_RESULT_NO_AER_DRIVER	= 6
};
enum {
	pci_channel_io_normal		= 1,
	pci_channel_io_frozen		= 2,
	pci_channel_io_perm_failure	= 3
};

struct pci_dev;
struct pci_error_handlers {
	pci_ers_result_t (*error_detected)(struct pci_dev *dev,
	    pci_channel_state_t error);
	pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);
	pci_ers_result_t (*slot_reset)(struct pci_dev *dev);
	void (*reset_prepare)(struct pci_dev *dev);
	void (*reset_done)(struct pci_dev *dev);
	void (*resume)(struct pci_dev *dev);
	void (*cor_error_detected)(struct pci_dev *dev);
};

/* Saved config space: not supported (pci_store_saved_state() is NULL). */
struct pci_saved_state;

struct pci_bus {
	struct pci_bus *parent;
	struct pci_dev *self;
	unsigned char number;
	unsigned char primary;
	int domain_nr;
};

struct lkpi_pci_irq;

struct pci_dev {
	struct device dev;
	struct pci_bus *bus;
	struct pci_bus lkpi_bus;
	unsigned int devfn;
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned int class;
	u8 revision;
	u8 hdr_type;
	u8 pcie_cap;
	u8 msi_cap;
	u8 msix_cap;
	int cfg_size;
	int irq;
	unsigned int msi_enabled:1;
	unsigned int msix_enabled:1;
	unsigned int is_virtfn:1;
	unsigned int no_64bit_msi:1;
	struct resource resource[PCI_NUM_RESOURCES + 1];
	struct pci_driver *driver;
	u64 dma_mask;
	pci_dev_flags_t dev_flags;
	/*
	 * A copy of the expansion ROM that the platform firmware left in
	 * memory (Linux: EFI's PCI I/O protocol ROM images): not known,
	 * always 0 (drivers then read the ROM BAR or the ACPI tables).
	 */
	phys_addr_t rom;
	size_t romlen;
	u32 saved_config_space[16];	/* never filled: see pci_save_state() */
	/* illumos */
	ddi_acc_handle_t lkpi_cfg;
	int lkpi_regnum[PCI_NUM_RESOURCES + 1];
	bool lkpi_foreign;
	struct lkpi_pci_irq *lkpi_irq;
};

struct pci_driver {
	const char *name;
	const struct pci_device_id *id_table;
	int (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove)(struct pci_dev *dev);
	void (*shutdown)(struct pci_dev *dev);
	int (*suspend)(struct pci_dev *dev, pm_message_t state);
	int (*resume)(struct pci_dev *dev);
	const struct pci_error_handlers *err_handler;
	const struct attribute_group **dev_groups;	/* sysfs: unused */
	struct device_driver driver;
};

#define	to_pci_dev(d)		container_of((d), struct pci_dev, dev)
#define	pci_get_drvdata(p)	dev_get_drvdata(&(p)->dev)
#define	pci_set_drvdata(p, v)	dev_set_drvdata(&(p)->dev, (v))
#define	pci_name(p)		dev_name(&(p)->dev)
#define	pci_domain_nr(bus)	((bus)->domain_nr)
#define	pci_dev_id(p)		((u16)(((p)->bus->number << 8) | (p)->devfn))
#define	pci_resource_start(p, bar)	((p)->resource[(bar)].start)
#define	pci_resource_end(p, bar)	((p)->resource[(bar)].end)
#define	pci_resource_flags(p, bar)	((p)->resource[(bar)].flags)
#define	pci_resource_len(p, bar) \
	((p)->resource[(bar)].end == 0 ? 0 : resource_size(&(p)->resource[(bar)]))
#define	pci_resource_n(p, bar)	(&(p)->resource[(bar)])
#define	pci_is_root_bus(b)	((b)->parent == NULL)
#define	pci_is_pcie(p)		((p)->pcie_cap != 0)
#define	pci_pcie_cap(p)		((p)->pcie_cap)
#define	pci_dev_put(p)		lkpi_pci_dev_put(p)
#define	pci_dev_get(p)		lkpi_pci_dev_get(p)
#define	pci_dev_present(ids)	0
#define	pci_is_enabled(p)	true
#define	pci_dev_run_wake(p)	false
#define	pci_ignore_hotplug(p)	((void)(p))
#define	pci_d3cold_disable(p)	((void)(p))
#define	pci_d3cold_enable(p)	((void)(p))
#define	pci_dev_is_disconnected(p)	false

extern int pci_read_config_byte(const struct pci_dev *dev, int where, u8 *val);
extern int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val);
extern int pci_read_config_dword(const struct pci_dev *dev, int where, u32 *val);
extern int pci_write_config_byte(const struct pci_dev *dev, int where, u8 val);
extern int pci_write_config_word(const struct pci_dev *dev, int where, u16 val);
extern int pci_write_config_dword(const struct pci_dev *dev, int where, u32 val);
extern int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
    int where, u8 *val);
extern int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
    int where, u16 *val);
extern int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 *val);
extern int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
    int where, u8 val);
extern int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn,
    int where, u16 val);
extern int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 val);
extern u8 pci_find_capability(struct pci_dev *dev, int cap);
extern u16 pci_find_ext_capability(struct pci_dev *dev, int cap);
extern int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val);
extern int pcie_capability_read_dword(struct pci_dev *dev, int pos, u32 *val);
extern int pcie_capability_write_word(struct pci_dev *dev, int pos, u16 val);
extern int pcie_capability_clear_and_set_word(struct pci_dev *dev, int pos,
    u16 clear, u16 set);
extern int pci_enable_device(struct pci_dev *dev);
extern void pci_disable_device(struct pci_dev *dev);
extern void pci_set_master(struct pci_dev *dev);
extern void pci_clear_master(struct pci_dev *dev);
extern int pci_set_power_state(struct pci_dev *dev, pci_power_t state);
extern int pci_save_state(struct pci_dev *dev);
extern void pci_restore_state(struct pci_dev *dev);
extern int pci_request_regions(struct pci_dev *dev, const char *name);
extern void pci_release_regions(struct pci_dev *dev);
extern void __iomem *pci_iomap(struct pci_dev *dev, int bar,
    unsigned long maxlen);
extern void __iomem *pci_iomap_range(struct pci_dev *dev, int bar,
    unsigned long offset, unsigned long maxlen);
extern void __iomem *pci_iomap_wc(struct pci_dev *dev, int bar,
    unsigned long maxlen);
extern void pci_iounmap(struct pci_dev *dev, void __iomem *addr);
extern struct pci_dev *pci_get_domain_bus_and_slot(int domain,
    unsigned int bus, unsigned int devfn);
extern struct pci_dev *pci_get_device(unsigned int vendor,
    unsigned int device, struct pci_dev *from);
extern struct pci_dev *pci_get_class(unsigned int cls, struct pci_dev *from);
extern struct pci_dev *lkpi_pci_dev_get(struct pci_dev *dev);
extern void lkpi_pci_dev_put(struct pci_dev *dev);
extern int pci_register_driver(struct pci_driver *drv);
extern void pci_unregister_driver(struct pci_driver *drv);
extern const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
    struct pci_dev *dev);
extern int pci_enable_msi(struct pci_dev *dev);
extern void pci_disable_msi(struct pci_dev *dev);
extern int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
    unsigned int max_vecs, unsigned int flags);
extern void pci_free_irq_vectors(struct pci_dev *dev);
extern int pci_irq_vector(struct pci_dev *dev, unsigned int nr);
extern int pci_msix_vec_count(struct pci_dev *dev);
extern int pci_msi_vec_count(struct pci_dev *dev);
extern struct resource *pci_find_resource(struct pci_dev *dev,
    struct resource *res);
extern int pci_bus_alloc_resource(struct pci_bus *bus, struct resource *res,
    resource_size_t size, resource_size_t align, resource_size_t min,
    unsigned long type_mask,
    resource_size_t (*alignf)(void *, const struct resource *,
    resource_size_t, resource_size_t), void *alignf_data);
extern void pci_assign_unassigned_bus_resources(struct pci_bus *bus);
extern resource_size_t pcibios_align_resource(void *data,
    const struct resource *res, resource_size_t size, resource_size_t align);
#define	PCIBIOS_MIN_MEM		0x10000000UL
#define	PCIBIOS_MIN_IO		0x1000UL
extern int pci_resize_resource(struct pci_dev *dev, int i, int size,
    int exclude_bars);
extern u64 pci_rebar_get_possible_sizes(struct pci_dev *pdev, int bar);
extern int pci_rebar_get_max_size(struct pci_dev *pdev, int bar);

/*
 * PCI Express, from the device's own capability registers: the port type,
 * the link's speed and width capabilities and its ASPM control, and
 * waiting for outstanding transactions. The upstream path is not known
 * (pci_upstream_bridge() is NULL), so pcie_bandwidth_available() reports
 * the device's own link, AtomicOps to the root port are refused
 * (-EINVAL), and a function reset is not offered (-ENOTTY).
 */
extern int pci_pcie_type(const struct pci_dev *dev);
extern enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *dev);
extern enum pcie_link_width pcie_get_width_cap(struct pci_dev *dev);
extern u32 pcie_bandwidth_available(struct pci_dev *dev,
    struct pci_dev **limiting_dev, enum pci_bus_speed *speed,
    enum pcie_link_width *width);
extern bool pcie_aspm_enabled(struct pci_dev *dev);
extern int pci_wait_for_pending_transaction(struct pci_dev *dev);
extern int pci_reset_function(struct pci_dev *dev);
extern int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask);
extern struct pci_saved_state *pci_store_saved_state(struct pci_dev *dev);
extern int pci_load_saved_state(struct pci_dev *dev,
    struct pci_saved_state *state);
extern int pci_load_and_free_saved_state(struct pci_dev *dev,
    struct pci_saved_state **state);
#define	pci_pr3_present(p)		((void)(p), false)
#define	pcie_capability_set_word(d, pos, set) \
	pcie_capability_clear_and_set_word((d), (pos), 0, (set))
#define	pcie_capability_clear_word(d, pos, clear) \
	pcie_capability_clear_and_set_word((d), (pos), (clear), 0)

/*
 * Device Control's Max_Read_Request_Size and Max_Payload_Size (bytes),
 * read and (read request only) written directly; the device being
 * present (its vendor ID reads back). The layer leaves MSI programming
 * to the illumos interrupt framework and never resets a device, so
 * there is no MSI state to restore; no device is armed for wake-up.
 */
extern int pcie_get_readrq(struct pci_dev *dev);
extern int pcie_set_readrq(struct pci_dev *dev, int rq);
extern int pcie_get_mps(struct pci_dev *dev);
extern bool pci_device_is_present(struct pci_dev *dev);
#define	pci_restore_msi_state(p)	((void)(p))
static inline int pci_wake_from_d3(struct pci_dev *p, bool e) { return -EINVAL; }

/*
 * The layer knows no host-bridge windows: every bus has none, so a
 * driver that looks for a 64-bit window before resizing a BAR (amdgpu)
 * finds none and keeps the BAR the firmware sized.
 */
#define	pci_bus_for_each_resource(bus, res, ...) \
	for ((void)(bus), (res) = NULL __VA_OPT__(, (__VA_ARGS__) = 0); \
	    (res) != NULL; )

#define	pcim_enable_device(p)		pci_enable_device(p)
#define	pcim_iomap(p, bar, len)		pci_iomap(p, bar, len)
#define	pcim_iounmap(p, a)		pci_iounmap(p, a)
#define	pci_rebar_bytes_to_size(b)	0
#define	pci_rebar_size_to_bytes(s)	((u64)1 << ((s) + 20))
#define	pci_platform_rom(p, s)		NULL
#define	pci_map_rom(p, s)		((void __iomem *)NULL)
#define	pci_unmap_rom(p, r)		((void)(r))
#define	pci_upstream_bridge(p)		((struct pci_dev *)NULL)
#define	pcie_find_root_port(p)		((struct pci_dev *)NULL)
#define	pci_physfn(p)			(p)
#define	pci_num_vf(p)			0
#define	pci_sriov_get_totalvfs(p)	0
#define	pci_vfs_assigned(p)		0

/*
 * The illumos side of a Linux PCI driver: the driver's attach(9E) and
 * detach(9E) call these with its struct pci_driver. Attach builds the
 * struct pci_dev of the node, matches the ID table and calls probe();
 * detach calls remove() and releases the device's managed resources.
 * DDI_SUSPEND/DDI_RESUME are refused (DDI_FAILURE).
 */
extern int lkpi_pci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd,
    struct pci_driver *drv);
extern int lkpi_pci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd,
    struct pci_driver *drv);
extern struct pci_dev *lkpi_pci_dev_of(dev_info_t *dip);

/*
 * The driver registered with pci_register_driver() under the Linux name
 * (drv->name), or NULL: the illumos glue of a driver whose struct
 * pci_driver is static (i915) finds it here for lkpi_pci_attach().
 */
extern struct pci_driver *lkpi_pci_driver_lookup(const char *name);

#define	module_pci_driver(drv) \
	int lkpi_module_init_pci(void) { return (pci_register_driver(&(drv))); } \
	void lkpi_module_exit_pci(void) { pci_unregister_driver(&(drv)); }

#endif
