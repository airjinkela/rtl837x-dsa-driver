// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/if_bridge.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/version.h>

#include "rtl837x.h"

/*
 * Internal SerDes registers are reached through an indirect access window:
 * write the page/register/port index, then read or write the data register.
 */
#define RTL837X_SDS_INDACS_CMD_REG                      0x3F8
#define   RTL837X_SDS_INDACS_CMD_TRIGGER_MASK           BIT(15)
#define   RTL837X_SDS_INDACS_CMD_WR_MASK                BIT(14)
#define   RTL837X_SDS_INDACS_CMD_REGAD_MASK             GENMASK(11, 7)
#define   RTL837X_SDS_INDACS_CMD_PAGE_MASK              GENMASK(6, 1)
#define   RTL837X_SDS_INDACS_CMD_INDEX_MASK             BIT(0)
#define RTL837X_SDS_INDACS_ADR_REG                      0x3FC
#define RTL837X_SDS_INDACS_DATA_REG                     0x400

/*
 * SMI access to the internal PHYs. CTRL_0 selects the PHY, CTRL_1 carries
 * the command (devad/register/read-write), CTRL_2 returns read data and
 * CTRL_3 holds write data.
 */
#define RTL837X_SMI_PHY_CTRL_0_REG                      0x6438
#define RTL837X_SMI_PHY_CTRL_1_REG                      0x643C
#define   RTL837X_SMI_PHY_CTRL_1_FAIL_MASK              GENMASK(26, 24)
#define   RTL837X_SMI_PHY_CTRL_1_DEVAD_MASK             GENMASK(23, 19)
#define   RTL837X_SMI_PHY_CTRL_1_REG_MASK               GENMASK(18, 3)
#define   RTL837X_SMI_PHY_CTRL_1_WR_MASK                BIT(2)
#define   RTL837X_SMI_PHY_CTRL_1_TYPE_MASK              BIT(1)
#define   RTL837X_SMI_PHY_CTRL_1_TRIGGER_MASK           BIT(0)
#define RTL837X_SMI_PHY_CTRL_2_REG                      0x6440
#define   RTL837X_SMI_PHY_CTRL_2_DATA_MASK              GENMASK(15, 0)
#define RTL837X_SMI_PHY_CTRL_3_REG                      0x6444
#define   RTL837X_SMI_PHY_CTRL_3_DATA_MASK              GENMASK(15, 0)

/*
 * Internal PHY OCP access, used for the vendor MMD registers that are not
 * reachable through the standard SMI window above.
 */
#define RTL837X_PHY_OCP_CTRL_REG                        0xBC8
#define   RTL837X_PHY_OCP_CTRL_ADDR_MASK                GENMASK(31, 16)
#define   RTL837X_PHY_OCP_CTRL_PHYADDR_MASK             GENMASK(8, 4)
#define   RTL837X_PHY_OCP_CTRL_FAIL_MASK                BIT(2)
#define   RTL837X_PHY_OCP_CTRL_WR_MASK                  BIT(1)
#define   RTL837X_PHY_OCP_CTRL_TRIGGER_MASK             BIT(0)
#define RTL837X_PHY_OCP_READ_DATA_REG                   0xBCC
#define   RTL837X_PHY_OCP_READ_DATA_MASK                GENMASK(15, 0)
#define RTL837X_PHY_OCP_WRITE_DATA_REG                  0xBD0


/** ================================================= **/
/** -------------------Register Funcs-----------------**/
/** ================================================= **/

