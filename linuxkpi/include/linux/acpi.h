/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's ACPI interface. CONFIG_ACPI is off: no ACPI handles, no
 * methods, acpi_disabled is true. Only ACPI table lookup is supported:
 * acpi_get_table()/acpi_put_table() on illumos ACPICA (misc/acpica,
 * src/lkpi_acpi.c), which amdgpu reads its VBIOS from (the VFCT table).
 */
#ifndef _LINUX_ACPI_H
#define	_LINUX_ACPI_H
#include <linux/types.h>

/* ACPICA's boolean constants (Linux: <acpi/actypes.h>), which amdgpu uses. */
#ifndef	TRUE
#define	TRUE	(1 == 1)
#endif
#ifndef	FALSE
#define	FALSE	(1 == 0)
#endif
#define	acpi_disabled		1
#define	ACPI_HANDLE(dev)	((void *)NULL)
typedef void *acpi_handle;

/* ACPICA's status and size types (Linux: <acpi/actypes.h>, <acpi/acexcep.h>). */
typedef u32 acpi_status;
typedef u64 acpi_size;
#define	AE_OK			((acpi_status)0x0000)
#define	ACPI_SUCCESS(a)		(!(a))
#define	ACPI_FAILURE(a)		(a)

/*
 * The common header of every ACPI table (ACPI 6.5, 5.2.6; Linux:
 * <acpi/actbl.h>), the layout of ACPICA's ACPI_TABLE_HEADER.
 */
#define	ACPI_NAMESEG_SIZE	4
#define	ACPI_OEM_ID_SIZE	6
#define	ACPI_OEM_TABLE_ID_SIZE	8
struct acpi_table_header {
	char	signature[ACPI_NAMESEG_SIZE];
	u32	length;
	u8	revision;
	u8	checksum;
	char	oem_id[ACPI_OEM_ID_SIZE];
	char	oem_table_id[ACPI_OEM_TABLE_ID_SIZE];
	u32	oem_revision;
	char	asl_compiler_id[ACPI_NAMESEG_SIZE];
	u32	asl_compiler_revision;
};

/*
 * The instance'th (from 1) table with the signature, mapped until
 * acpi_put_table(). Not from interrupt context.
 */
extern acpi_status acpi_get_table(char *signature, u32 instance,
    struct acpi_table_header **out_table);
extern void acpi_put_table(struct acpi_table_header *table);
#endif
