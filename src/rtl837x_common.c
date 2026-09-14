/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#include "./rtl837x.h"

static const u16 patch_an_10p3125g_a[][3] ={
	{0x0021, 0x0010, 0x4480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x002E, 0x0004, 0x0080},
	{0x002E, 0x0006, 0x0408},
	{0x002E, 0x0007, 0x020D},
	{0x002E, 0x0009, 0x0601},
	{0x002E, 0x000B, 0x222C},
	{0x002E, 0x000C, 0x9217},
	{0x002E, 0x0016, 0x0743},
	{0x002E, 0x001D, 0xABB0}
};

static const u16 patch_an_10p3125g_b[][3] ={
	{0x0021, 0x0010, 0x4480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F},
	{0x0036, 0x0010, 0x0200},
	{0x002E, 0x0004, 0x0080},
	{0x002E, 0x0006, 0x0408},
	{0x002E, 0x0007, 0x020D},
	{0x002E, 0x0009, 0x0601},
	{0x002E, 0x000B, 0x222C},
	{0x002E, 0x000C, 0xA217},
	{0x002E, 0x000D, 0xFE40},
	{0x002E, 0x0015, 0xF5C1},
	{0x002E, 0x0016, 0x0443},
	{0x002E, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_a[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0028, 0x0004, 0x0080},
	{0x0028, 0x0009, 0x0601},
	{0x0028, 0x000B, 0x232C},
	{0x0028, 0x000C, 0x9217},
	{0x0028, 0x000F, 0x5B50},
	{0x0028, 0x0016, 0x0443},
	{0x0028, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_b[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F},
	{0x0036, 0x0010, 0x0200},
	{0x0028, 0x0004, 0x0080},
	{0x0028, 0x0007, 0x1201},
	{0x0028, 0x0009, 0x0601},
	{0x0028, 0x000B, 0x232C},
	{0x0028, 0x000C, 0x9217},
	{0x0028, 0x000F, 0x5B50},
	{0x0028, 0x0015, 0xE7F1},
	{0x0028, 0x0016, 0x0443},
	{0x0028, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_a[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F},
	{0x0036, 0x0010, 0x0300},
	{0x0024, 0x0004, 0x0080},
	{0x0024, 0x0007, 0x1201},
	{0x0024, 0x0009, 0x0601},
	{0x0024, 0x000B, 0x232C},
	{0x0024, 0x000C, 0x9217},
	{0x0024, 0x000F, 0x5B50},
	{0x0024, 0x0015, 0xE7C1},
	{0x0024, 0x0016, 0x0443},
	{0x0024, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_b[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F},
	{0x0036, 0x0010, 0x0300},
	{0x0024, 0x0004, 0x0080},
	{0x0024, 0x0007, 0x1201},
	{0x0024, 0x0009, 0x0601},
	{0x0024, 0x000B, 0x232C},
	{0x0024, 0x000C, 0x9217},
	{0x0024, 0x000F, 0x5B50},
	{0x0024, 0x0015, 0xE7C1},
	{0x0024, 0x0016, 0x0443},
	{0x0024, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_a[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0026, 0x0004, 0x0080},
	{0x0026, 0x0009, 0x0601},
	{0x0026, 0x000B, 0x232C},
	{0x0026, 0x000C, 0x9217},
	{0x0026, 0x000F, 0x5B50},
	{0x0026, 0x0016, 0x0443},
	{0x0026, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_b[][3]={
	{0x0021, 0x0010, 0x6480},
	{0x0021, 0x0013, 0x0400},
	{0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E},
	{0x0021, 0x001D, 0x0002},
	{0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F},
	{0x0036, 0x0010, 0x0300},
	{0x0026, 0x0004, 0x0080},
	{0x0026, 0x0007, 0x1201},
	{0x0026, 0x0009, 0x0601},
	{0x0026, 0x000B, 0x232C},
	{0x0026, 0x000C, 0x9217},
	{0x0026, 0x000F, 0x5B50},
	{0x0026, 0x0015, 0xE7C1},
	{0x0026, 0x0016, 0x0443},
	{0x0026, 0x001D, 0xABB0}
};

static const u16 patch_data_mac[][3] = {
	{0x0006, 0x0012, 0x5078},
	{0x0007, 0x0006, 0x9401},
	{0x0007, 0x0008, 0x9401},
	{0x0007, 0x000A, 0x9401},
	{0x0007, 0x000C, 0x9401},
	{0x001F, 0x000B, 0x0003},
	{0x0006, 0x0003, 0xC45C},
	{0x0006, 0x001F, 0x2100}
};

static const u16 patch_data_phy[][3]= {
	{0x0006, 18, 0x5078},
	{0x0006,  3, 0xc45c},
	{0x0006, 30, 0x000C},
	{0x0006, 31, 0x2100} 
};

char* chipid_to_chip_name(switch_chip_t id)
{
    switch (id)
    {
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

rtk_sds_mode_t phy_interface_to_rtk_sds_mode(phy_interface_t interface)
{
	switch (interface)
	{
	case PHY_INTERFACE_MODE_USXGMII:
		return SERDES_10GUSXG;
	case PHY_INTERFACE_MODE_1000BASEX:
		return SERDES_1000BASEX;
	case PHY_INTERFACE_MODE_SGMII:
		return SERDES_SG;
	case PHY_INTERFACE_MODE_2500BASEX:
		return SERDES_2500BASEX;
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10GKR:
	default:
		return SERDES_10GR;
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

int rtl837x_phy_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 *pval)
{
	int ret;
    u32 tmp;

	ret = rtl837x_reg_bits_write(priv, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR,
			  RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, phy);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, devad) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, regnum) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK, 0) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp, 
		((tmp & (RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK | RTL8373_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK))==0),
		0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_read(priv, RTL8373_SMI_ACCESS_PHY_CTRL_2_ADDR, &tmp);
	if (ret)
		return ret;

	*pval = (tmp & RTL8373_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK) >> __ffs(RTL8373_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK); 
	return 0;
}

int rtl837x_phy_read_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval)
{
	int ret;
    u32 tmp;
	tmp = FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK, phy) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK, 0) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp);
	if (ret)
		return ret;
	ret = regmap_read_poll_timeout(priv->map, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp, 
		((tmp & (RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK | RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK))==0),
		0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_read(priv, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_1_ADDR, &tmp);
	if (ret)
		return ret;
	*pval = (tmp & RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK) >> __ffs(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK); 
    return 0;
}

int rtl837x_phy_write_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val)
{
	int ret;
    u32 tmp;
	ret = rtl837x_reg_write(priv, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_2_ADDR, val);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK, regnum) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK, phy) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK, 1) |
		  FIELD_PREP(RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp);
	if (ret)
		return ret;
	ret = regmap_read_poll_timeout(priv->map, RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR, tmp, 
		((tmp & (RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK | RTL8373_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK))==0),
		0, 1000);
	if (ret)
		return ret;
    return 0;
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

int rtl837x_phys_write_c45(struct rtl837x_priv *priv, u16 phy_mask, int devad, int regnum, u16 val)
{
	int ret;
    u32 tmp;

	ret = rtl837x_reg_write(priv, RTL8373_SMI_ACCESS_PHY_CTRL_0_ADDR, phy_mask);
	if (ret)
		return ret;

	ret = rtl837x_reg_bits_write(priv, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR,
			  RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK,
			  val);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, devad) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, regnum) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK, 1) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK, 1) |
		FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, tmp, 
		((tmp & (RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK | RTL8373_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK))==0),
		0, 1000);
	if (ret)
		return ret;
    return 0;
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
	int ret;
	u32 val, tmp;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_RWOP_MASK, 0) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_read(priv, RTL8373_SDS_INDACS_RD_ADDR, &val);
	if (ret)
		return ret;

	*pdata = val & 0xFFFF;
	return 0;
}