int rtl837x_reg_bits_read(struct realtek_priv *priv, u32 reg, u32 mask, u32 *pval)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_read(priv, reg, &tmp);
	if (ret)
		return ret;

	*pval = (tmp & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_reg_bits_write(struct realtek_priv *priv, u32 reg, u32 mask, u32 val)
{
	return regmap_update_bits(priv->map, reg, mask, (val << __ffs(mask)) & mask);
}

int rtl837x_phy_read_ocp(struct realtek_priv *priv, u16 phy, int regnum, u16 *pval)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->map_lock);

	tmp = FIELD_PREP(RTL837X_PHY_OCP_CTRL_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_PHYADDR_MASK, phy) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_WR_MASK, 0) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_PHY_OCP_CTRL_REG, tmp);
	if (ret)
		goto out;
	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_PHY_OCP_CTRL_REG, tmp,
		((tmp & (RTL837X_PHY_OCP_CTRL_TRIGGER_MASK | RTL837X_PHY_OCP_CTRL_FAIL_MASK)) == 0),
		0, 1000);
	if (tmp & RTL837X_PHY_OCP_CTRL_FAIL_MASK) {
		ret = -EIO;
		goto out;
	}

	if (ret)
		goto out;

	ret = regmap_read(priv->map_nolock, RTL837X_PHY_OCP_READ_DATA_REG, &tmp);
	if (ret)
		goto out;
	*pval = (tmp & RTL837X_PHY_OCP_READ_DATA_MASK) >> __ffs(RTL837X_PHY_OCP_READ_DATA_MASK);
out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

int rtl837x_phy_write_ocp(struct realtek_priv *priv, u16 phy, int regnum, u16 val)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->map_lock);

	ret = regmap_write(priv->map_nolock, RTL837X_PHY_OCP_WRITE_DATA_REG, val);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_PHY_OCP_CTRL_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_PHYADDR_MASK, phy) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_WR_MASK, 1) |
		  FIELD_PREP(RTL837X_PHY_OCP_CTRL_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_PHY_OCP_CTRL_REG, tmp);
	if (ret)
		goto out;
	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_PHY_OCP_CTRL_REG, tmp,
		((tmp & (RTL837X_PHY_OCP_CTRL_TRIGGER_MASK | RTL837X_PHY_OCP_CTRL_FAIL_MASK)) == 0),
		0, 1000);
	if (tmp & RTL837X_PHY_OCP_CTRL_FAIL_MASK) {
		ret = -EIO;
		goto out;
	}

	if (ret)
		goto out;
out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

// It seems that the C22 function is incomplete on phy rtl8224?
int rtl837x_phy_read_c22(struct realtek_priv *priv, u16 phy, int regnum, u16 *pval)
{
	return rtl837x_phy_read_ocp(priv, phy, 0xA400 + (regnum * 2), pval);
}

int rtl837x_phy_write_c22(struct realtek_priv *priv, u16 phy, int regnum, u16 val)
{
	return rtl837x_phy_write_ocp(priv, phy, 0xA400 + (regnum * 2), val);
}

int rtl837x_phy_read_c45(struct realtek_priv *priv, int phy, int devad, int regnum, u16 *pval)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->map_lock);

	ret = regmap_update_bits(priv->map_nolock, RTL837X_SMI_PHY_CTRL_3_REG,
			  RTL837X_SMI_PHY_CTRL_3_DATA_MASK,
			  FIELD_PREP(RTL837X_SMI_PHY_CTRL_3_DATA_MASK, phy)
			);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_DEVAD_MASK, devad) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_REG_MASK, regnum) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_WR_MASK, 0) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_SMI_PHY_CTRL_1_REG, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SMI_PHY_CTRL_1_REG, tmp,
		((tmp & (RTL837X_SMI_PHY_CTRL_1_TRIGGER_MASK | RTL837X_SMI_PHY_CTRL_1_FAIL_MASK)) == 0),
		0, 1000);
	if (tmp & RTL837X_SMI_PHY_CTRL_1_FAIL_MASK) {
		ret = -EIO;
		goto out;
	}

	if (ret)
		goto out;

	ret = regmap_read(priv->map_nolock, RTL837X_SMI_PHY_CTRL_2_REG, &tmp);
	if (ret)
		goto out;

	*pval = (tmp & RTL837X_SMI_PHY_CTRL_2_DATA_MASK) >> __ffs(RTL837X_SMI_PHY_CTRL_2_DATA_MASK);
out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

