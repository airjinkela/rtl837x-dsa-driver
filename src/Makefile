# SPDX-License-Identifier: GPL-2.0-or-later 
# Copyright (C) 2025 StarField Xu <air_jinkela@163.com>

ccflags-y += -Wall

ifeq ($(CONFIG_RTL8372N_DSA_DEBUG),y)
ccflags-y += -DDEBUG
endif

obj-$(CONFIG_RTL8372N_DSA) += rtl8372n_dsa.o

rtl8372n_dsa-objs := rtl8372n.o

ifeq ($(CONFIG_GPIOLIB),y)
rtl8372n_dsa-objs += rtl837x_gpio.o
endif

rtl8372n_dsa-objs += rtl837x_dbg.o
rtl8372n_dsa-objs += rtl837x_mdio.o
rtl8372n_dsa-objs += rtl837x_common.o
rtl8372n_dsa-objs += rtl837x_common_l2.o

ifeq ($(CONFIG_RTL837X_PHY_PATCH),y)
ccflags-y += -DRTL837X_PHY_PATCH
rtl8372n_dsa-objs += rtl837x_intphy_patch.o
endif