int rtl837x_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data)
{
	int ret;
	u32 tmp;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
			  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
			  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_write(priv, RTL8373_SDS_INDACS_WD_ADDR, data);

	tmp = FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_RWOP_MASK, 1) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_reg_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
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

int rtl837x_rtl8224_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval)
{
	int ret;
	u32 tmp;

	ret = rtl837x_rtl8224_reg_read(priv, reg, &tmp);
	if (ret)
		return ret;

	*pval = (tmp & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_rtl8224_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val)
{
	if (!priv->map_8224)
		return -ENODEV;
    return regmap_update_bits(priv->map_8224, reg, mask, (val << __ffs(mask)) & mask);
}

int rtl837x_rtl8224_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata)
{
	int ret;
	u32 val, tmp;

	if (!priv->map_8224)
		return -ENODEV;

	ret = regmap_read_poll_timeout(priv->map_8224, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_RWOP_MASK, 0) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_rtl8224_reg_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map_8224, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_rtl8224_reg_read(priv, RTL8373_SDS_INDACS_RD_ADDR, &val);
	if (ret)
		return ret;

	*pdata = val & 0xFFFF;
	return 0;
}

int rtl837x_rtl8224_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data)
{
	int ret;
	u32 tmp;

	if (!priv->map_8224)
		return -ENODEV;

	ret = regmap_read_poll_timeout(priv->map_8224, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
			  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
			  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_rtl8224_reg_write(priv, RTL8373_SDS_INDACS_WD_ADDR, data);

	tmp = FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_INDEX_MASK, sds_idx) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_RWOP_MASK, 1) |
				FIELD_PREP(RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK, 1);

	ret = rtl837x_rtl8224_reg_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map_8224, RTL8373_SDS_INDACS_CMD_ADDR, tmp,
		  ((tmp & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_rtl8224_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata)
{
	int ret;
	u16 val;

	ret = rtl837x_rtl8224_sds_reg_read(priv, sds_idx, sds_page, sds_reg, &val);
	if (ret)
		return ret;

	*pdata = (val & mask) >> __ffs(mask);
	return 0;
}

int rtl837x_rtl8224_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data)
{
	int ret;
	u16 val;

	ret = rtl837x_rtl8224_sds_reg_read(priv, sds_idx, sds_page, sds_reg, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= (data << __ffs(mask)) & mask;

	return rtl837x_rtl8224_sds_reg_write(priv, sds_idx, sds_page, sds_reg, val);
}

#define _reg_read(is_8224, priv, reg, pval) \
	  (is_8224 ? rtl837x_rtl8224_reg_read(priv, reg, pval): \
	             rtl837x_reg_read(priv, reg, pval))
#define _reg_write(is_8224, priv, reg, val) \
	  (is_8224 ? rtl837x_rtl8224_reg_write(priv, reg, val): \
	             rtl837x_reg_write(priv, reg, val))
#define _reg_bits_read(is_8224, priv, reg, mask, pval) \
	  (is_8224 ? rtl837x_rtl8224_reg_bits_read(priv, reg, mask, pval): \
	             rtl837x_reg_bits_read(priv, reg, mask, pval))
#define _reg_bits_write(is_8224, priv, reg, mask, val) \
	  (is_8224 ? rtl837x_rtl8224_reg_bits_write(priv, reg, mask, val): \
	             rtl837x_reg_bits_write(priv, reg, mask, val))

#define _sds_reg_read(is_8224, priv, sds_idx, page, reg, pval) \
	  (is_8224 ? rtl837x_rtl8224_sds_reg_read(priv, sds_idx, page, reg, pval): \
	             rtl837x_sds_reg_read(priv, sds_idx, page, reg, pval))
#define _sds_reg_write(is_8224, priv, sds_idx, page, reg, val) \
	  (is_8224 ? rtl837x_rtl8224_sds_reg_write(priv, sds_idx, page, reg, val): \
	             rtl837x_sds_reg_write(priv, sds_idx, page, reg, val))
#define _sds_reg_bits_read(is_8224, priv, sds_idx, page, reg, mask, pval) \
	  (is_8224 ? rtl837x_rtl8224_sds_reg_bits_read(priv, sds_idx, page, reg, mask, pval): \
	             rtl837x_sds_reg_bits_read(priv, sds_idx, page, reg, mask, pval))
#define _sds_reg_bits_write(is_8224, priv, sds_idx, page, reg, mask, val) \
	  (is_8224 ? rtl837x_rtl8224_sds_reg_bits_write(priv, sds_idx, page, reg, mask, val): \
	             rtl837x_sds_reg_bits_write(priv, sds_idx, page, reg, mask, val))

/** ================================================= **/
/** --------------------Common Funcs------------------**/
/** ================================================= **/

int rtl837x_vlan_set(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_write(priv, RTL8373_ITA_WRITE_DATA0_ADDR(0), vlan->val);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_ITA_CTRL0_TBL_ADDR_MASK, vlan->vid) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_CVLAN) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_WRITE) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);

	ret = rtl837x_reg_write(priv, RTL8373_ITA_CTRL0_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;
	return 0;
}

