/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef _RTL837X_TABLE_H
#define _RTL837X_TABLE_H

#include "rtl837x.h"
#include "rtl837x_l2.h"

#define RTL837X_ITA_TABLE_SIZE 5 // 5*u32

/*
 * enum rtl837x_table_op - table query operation
 * @RTL837X_TABLE_OPERA_READ: read an entry from the target table
 * @RTL837X_TABLE_OPERA_WRITE: write an entry to the target table
 * @RTL837X_TABLE_OPERA_DELETE: clear the L2 entry at the given address. The
 *	value is only a driver-side discriminator, since ITA_CTRL can encode
 *	just read and write; the delete itself is requested by setting the
 *	entry-clean bit in ITA_L2_CTRL, without any data words. L2 only.
 */
enum rtl837x_table_opera {
	RTL837X_TABLE_OPERA_READ = 0,
	RTL837X_TABLE_OPERA_WRITE = 1,
	RTL837X_TABLE_OPERA_DELETE = 3,
};

/*
 * enum rtl837x_table - available switch tables
 * @RTL837X_TABLE_ACL_RULE: ACL rules
 * @RTL837X_TABLE_ACL_ACTION: ACL actions
 * @RTL837X_TABLE_CVLAN: VLAN4k configurations
 * @RTL837X_TABLE_L2: filtering database (2K hash table)
 * @RTL837X_TABLE_IGMP_GROUP: IGMP group database (readonly)
 *
 * described by @RTL837X_ITA_CTRL_TABLE_TYPE_MASK.
 */
enum rtl837x_table {
	RTL837X_TABLE_ACL_RULE = 1,
	RTL837X_TABLE_ACL_ACT,
	RTL837X_TABLE_CVLAN,
	RTL837X_TABLE_L2,
	RTL837X_TABLE_IGMP_GROUP,
	RTL837X_TABLE_HSA,
	RTL837X_TABLE_HSB
};

/*
 * rtl837x_ita_execute() - read from, write to or clear a switch table
 * @priv: driver context
 * @table: target table, see &enum rtl837x_table
 * @opera: read, write or delete operation, see &enum rtl837x_table_opera
 * @addr: table address. For indexed tables it selects the entry to access.
 *	  For L2 read queries the MAC-based method ignores it as input, while
 *	  the address-based methods use it as the starting address. On a
 *	  successful L2 operation it is updated with the matched or allocated
 *	  entry address, so it is both an input and an output for that table.
 * @port: for L2 read queries using %RTL837X_LUT_METHOD_NEXT_L2UCSPA, restrict
 *	  the search to entries learnt on this source port. Ignored otherwise.
 * @method: L2 lookup method, see &enum rtl837x_lut_method. Ignored for
 *	  non-L2 tables.
 * @data: entry buffer. Input when writing, and also the lookup key for
 *	  %RTL837X_LUT_METHOD_MAC; output for every successful read. Must
 *	  hold at least @size words.
 * @size: number of 32-bit words in @data, at most %RTL837X_ITA_TABLE_SIZE.
 *	  Unused by %RTL837X_TABLE_OPERA_DELETE.
 *
 * Single access path to the switch tables behind the ITA (indirect table
 * access) engine. All tables except L2 are indexed: @addr selects the entry
 * and @opera selects read or write. The L2 table is a hash table, so it
 * additionally accepts several read methods: %RTL837X_LUT_METHOD_MAC looks
 * up the MAC/FID key supplied in @data, whereas the address-based methods
 * read the entry at @addr or iterate over the following valid entries.
 *
 * %RTL837X_TABLE_OPERA_DELETE clears the L2 entry at @addr. It is only valid
 * for @table == %RTL837X_TABLE_L2: the operation field in ITA_CTRL has a
 * single bit, so the delete is carried out by asserting the entry-clean bit
 * in ITA_L2_CTRL, and no data words are transferred.
 *
 * The function serialises all accesses on priv->map_lock, so callers must
 * not hold it.
 *
 * Return: 0 on success, -ENOENT if an L2 lookup matched no entry, -EINVAL
 * for an unsupported L2 read method, or another negative error code on
 * failure.
 */
int rtl837x_ita_execute(struct rtl837x_priv *priv, enum rtl837x_table table,
		    enum rtl837x_table_opera opera, u16 *addr, int port,
			enum rtl837x_lut_method method,
		    u32 *data, size_t size);

#endif /* _RTL837X_TABLE_H */
