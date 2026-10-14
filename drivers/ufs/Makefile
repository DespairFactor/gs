# SPDX-License-Identifier: GPL-2.0

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)
O ?= $(OUT_DIR)
OBJTREE ?= $(O)/$(M)

SBB_MUX_SYMVERS = $(COMMON_OUT_DIR)/private/google-modules/soc/gs/drivers/misc/sbb-mux/sbb-mux_Module.symvers
ifneq ($(wildcard $(SBB_MUX_SYMVERS)),)
EXTRA_SYMBOLS += $(SBB_MUX_SYMVERS)
endif

EXYNOS_CPUPM_SYMVERS = $(COMMON_OUT_DIR)/private/google-modules/soc/gs/drivers/soc/google/exynos-cpupm_Module.symvers
ifneq ($(wildcard $(EXYNOS_CPUPM_SYMVERS)),)
EXTRA_SYMBOLS += $(EXYNOS_CPUPM_SYMVERS)
endif

EXYNOS_PMU_IF_SYMVERS = $(COMMON_OUT_DIR)/private/google-modules/soc/gs/drivers/soc/google/exynos-pmu-if_Module.symvers
ifneq ($(wildcard $(EXYNOS_PMU_IF_SYMVERS)),)
EXTRA_SYMBOLS += $(EXYNOS_PMU_IF_SYMVERS)
endif

EXYNOS_PM_QOS_SYMVERS = $(COMMON_OUT_DIR)/private/google-modules/soc/gs/drivers/soc/google/exynos_pm_qos_Module.symvers
ifneq ($(wildcard $(EXYNOS_PM_QOS_SYMVERS)),)
EXTRA_SYMBOLS += $(EXYNOS_PM_QOS_SYMVERS)
endif

GSA_SYMVERS = $(COMMON_OUT_DIR)/private/google-modules/soc/gs/drivers/soc/google/gsa/gsa_ko_Module.symvers
ifneq ($(wildcard $(GSA_SYMVERS)),)
EXTRA_SYMBOLS += $(GSA_SYMVERS)
endif

modules modules_install headers_install clean compile_commands.json:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) \
	KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

modules_install: modules_preinstall

include $(KERNEL_SRC)/$(M)/Makefile.preinstall
