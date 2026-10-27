// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include "rtl837x.h"
#include "rtl837x_table.h"

#define RTL837X_ITA_L2_CTRL_ADDR                     0x5CB0
#define   RTL837X_ITA_L2_CTRL_PORT_NUM_MASK          GENMASK(22, 19)
#define   RTL837X_ITA_L2_CTRL_ENTRY_CLR_MASK         BIT(18)
#define   RTL837X_ITA_L2_CTRL_READ_MTHD_MASK         GENMASK(17, 14)
#define   RTL837X_ITA_L2_CTRL_ACT_STS_MASK           BIT(12)
#define   RTL837X_ITA_L2_CTRL_TBL_ADDR_MASK          GENMASK(11, 0)

int rtl837x_ita_execute(struct rtl837x_priv *priv, enum rtl837x_table table,
		    enum rtl837x_table_opera opera, u16 *addr, int port,
			enum rtl837x_l2_method method,
		    u32 *data, size_t size)
{
	int ret = 0;
	bool write_data = false;
	u32 cmd, tmp;

	mutex_lock(&priv->ita_lock);

	cmd = 0;
	cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TLB_TYPE_MASK, table);
	cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TLB_ACT_MASK, opera);
	cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TLB_EXECUTE_MASK, RTL837X_TABLE_EXECUTE);

	// check busy
	ret = regmap_read_poll_timeout(priv->map, RTL837X_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL837X_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		goto out;

	if (table == RTL837X_TABLE_L2) {
		if (opera == RTL837X_TABLE_OPERA_READ) {
			ret = rtl837x_reg_bits_write(priv, RTL837X_ITA_L2_CTRL_ADDR,
				  RTL837X_ITA_L2_CTRL_READ_MTHD_MASK, method);
			if (ret)
				goto out;

			switch (method) {
			case LUT_READ_METHOD_ADDRESS:
			case LUT_READ_METHOD_NEXT_ADDRESS:
			case LUT_READ_METHOD_NEXT_L2UC:
			case LUT_READ_METHOD_NEXT_L2MC:
			case LUT_READ_METHOD_NEXT_L3MC:
			case LUT_READ_METHOD_NEXT_L2L3MC:
				cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TBL_ADDR_MASK, *addr);
				break;
			case LUT_READ_METHOD_NEXT_L2UCSPA:
				ret = rtl837x_reg_bits_write(priv, RTL837X_ITA_L2_CTRL_ADDR,
							  RTL837X_ITA_L2_CTRL_PORT_NUM_MASK, port);
				if (ret)
					goto out;
				cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TBL_ADDR_MASK, *addr);
				break;
			case LUT_READ_METHOD_MAC:
				write_data = true;
				break;
			default:
				ret = -EINVAL;
				goto out;
			}
		} else if (opera == RTL837X_TABLE_OPERA_DELETE) {
			cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TBL_ADDR_MASK, *addr);
			ret = rtl837x_reg_bits_write(priv, RTL837X_ITA_L2_CTRL_ADDR,
								  RTL837X_ITA_L2_CTRL_ENTRY_CLR_MASK, 1);
			if (ret)
				goto out;
		} else {
			write_data = true;
		}
	} else if (table == RTL837X_TABLE_CVLAN) {
		cmd |= FIELD_PREP(RTL837X_ITA_CTRL0_TBL_ADDR_MASK, *addr);
		if (opera == RTL837X_TABLE_OPERA_WRITE)
			write_data = true;
	}

	if (write_data) {
		for (int i = 0; i < size; i++) {
			ret = rtl837x_reg_write(priv, RTL837X_ITA_WRITE_DATA0_ADDR(i), data[i]);
			if (ret)
				goto out;
		}
	}

	// execute the table cmd
	ret = rtl837x_reg_write(priv, RTL837X_ITA_CTRL0_ADDR, cmd);
	if (ret)
		goto out;

	// wait busy flag
	ret = regmap_read_poll_timeout(priv->map, RTL837X_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL837X_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		goto out;

	if (table == RTL837X_TABLE_L2) {
		if (opera == RTL837X_TABLE_OPERA_DELETE) {
			ret = rtl837x_reg_bits_write(priv, RTL837X_ITA_L2_CTRL_ADDR,
					  RTL837X_ITA_L2_CTRL_ENTRY_CLR_MASK, 0);
			goto out;
		}

		// check did the query hit a entry
		ret = rtl837x_reg_read(priv, RTL837X_ITA_L2_CTRL_ADDR,
					&tmp);
		if (ret)
			goto out;

		bool hit = FIELD_GET(RTL837X_ITA_L2_CTRL_ACT_STS_MASK, tmp);

		if (!hit) {
			ret = -ENOENT; // not hit
			goto out;
		}

		*addr = FIELD_GET(RTL837X_ITA_L2_CTRL_TBL_ADDR_MASK, tmp);
	}

	if (opera == RTL837X_TABLE_OPERA_READ) {
		for (int i = 0; i < size; i++) {
			ret = rtl837x_reg_read(priv, RTL837X_ITA_READ_DATA0_ADDR(i), &data[i]);
			if (ret)
				goto out;
		}
	}

out:
	mutex_unlock(&priv->ita_lock);
	return ret;
}
