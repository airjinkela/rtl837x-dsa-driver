/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef _RTL837X_TABLE_H
#define _RTL837X_TABLE_H

#include "rtl837x.h"
#include "rtl837x_l2.h"

#define RTL837X_ITA_TABLE_SIZE 5 // 5*u32

enum rtl837x_table_opera {
	RTL837X_TABLE_OPERA_READ = 0,
	RTL837X_TABLE_OPERA_WRITE = 1,
	RTL837X_TABLE_OPERA_DELETE = 3,
};

enum rtl837x_table {
	RTL837X_TABLE_ACLRULE = 1,
	RTL837X_TABLE_ACLACT,
	RTL837X_TABLE_CVLAN,
	RTL837X_TABLE_L2,
	RTL837X_TABLE_IGMP_GROUP,
	RTL837X_TABLE_HSA,
	RTL837X_TABLE_HSB
};

int rtl837x_ita_execute(struct rtl837x_priv *priv, enum rtl837x_table table,
		    enum rtl837x_table_opera opera, u16 *addr, int port,
			enum rtl837x_l2_method method,
		    u32 *data, size_t size);

#endif /* _RTL837X_TABLE_H */