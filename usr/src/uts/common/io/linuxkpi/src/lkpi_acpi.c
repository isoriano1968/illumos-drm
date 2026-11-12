/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * ACPI table lookup of <linux/acpi.h> on illumos ACPICA (misc/acpica;
 * the layer links with -N misc/acpica). ACPICA's headers are not
 * installed, so its two table functions are declared here as in
 * uts/intel/sys/acpi/acpixf.h (ACPI_STATUS is UINT32, ACPI_STRING
 * char *); struct acpi_table_header has ACPI_TABLE_HEADER's layout.
 */

#include <linux/acpi.h>
#include <linux/build_bug.h>
#include <linux/stddef.h>
#include <linux/export.h>

extern uint32_t AcpiGetTable(char *Signature, uint32_t Instance,
    struct acpi_table_header **OutTable);
extern void AcpiPutTable(struct acpi_table_header *Table);

/* ACPI_TABLE_HEADER is 36 bytes, packed by layout (no padding). */
static_assert(sizeof (struct acpi_table_header) == 36);
static_assert(offsetof(struct acpi_table_header, oem_revision) == 24);

acpi_status
acpi_get_table(char *signature, u32 instance,
    struct acpi_table_header **out_table)
{
	return (AcpiGetTable(signature, instance, out_table));
}
EXPORT_SYMBOL(acpi_get_table);

void
acpi_put_table(struct acpi_table_header *table)
{
	if (table != NULL)
		AcpiPutTable(table);
}
EXPORT_SYMBOL(acpi_put_table);
