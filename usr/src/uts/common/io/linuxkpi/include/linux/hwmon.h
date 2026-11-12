/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's hardware monitoring class (amdgpu's temperature, fan and power
 * sensors in sysfs). Not provided on illumos yet: registration returns
 * -ENODEV, which amdgpu reports and ignores. (illumos's equivalent is the
 * ksensor framework; a bridge is future work.)
 */
#ifndef _LINUX_HWMON_H
#define	_LINUX_HWMON_H
#include <linux/types.h>
#include <linux/err.h>
#include <linux/sysfs.h>
struct device;
enum hwmon_sensor_types { hwmon_chip, hwmon_temp, hwmon_in, hwmon_curr,
	hwmon_power, hwmon_energy, hwmon_humidity, hwmon_fan, hwmon_pwm };
enum hwmon_temp_attributes {
	hwmon_temp_enable,
	hwmon_temp_input,
	hwmon_temp_type,
	hwmon_temp_lcrit,
	hwmon_temp_lcrit_hyst,
	hwmon_temp_min,
	hwmon_temp_min_hyst,
	hwmon_temp_max,
	hwmon_temp_max_hyst,
	hwmon_temp_crit,
	hwmon_temp_crit_hyst,
	hwmon_temp_emergency,
	hwmon_temp_emergency_hyst,
	hwmon_temp_alarm,
	hwmon_temp_lcrit_alarm,
	hwmon_temp_min_alarm,
	hwmon_temp_max_alarm,
	hwmon_temp_crit_alarm,
	hwmon_temp_emergency_alarm,
	hwmon_temp_fault,
	hwmon_temp_offset,
	hwmon_temp_label
};
static inline struct device *
hwmon_device_register_with_groups(struct device *dev, const char *name,
    void *drvdata, const struct attribute_group **groups)
{
	return (ERR_PTR(-ENODEV));
}
static inline void hwmon_device_unregister(struct device *dev) {}
#endif
