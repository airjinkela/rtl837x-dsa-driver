// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include "rtl837x.h"
#include "rtl837x_table.h"

/*
 * Indirect table access (ITA). A table is described by a type, an operation
 * and an address; the entry itself is exchanged through five 32-bit words.
 */
#define RTL837X_TABLE_CTRL_REG                       0x5CAC
#define   RTL837X_TABLE_CTRL_ADDR_MASK               GENMASK(28, 16)
#define   RTL837X_TABLE_CTRL_TABLE_MASK              GENMASK(10, 8)
#define   RTL837X_TABLE_CTRL_OP_MASK                 BIT(1)
#define   RTL837X_TABLE_CTRL_EXECUTE_MASK            BIT(0)
#define RTL837X_TABLE_WRITE_REG(_i)                  (0x5CB8 + (((_i) << 2))) /* index: 0-4 */
#define RTL837X_TABLE_READ_REG(_i)                   (0x5CCC + (((_i) << 2))) /* index: 0-4 */

/* Extra L2 lookup controls: port, method and extended entry address */
#define RTL837X_TABLE_L2_CTRL_REG                    0x5CB0
#define   RTL837X_TABLE_L2_CTRL_PORT_IDX_MASK        GENMASK(22, 19)
#define   RTL837X_TABLE_L2_CTRL_ENTRY_CLEAN_MASK     BIT(18)
#define   RTL837X_TABLE_L2_CTRL_METHOD_MASK          GENMASK(17, 14)
#define   RTL837X_TABLE_L2_CTRL_TBL_TYPE_MASK        BIT(13) /* undocumented */
#define   RTL837X_TABLE_L2_CTRL_HIT_MASK             BIT(12)
#define   RTL837X_TABLE_L2_CTRL_ADDR_EXT_MASK        GENMASK(11, 0)

int rtl837x_ita_execute(struct realtek_priv *priv, enum rtl837x_table table,
			enum rtl837x_table_opera opera, u16 *addr, int port,
			enum rtl837x_lut_method method, u32 *data, size_t size)
{
	u32 cmd, l2_ctrl, tmp;
	bool write_data = false;
	int ret = 0;

	l2_ctrl = 0;
	cmd = 0;
	cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_TABLE_MASK, table);
	cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_OP_MASK, opera);
	cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_EXECUTE_MASK, 1);

	if (table == RTL837X_TABLE_L2) {
		if (opera == RTL837X_TABLE_OPERA_READ) {
			l2_ctrl |= FIELD_PREP(RTL837X_TABLE_L2_CTRL_METHOD_MASK, method);

			switch (method) {
			case RTL837X_LUT_METHOD_NEXT_L2UCSPA:
				l2_ctrl |= FIELD_PREP(RTL837X_TABLE_L2_CTRL_PORT_IDX_MASK, port);
				fallthrough;
			case RTL837X_LUT_METHOD_ADDRESS:
			case RTL837X_LUT_METHOD_NEXT_ADDRESS:
			case RTL837X_LUT_METHOD_NEXT_L2UC:
			case RTL837X_LUT_METHOD_NEXT_L2MC:
			case RTL837X_LUT_METHOD_NEXT_L3MC:
			case RTL837X_LUT_METHOD_NEXT_L2L3MC:
				cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_ADDR_MASK, *addr);
				break;
			case RTL837X_LUT_METHOD_MAC:
				write_data = true;
				break;
			default:
				ret = -EINVAL;
				goto out;
			}
		} else if (opera == RTL837X_TABLE_OPERA_DELETE) {
			cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_ADDR_MASK, *addr);
			l2_ctrl |= FIELD_PREP(RTL837X_TABLE_L2_CTRL_ENTRY_CLEAN_MASK, 1);
		} else {
			write_data = true;
		}
	} else if (table == RTL837X_TABLE_CVLAN) {
		cmd |= FIELD_PREP(RTL837X_TABLE_CTRL_ADDR_MASK, *addr);
		if (opera == RTL837X_TABLE_OPERA_WRITE)
			write_data = true;
	} else if (opera == RTL837X_TABLE_OPERA_WRITE) {
		write_data = true;
	}

	mutex_lock(&priv->map_lock);

	/* Check the busy flag before touching the table */
	ret = regmap_read_poll_timeout(priv->map_nolock,
				       RTL837X_TABLE_CTRL_REG, tmp,
				       ((tmp & RTL837X_TABLE_CTRL_EXECUTE_MASK) == 0),
				       10, 10000);
	if (ret)
		goto out;

	if (write_data) {
		for (int i = 0; i < size; i++) {
			ret = regmap_write(priv->map_nolock,
					   RTL837X_TABLE_WRITE_REG(i), data[i]);
			if (ret)
				goto out;
		}
	}

	/* Write the ITA L2 control register */
	ret = regmap_write(priv->map_nolock, RTL837X_TABLE_L2_CTRL_REG, l2_ctrl);
	if (ret)
		goto out;

	/* Execute the table command */
	ret = regmap_write(priv->map_nolock, RTL837X_TABLE_CTRL_REG, cmd);
	if (ret)
		goto out;

	/* Wait for the busy flag to clear */
	ret = regmap_read_poll_timeout(priv->map_nolock,
				       RTL837X_TABLE_CTRL_REG, tmp,
				       ((tmp & RTL837X_TABLE_CTRL_EXECUTE_MASK) == 0),
				       10, 10000);
	if (ret)
		goto out;

	if (table == RTL837X_TABLE_L2) {
		bool hit;

		if (opera == RTL837X_TABLE_OPERA_DELETE)
			goto out;

		/* Check whether the query hit an entry */
		ret = regmap_read(priv->map_nolock, RTL837X_TABLE_L2_CTRL_REG,
				  &tmp);
		if (ret)
			goto out;

		hit = FIELD_GET(RTL837X_TABLE_L2_CTRL_HIT_MASK, tmp);

		if (!hit) {
			ret = -ENOENT;
			goto out;
		}

		*addr = FIELD_GET(RTL837X_TABLE_L2_CTRL_ADDR_EXT_MASK, tmp);
	}

	if (opera == RTL837X_TABLE_OPERA_READ) {
		for (int i = 0; i < size; i++) {
			ret = regmap_read(priv->map_nolock,
					  RTL837X_TABLE_READ_REG(i), &data[i]);
			if (ret)
				goto out;
		}
	}

out:
	mutex_unlock(&priv->map_lock);
	return ret;
}