int rtl837x_vlan_get(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan)
{
	int ret;
	u32 tmp;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	tmp = FIELD_PREP(RTL8373_ITA_CTRL0_TBL_ADDR_MASK, vlan->vid) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_CVLAN) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_READ) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);

	ret = rtl837x_reg_write(priv, RTL8373_ITA_CTRL0_ADDR, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  0, 1000);
	if (ret)
		return ret;

	ret = rtl837x_reg_read(priv, RTL8373_ITA_READ_DATA0_ADDR(0), &tmp);
	if (ret)
		return ret;
	vlan->val = tmp;

	return 0;
}

// 10G reset
static inline int _rtl837x_sds_reset_R(bool is_8224, struct rtl837x_priv *priv, u8 sds_idx)
{
	int ret;
	u16 rx_sts, tmp;
	bool rx_idle, nsq, sync_ok, link_ok, hi_ber; // nsq:(Noise Squelch) hi_ber:(High bit error rate)

	ret = _sds_reg_bits_read(is_8224, priv, sds_idx, 0x20, 0, 0x30, &rx_sts);
	if (ret)
		return ret;

	// check serdes rx status
	// 0,2,3: rx is enabled
	// 1    : rx is disabled
	if (rx_sts == 1) // do nothing when rx is disabled
		return 0;

	// enable rx test
	// switch debug port (What is this?)
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x21, 0x00, BIT(2), 1); // RX_TEST_EN
	if (ret)
		return ret;
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x36, 0x05, 0xf<<11, 8); // REG0_DEBUG_SEL
	if (ret)
		return ret;
	ret = _sds_reg_write(is_8224, priv, sds_idx, 0x1f, 0x02, 0x1f); // switch debug port
	if (ret)
		return ret;

	// Get RX status
	ret = _sds_reg_read(is_8224, priv, sds_idx, 0x1f, 0x15, &tmp);
	if (ret)
		return ret;
	rx_idle = !!((tmp>>7)&1);
	nsq = !!((tmp>>6)&1);

	// Do reset when RX is not idle or RX Noise Squelch
	if (!(nsq==1 || rx_idle==0))
		return 0;

	// check sync_ok
	ret = _sds_reg_read(is_8224, priv, sds_idx, 0x05, 0x00, &tmp);
	if (ret)
		return ret;

	sync_ok = !!(tmp&1);
	link_ok = !!((tmp>>12)&1);
	hi_ber = !!((tmp>>1)&1);

	if (sync_ok == 0)
		goto do_reset;
	else
		if ((link_ok==0) || (hi_ber==1))
			goto do_reset;
	return 0;

