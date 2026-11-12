/* SPDX-License-Identifier: MIT */
/* Copyright 2026 The illumos-drm authors. Linux's <linux/time.h>: the layer's time64 and ktime. */
#ifndef _LINUX_TIME_H
#define	_LINUX_TIME_H
#include <linux/time64.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>

/* Broken-down UTC time (Linux's struct tm: tm_year from 1900, tm_mon 0-11). */
struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	long tm_year;
	int tm_wday;
	int tm_yday;
};
extern void time64_to_tm(time64_t totalsecs, int offset, struct tm *result);
#endif
