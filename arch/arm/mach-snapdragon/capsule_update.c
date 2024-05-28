// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm Snapdragon boards.
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#define pr_fmt(fmt) "QCOM-FMP: " fmt

#include <dm/device.h>
#include <dm/uclass.h>
#include <efi.h>
#include <efi_loader.h>
#include <malloc.h>
#include <scsi.h>
#include <part.h>
#include <linux/err.h>

#include "qcom-priv.h"

struct efi_fw_image fw_images[] = {
	{
		/* Filled in automatically */
		.image_type_id = { { 0 } },
		.fw_name = u"QUALCOMM-UBOOT",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	/* Filled in by configure_dfu_string() */
	.dfu_string = NULL,
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

/* LSB first */
struct part_slot_status {
	u16: 2;
	u16 active : 1;
	u16: 3;
	u16 successful : 1;
	u16 unbootable : 1;
	u16 tries_remaining : 4;
};

static int find_boot_partition(const char *partname, struct blk_desc *blk_dev, char *name)
{
	int ret;
	int partnum;
	struct disk_partition info;
	struct part_slot_status *slot_status;

	for (partnum = 1;; partnum++) {
		ret = part_get_info(blk_dev, partnum, &info);
		if (ret)
			return ret;

		slot_status = (struct part_slot_status *)&info.type_flags;
		log_io("%16s: Active: %1d, Successful: %1d, Unbootable: %1d, Tries left: %1d\n",
		       info.name, slot_status->active,
		       slot_status->successful, slot_status->unbootable,
		       slot_status->tries_remaining);
		if (!strncmp(info.name, partname, strlen(partname)) && slot_status->active) {
			log_debug("Found active %s partition: '%s'!\n", partname, info.name);
			strlcpy(name, info.name, sizeof(info.name));
			return partnum;
		}
	}

	return -1;
}

/**
 * qcom_configure_capsule_updates() - Configure the DFU string for capsule updates
 *
 * U-Boot is flashed to the boot partition on Qualcomm boards. In most cases there
 * are two boot partitions, boot_a and boot_b. As we don't currently support doing
 * full A/B updates, we only support updating the currently active boot partition.
 *
 * So we need to find the current slot suffix and the associated boot partition.
 * We do this by looking for the boot partition that has the 'active' flag set
 * in the GPT partition vendor attribute bits.
 */
void qcom_configure_capsule_updates(void)
{
	struct blk_desc *desc;
	int ret = 0, partnum = -1, devnum;
	static char dfu_string[32] = { 0 };
	char name[32]; /* GPT partition name */
	char *partname = "boot";
	struct udevice *dev = NULL;

	/*
	 * There is currently no good way to check how U-Boot is booting, but we have
	 * a few hueristics, like here checking if our DTB has a kaslr-seed specified
	 * will tell us if we were chainloaded by another bootloader.
	 * FIXME: we should do this check once and use some proper API to expose the data.
	 */
	if (!ofnode_has_property(ofnode_path("/chosen"), "kaslr-seed")) {
		log_debug("No initrd address present, skip as we might not be chainloaded\n");
		return;
	}

	if (IS_ENABLED(CONFIG_SCSI)) {
		/* Scan for SCSI devices */
		ret = scsi_scan(false);
		if (ret) {
			debug("Failed to scan SCSI devices: %d\n", ret);
			return;
		}
	}

	uclass_foreach_dev_probe(UCLASS_BLK, dev) {
		if (device_get_uclass_id(dev) != UCLASS_BLK)
			continue;

		desc = dev_get_uclass_plat(dev);
		if (!desc || desc->part_type == PART_TYPE_UNKNOWN)
			continue;
		devnum = desc->devnum;
		partnum = find_boot_partition(partname, desc,
					      name);
		if (partnum >= 0)
			break;
	}

	if (partnum < 0) {
		log_err("Failed to find boot partition\n");
		return;
	}

	switch (desc->uclass_id) {
	case UCLASS_SCSI:
		snprintf(dfu_string, 32, "scsi %d=u-boot-bin part %d", devnum, partnum);
		break;
	case UCLASS_MMC:
		snprintf(dfu_string, 32, "mmc 0=u-boot-bin part %d %d", devnum, partnum);
		break;
	default:
		debug("Unsupported storage uclass: %d\n", desc->uclass_id);
		return;
	}
	log_debug("boot partition is %s, DFU string: '%s'\n", name, dfu_string);

	update_info.dfu_string = dfu_string;
}