do_reset:
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 3);
	if (ret) return ret;
	msleep(1);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 1);
	if (ret) return ret;
	msleep(1);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 3);
	if (ret) return ret;
	msleep(1);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0);
	if (ret) return ret;
	msleep(1);

	return 0;
}

int rtl837x_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx)
{
	return _rtl837x_sds_reset_R(false, priv, sds_idx);
}

int rtl837x_rtl8224_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx)
{
	return _rtl837x_sds_reset_R(true, priv, sds_idx);
}

// 100M/1G/2.5G/5G reset
int rtl837x_sds_reset_X(struct rtl837x_priv *priv, u8 sds_idx)
{
	int ret;
	u16 rx_sts, tmp;
	bool sig_ok, sync_ok, link_ok;

	ret = rtl837x_sds_reg_bits_read(priv, sds_idx, 0x20, 0, 0x30, &rx_sts);
	if (ret)
		return ret;

	// check serdes rx status
	// 0,2,3: rx is enabled
	// 1    : rx is disabled
	if (rx_sts == 1) // do nothing when rx is disabled
		return 0;

	ret = rtl837x_sds_reg_read(priv, sds_idx, 0x01, 0x1d, &tmp);
	if (ret)
		return ret;

	sig_ok = !!((tmp>>8)&1);
	link_ok = !!((tmp>>4)&1);
	sync_ok = !!(tmp&1);

	if (!sig_ok)
		return 0;

	if (sync_ok==0)
		goto do_reset;
	else
		if (link_ok==0)
			goto do_reset;
	return 0;

do_reset:
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 1);
	if (ret) return ret;
	msleep(1);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 0);
	if (ret) return ret;
	msleep(1);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 1);
	if (ret) return ret;
	msleep(1);
	return 0;
}

