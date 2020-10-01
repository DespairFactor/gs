/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __S2MPU_LIB_H
#define __S2MPU_LIB_H

#define WHI_MAX_GB_GRANULES 64

#define S2MPU_TEST

struct s2pt {
	u32 l1entry_attrs[WHI_MAX_GB_GRANULES];
	u32 l2table_addrs[WHI_MAX_GB_GRANULES];
};

struct smpt_region {
	void *va;
	dma_addr_t pa;
	size_t len;
	struct list_head list;
};

struct s2mpu_info {
	void __iomem *base;
	void __iomem *ssmt_base;
	struct device *dev;
	/* lock to take whenever calling an operation using this s2mpu_info instance */
	struct mutex lock;
	struct s2pt pt;
	/* this is list of s2mpu regions for this particular instance */
	struct list_head smpt_regions;
	/* this is global list of s2mpu_info instances */
	struct list_head list;
	u32 *sids;
	unsigned int sidcount;
#ifdef S2MPU_TEST
	struct dentry *debugfs_dentry;
	u32 enabled;
#endif
	u8 vid;
};

/* NOTE all functions in s2mpu-lib.c are single threaded for a given instance
 * of s2mpu_info. s2mpu_lib_init will be called from probe which will be
 * single-threaded. others will acquire s2mpu_info.lock before doing anything.
 */

struct s2mpu_info *s2mpu_lib_init(struct device *dev, void __iomem *base,
				  void __iomem *ssmt_base, u8 vid, u32 *sids,
				  unsigned int sidcount);
void s2mpu_lib_deinit(struct s2mpu_info *info);
int s2mpu_lib_open_close(struct s2mpu_info *info, u64 start, size_t len,
			 bool open);
int s2mpu_lib_restore(struct s2mpu_info *info);
#ifdef S2MPU_TEST
u32 s2mpu_get_enabled(struct s2mpu_info *info);
void s2mpu_set_enabled(struct s2mpu_info *info, u32 val);
#endif

#endif /* __S2MPU_LIB_H */
