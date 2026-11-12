/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's KUnit hooks in non-test code: no test is ever running.
 */
#ifndef _KUNIT_TEST_BUG_H
#define	_KUNIT_TEST_BUG_H
#define	kunit_get_current_test()	NULL
#define	kunit_fail_current_test(fmt, ...)	((void)0)
#endif