static inline int rtl837x_fiber_fc_en(struct rtl837x_priv *priv, u8 sds_idx, rtk_sds_mode_t mode, bool fc_en)
{
	int ret;
	switch(mode)
	{
		case SERDES_100FX:
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 5, 0x1<<2, 0x1);
			if (ret) return ret;
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 5, 0x1<<3, 0x1);
			if (ret) return ret;
			if(fc_en)
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 2, 4, 0x3<<10, 0x3);
			else
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 2, 4, 0x3<<10, 0x0);
			if (ret) return ret;
			break;
		case SERDES_1000BASEX:
		case SERDES_2500BASEX:
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 5, 0x1<<2, 0x1);
			if (ret) return ret;
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 5, 0x1<<3, 0x0);
			if (ret) return ret;
			if(fc_en)
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 2, 4, 0x3<<7, 0x3);
			else
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 2, 4, 0x3<<7, 0x0);
			if (ret) return ret;
			break;  
		case SERDES_10GR: 
			if(fc_en)
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 11, 0x3<<2, 0x3);
			else
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 31, 11, 0x3<<2, 0x0);
			if (ret) return ret;
			break; 	 
		default:
			break;  
	}
	return 0;
}

static inline int rtl837x_sds_nway_set(struct rtl837x_priv *priv, u8 sds_idx, rtk_sds_mode_t mode, bool an_en)
{
	int ret;
	switch(mode)
	{
		case SERDES_100FX:
		case SERDES_10GR:			 
			break;
		case SERDES_1000BASEX:
		case SERDES_2500BASEX:
		case SERDES_SG:
		case SERDES_HSG:
			if(an_en)
			{
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0, 2, 0x3<<8, 0x3);
				if (ret) return ret;
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0, 4, 0x1<<2, 0x1);
				if (ret) return ret;
			}
			else
			{
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0, 2, 0x3<<8, 0x1);
				if (ret) return ret;
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0, 4, 0x1<<2, 0x1);
				if (ret) return ret;
			}
			break;  
		case SERDES_10GUSXG:
		case SERDES_10GQXG:
			if(an_en)
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 7, 17, 0xf<<0, 0xf);
			else
				ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 7, 17, 0xf<<0, 0x0);
			if (ret) return ret;	
			break;
		default:
			break;
	}
	return 0;
}

