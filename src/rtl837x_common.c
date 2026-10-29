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


char *chipid_to_chip_name(enum switch_chip id)
{
	switch (id) {
	case CHIP_RTL8373:
	return "RTL8373";
	case CHIP_RTL8372:
	return "RTL8372";
	case CHIP_RTL8224:
	return "RTL8224";
	case CHIP_RTL8373N:
	return "RTL8373N";
	case CHIP_RTL8372N:
	return "RTL8372N";
	case CHIP_RTL8224N:
	return "RTL8224N";
	case CHIP_RTL8366U:
	return "RTL8366U";
	default:
	return "Unknow";
	}
}

/** ================================================= **/
/** -------------------Register Funcs-----------------**/
/** ================================================= **/

int rtl837x_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_read(priv, reg, &tmp);
	if (ret)
		return ret;

	*pval = (tmp & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val)
{
	return regmap_update_bits(priv->map, reg, mask, (val << __ffs(mask)) & mask);
}

int rtl837x_phy_read_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->phy_ocp_lock);

	tmp = FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK, phy) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK, 0) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp);
	if (ret)
		goto out;
	ret = regmap_read_poll_timeout(priv->map, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp,
		((tmp & (RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK | RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK)) == 0),
		0, 1000);
	if (ret)
		goto out;

	ret = rtl837x_reg_read(priv, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_ADDR, &tmp);
	if (ret)
		goto out;
	*pval = (tmp & RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK) >> __ffs(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK);
out:
	mutex_unlock(&priv->phy_ocp_lock);
	return ret;
}

int rtl837x_phy_write_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->phy_ocp_lock);

	ret = rtl837x_reg_write(priv, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_2_ADDR, val);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK, phy) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK, 1) |
		  FIELD_PREP(RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp);
	if (ret)
		goto out;
	ret = regmap_read_poll_timeout(priv->map, RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp,
		((tmp & (RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK | RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK)) == 0),
		0, 1000);
	if (ret)
		goto out;
out:
	mutex_unlock(&priv->phy_ocp_lock);
	return ret;
}

// It seems that the C22 function is incomplete on phy rtl8224?
int rtl837x_phy_read_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval)
{
	return rtl837x_phy_read_ocp(priv, phy, 0xA400 + (regnum * 2), pval);
}

int rtl837x_phy_write_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val)
{
	return rtl837x_phy_write_ocp(priv, phy, 0xA400 + (regnum * 2), val);
}

int rtl837x_phy_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 *pval)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->phy_mmd_lock);

	ret = rtl837x_reg_bits_write(priv, RTL837X_SMI_ACCESS_PHY_CTRL_3_ADDR,
			  RTL837X_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, phy);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, devad) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, regnum) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK, 0) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp,
		((tmp & (RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK | RTL837X_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK)) == 0),
		0, 1000);
	if (ret)
		goto out;

	ret = rtl837x_reg_read(priv, RTL837X_SMI_ACCESS_PHY_CTRL_2_ADDR, &tmp);
	if (ret)
		goto out;

	*pval = (tmp & RTL837X_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK) >> __ffs(RTL837X_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK);
out:
	mutex_unlock(&priv->phy_mmd_lock);
	return ret;
}

int rtl837x_phys_write_c45(struct rtl837x_priv *priv, u16 phy_mask, int devad, int regnum, u16 val)
{
	int ret;
	u32 tmp;

	mutex_lock(&priv->phy_mmd_lock);

	ret = rtl837x_reg_write(priv, RTL837X_SMI_ACCESS_PHY_CTRL_0_ADDR, phy_mask);
	if (ret)
		goto out;

	ret = rtl837x_reg_bits_write(priv, RTL837X_SMI_ACCESS_PHY_CTRL_3_ADDR,
			  RTL837X_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK,
			  val);
	if (ret)
		goto out;

	tmp = FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, devad) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, regnum) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp);
	if (ret)
		goto out;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp,
		((tmp & (RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK | RTL837X_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK)) == 0),
		0, 1000);
out:
	mutex_unlock(&priv->phy_mmd_lock);
	return ret;
}

int rtl837x_phy_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 val)
{
	return rtl837x_phys_write_c45(priv, BIT(phy), devad, regnum, val);
}

int rtl837x_phy_bits_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 *pdata)
{
	int ret;
	u16 val;

	ret = rtl837x_phy_read_c45(priv, phy, devad, regnum, &val);
	if (ret)
		return ret;

	*pdata = (val & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_phy_bits_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 data)
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

int rtl837x_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata)
{
	u32 val, tmp;
	int ret;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_RWOP_MASK, 0) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_read(priv, RTL837X_SDS_INDACS_RD_ADDR, &val);
	if (ret)
		return ret;

	*pdata = val & 0xFFFF;
	return 0;
}

int rtl837x_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data)
{
	int ret;
	u32 tmp;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SDS_INDACS_CMD_ADDR, tmp,
			  ((tmp & RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
			  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_write(priv, RTL837X_SDS_INDACS_WD_ADDR, data);

	tmp = FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_RWOP_MASK, 1) |
				FIELD_PREP(RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL837X_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata)
{
	int ret;
	u16 val;

	ret = rtl837x_sds_reg_read(priv, sds_idx, sds_page, sds_reg, &val);
	if (ret)
		return ret;

	*pdata = (val & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data)
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

/** ================================================= **/
/** --------------------Other Funcs-------------------**/
/** ================================================= **/
int rtl837x_get_chip_ver(struct rtl837x_priv *priv, u16 *chip_ver)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_write(priv, RTL837X_CHIP_INFO_ADDR,
			    RTL837X_CHIP_INFO_CHIP_INFO_EN_MASK, 0xa);
	if (ret)
		return ret;
	ret = rtl837x_reg_bits_read(priv, RTL837X_CHIP_INFO_ADDR,
			    RTL837X_CHIP_INFO_RL_VID_MASK, &tmp);
	if (ret)
		return ret;
	ret = rtl837x_reg_bits_write(priv, RTL837X_CHIP_INFO_ADDR,
			    RTL837X_CHIP_INFO_CHIP_INFO_EN_MASK, 0);

	*chip_ver = tmp;
	return 0;
}
