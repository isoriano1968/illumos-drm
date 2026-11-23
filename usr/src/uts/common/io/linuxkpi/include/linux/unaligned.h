/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's unaligned accessors (include/linux/unaligned.h, GPL-2.0;
 * reimplemented with memcpy, which gcc turns into plain moves on x86).
 */
#ifndef _LINUX_UNALIGNED_H
#define	_LINUX_UNALIGNED_H

#include <linux/types.h>
#include <lkpi/byteorder.h>

#define	__lkpi_get_unaligned(T, p) ({ T __v; \
	__builtin_memcpy(&__v, (p), sizeof (__v)); __v; })
#define	__lkpi_put_unaligned(T, v, p) do { T __v = (v); \
	__builtin_memcpy((p), &__v, sizeof (__v)); } while (0)
#define	get_unaligned(p)	__lkpi_get_unaligned(typeof (*(p)), (p))
#define	put_unaligned(v, p)	__lkpi_put_unaligned(typeof (*(p)), (v), (p))
/* Through the byte-order conversions, so for either CPU byte order. */
#define	get_unaligned_le16(p)	le16_to_cpu(__lkpi_get_unaligned(u16, (p)))
#define	get_unaligned_le32(p)	le32_to_cpu(__lkpi_get_unaligned(u32, (p)))
#define	get_unaligned_le64(p)	le64_to_cpu(__lkpi_get_unaligned(u64, (p)))
#define	get_unaligned_be16(p)	be16_to_cpu(__lkpi_get_unaligned(u16, (p)))
#define	get_unaligned_be32(p)	be32_to_cpu(__lkpi_get_unaligned(u32, (p)))
#define	get_unaligned_be64(p)	be64_to_cpu(__lkpi_get_unaligned(u64, (p)))
#define	put_unaligned_le16(v, p)	__lkpi_put_unaligned(u16, cpu_to_le16(v), (p))
#define	put_unaligned_le32(v, p)	__lkpi_put_unaligned(u32, cpu_to_le32(v), (p))
#define	put_unaligned_le64(v, p)	__lkpi_put_unaligned(u64, cpu_to_le64(v), (p))
#define	put_unaligned_be16(v, p)	__lkpi_put_unaligned(u16, cpu_to_be16(v), (p))
#define	put_unaligned_be32(v, p)	__lkpi_put_unaligned(u32, cpu_to_be32(v), (p))
#define	put_unaligned_be64(v, p)	__lkpi_put_unaligned(u64, cpu_to_be64(v), (p))

#endif