static inline int rtl837x_serdes_patch(struct rtl837x_priv *priv, bool is_8224, u8 sds_idx, rtk_sds_mode_t mode)
{
	int ret;
	const u16 (*an_patch)[3];
	int an_patch_len;
	dev_dbg(priv->dev, "[%s] patch Serdes(%d); is_8224(%d); mode(0x%02X)\n", __func__, sds_idx, is_8224, mode);

	switch (mode)
	{
	case SERDES_10GUSXG:
	case SERDES_10GQXG:
	case SERDES_10GR:
		if ((!is_8224 ? priv->chip_ver == 0 : priv->chip_ver_8224 == 0))
		{
			an_patch = patch_an_10p3125g_a;
			an_patch_len = sizeof(patch_an_10p3125g_a)/(sizeof(u16)*3);
		} else
		{
			an_patch = patch_an_10p3125g_b;
			an_patch_len = sizeof(patch_an_10p3125g_b)/(sizeof(u16)*3);
		}

		break;
	case SERDES_HSG:
	case SERDES_2500BASEX:
		if ((!is_8224 ? priv->chip_ver == 0 : priv->chip_ver_8224 == 0))
		{
			an_patch = patch_an_3p125g_a;
			an_patch_len = sizeof(patch_an_3p125g_a)/(sizeof(u16)*3);
		} else
		{
			an_patch = patch_an_3p125g_b;
			an_patch_len = sizeof(patch_an_3p125g_b)/(sizeof(u16)*3);
		}
		break;
	case SERDES_SG:
	case SERDES_1000BASEX:
		if ((!is_8224 ? priv->chip_ver == 0 : priv->chip_ver_8224 == 0))
		{
			an_patch = patch_an_1p25g_a;
			an_patch_len = sizeof(patch_an_1p25g_a)/(sizeof(u16)*3);
		} else
		{
			an_patch = patch_an_1p25g_b;
			an_patch_len = sizeof(patch_an_1p25g_b)/(sizeof(u16)*3);
		}
		break;
	case SERDES_100FX:
		if ((!is_8224 ? priv->chip_ver == 0 : priv->chip_ver_8224 == 0))
		{
			an_patch = patch_an_125m_a;
			an_patch_len = sizeof(patch_an_125m_a)/(sizeof(u16)*3);
		} else
		{
			an_patch = patch_an_125m_b;
			an_patch_len = sizeof(patch_an_125m_b)/(sizeof(u16)*3);
		}
		break;
	default:
		return 0;
	}

	for(int i = 0; i < an_patch_len; i++){
		ret = _sds_reg_write(is_8224, priv, sds_idx, an_patch[i][0], an_patch[i][1], an_patch[i][2]);
		if (ret) return ret;
	}

	if (is_8224 && mode == SERDES_HSG)
	{
		for (int i = 0; i < sizeof(patch_data_phy)/(sizeof(u16)*3); i++) {
			ret = _sds_reg_write(is_8224, priv, sds_idx, patch_data_phy[i][0], patch_data_phy[i][1], patch_data_phy[i][2]);
			if (ret) return ret;
		}
	} else
	{
		for (int i = 0; i < sizeof(patch_data_mac)/(sizeof(u16)*3); i++) {
			ret = _sds_reg_write(is_8224, priv, sds_idx, patch_data_mac[i][0], patch_data_mac[i][1], patch_data_mac[i][2]);
			if (ret) return ret;
		}
	}

	return 0;
}