int rtl837x_phys_write_c45(struct realtek_priv *priv, u16 phy_mask, int devad, int regnum, u16 val)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->map_lock);

	ret = regmap_write(priv->map_nolock, RTL837X_SMI_PHY_CTRL_0_REG, phy_mask);
	if (ret)
		goto out;

	ret = regmap_update_bits(priv->map_nolock, RTL837X_SMI_PHY_CTRL_3_REG,
			  RTL837X_SMI_PHY_CTRL_3_DATA_MASK,
			  FIELD_PREP(RTL837X_SMI_PHY_CTRL_3_DATA_MASK, val)
			);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_DEVAD_MASK, devad) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_REG_MASK, regnum) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_WR_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_PHY_CTRL_1_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_SMI_PHY_CTRL_1_REG, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SMI_PHY_CTRL_1_REG, tmp,
		((tmp & (RTL837X_SMI_PHY_CTRL_1_TRIGGER_MASK | RTL837X_SMI_PHY_CTRL_1_FAIL_MASK)) == 0),
		0, 1000);
	if (tmp & RTL837X_SMI_PHY_CTRL_1_FAIL_MASK) {
		ret = -EIO;
		goto out;
	}

	if (ret)
		goto out;

out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

int rtl837x_phy_write_c45(struct realtek_priv *priv, int phy, int devad, int regnum, u16 val)
{
	return rtl837x_phys_write_c45(priv, BIT(phy), devad, regnum, val);
}

int rtl837x_phy_bits_read_c45(struct realtek_priv *priv, int phy, int devad, int regnum, u16 mask, u16 *pdata)
{
	int ret;
	u16 val;

	ret = rtl837x_phy_read_c45(priv, phy, devad, regnum, &val);
	if (ret)
		return ret;

	*pdata = (val & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_phy_bits_write_c45(struct realtek_priv *priv, int phy, int devad, int regnum, u16 mask, u16 data)
{
	int ret;
	u16 val;

	ret = rtl837x_phy_read_c45(priv, phy, devad, regnum, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= (data << __ffs(mask)) & mask;

	return rtl837x_phy_write_c45(priv, phy, devad, regnum, val);
}

int rtl837x_sds_reg_read(struct realtek_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata)
{
	u32 val, tmp;
	int ret;

	mutex_lock(&priv->map_lock);

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_TRIGGER_MASK) == 0),
		  0, 1000);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SDS_INDACS_CMD_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_WR_MASK, 0) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_TRIGGER_MASK) == 0),
		  0, 1000);
	if (ret)
		goto out;

	ret = regmap_read(priv->map_nolock, RTL837X_SDS_INDACS_ADR_REG, &val);
	if (ret)
		goto out;

	*pdata = val & 0xFFFF;
out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

int rtl837x_sds_reg_write(struct realtek_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->map_lock);

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp,
			  ((tmp & RTL837X_SDS_INDACS_CMD_TRIGGER_MASK) == 0),
			  10, 10000);
	if (ret)
		goto out;

	ret = regmap_write(priv->map_nolock, RTL837X_SDS_INDACS_DATA_REG, data);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SDS_INDACS_CMD_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_WR_MASK, 1) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_TRIGGER_MASK, 1);

	ret = regmap_write(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map_nolock, RTL837X_SDS_INDACS_CMD_REG, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_TRIGGER_MASK) == 0),
		  10, 10000);
	if (ret)
		goto out;

out:
	mutex_unlock(&priv->map_lock);
	return ret;
}

int rtl837x_sds_reg_bits_read(struct realtek_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata)
{
	int ret;
	u16 val;

	ret = rtl837x_sds_reg_read(priv, sds_idx, sds_page, sds_reg, &val);
	if (ret)
		return ret;

	*pdata = (val & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_sds_reg_bits_write(struct realtek_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data)
{
	int ret;
	u16 val;

	ret = rtl837x_sds_reg_read(priv, sds_idx, sds_page, sds_reg, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= (data << __ffs(mask)) & mask;

	return rtl837x_sds_reg_write(priv, sds_idx, sds_page, sds_reg, val);
}
