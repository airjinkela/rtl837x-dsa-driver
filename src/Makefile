# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2025 StarField Xu <air_jinkela@163.com>

ccflags-y += -Wall

ifeq ($(CONFIG_RTL8372N_DSA_DEBUG),y)
ccflags-y += -DDEBUG
endif

obj-$(CONFIG_RTL8372N_DSA) += rtl8372n_dsa.o

rtl8372n_dsa-objs := rtl8372n.o

rtl8372n_dsa-objs += rtl837x_dbg.o
rtl8372n_dsa-objs += rtl837x_mdio.o
rtl8372n_dsa-objs += rtl837x_common.o
rtl8372n_dsa-objs += rtl837x_l2.o
rtl8372n_dsa-objs += rtl837x_vlan.o
rtl8372n_dsa-objs += rtl837x_gpio.o
rtl8372n_dsa-objs += rtl837x_led.o
rtl8372n_dsa-objs += rtl837x_serdes.o
rtl8372n_dsa-objs += rtl837x_table.o