static int _set_serdes_mode(struct rtl837x_priv *priv, bool is_8224, u8 sds_idx, rtk_sds_mode_t mode)
{
	int ret;
	
	dev_dbg(priv->dev, "[%s] is_8224: %d\n", __func__, is_8224);
	u32 SDS_USX_SUB_MODE = mode==SERDES_10GQXG ? 2 : 0;

	if(sds_idx == 0)
	{
		ret = rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, 
			   RTL8373_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK, SDS_USX_SUB_MODE);
		if (ret) return ret;
		rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR,
			   RTL8373_SDS_MODE_SEL_SDS0_MODE_SEL_MASK, mode);
		if (ret) return ret;
	}
	else if(sds_idx == 1)
	{
		rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, 
			   RTL8373_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK, SDS_USX_SUB_MODE);
		if (ret) return ret;
		rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR,
			   RTL8373_SDS_MODE_SEL_SDS1_MODE_SEL_MASK, mode);
		if (ret) return ret;
	}

	if (is_8224)
	{
        ret = rtl837x_rtl8224_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC3_8221B_MASK, 0);
		if (ret) return ret;
        ret = rtl837x_rtl8224_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC8_8221B_MASK, 0);
		if (ret) return ret;
		rtl837x_serdes_patch(priv, is_8224, sds_idx, mode);
	} else
	{
		rtl837x_serdes_patch(priv, is_8224, sds_idx, mode);
        ret = rtl837x_fiber_fc_en(priv, sds_idx, mode, true);
		if (ret) return ret;
        ret = rtl837x_sds_nway_set(priv, sds_idx, mode, true);
		if (ret) return ret;
		
	}

	msleep(500);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x3);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x1);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x1);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x3);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x3);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x1);
	if (ret) return ret;
	msleep(5);


	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x1);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x3);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x0);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x3);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x1);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x0);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x1);
	if (ret) return ret;
	msleep(5);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x3);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x0);
	if (ret) return ret;
	msleep(50);


	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x1F, 0x00, 0xffff<<0, 0xB);
	if (ret) return ret;
	msleep(50);
	ret = _sds_reg_bits_write(is_8224, priv, sds_idx, 0x1F, 0x00, 0xffff<<0, 0x0);
	if (ret) return ret;
	msleep(50);

	return 0;
}

int rtl837x_serdes_set_mode(struct rtl837x_priv *priv, u8 sds_idx, rtk_sds_mode_t mode)
{
	int ret;

	switch (mode)
	{
	case SERDES_8221B:
		// TODO (I think this can be managered by linux phy driver)
		return -ENOSYS;
	case SERDES_OFF:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x3);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x1);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x1);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x3);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x3);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x1);
		if (ret) return ret;
		msleep(50);
		break;
	case SERDES_ON:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x1);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x3);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x0);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x3);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x1);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x0);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x1);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x3);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x0);
		if (ret) return ret;
		msleep(50);
		break;
	default:
		if (sds_idx==0)
            ret = rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC3_8221B_MASK, 0);
		else
            ret = rtl837x_reg_bits_write(priv, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC8_8221B_MASK, 0);
		if (ret) return ret;
		msleep(200);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x3);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<4, 0x1);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x1);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<6, 0x3);
		if (ret) return ret;
		msleep(50);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x3);
		if (ret) return ret;
		msleep(5);
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x20, 0x00, 0x3<<10, 0x1);
		if (ret) return ret;
		msleep(50);

		ret = _set_serdes_mode(priv, false, sds_idx, mode);
		if (ret) return ret;
		msleep(50);
		break;
	}

	switch (mode)
	{
		case SERDES_10GQXG:
		case SERDES_10GR:
		case SERDES_10GUSXG:
			dev_dbg(priv->dev, "[%s]Reset Serdes RX R\n", __func__);
			ret = rtl837x_sds_reset_R(priv, sds_idx);
			break;
		default:
			dev_dbg(priv->dev, "[%s]Reset Serdes RX X\n", __func__);
			ret = rtl837x_sds_reset_X(priv, sds_idx);
			break;
	}
	if (ret) return ret;
    msleep(50);

	return 0;
}
