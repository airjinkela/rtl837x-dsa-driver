#include <linux/version.h>

#include "rtl837x.h"

#define RTL837X_LUT_TABLE_SIZE         (3)

/*  unicast field define  */
#define RTL837X_L2_UC_D0_MAC5_MSK		GENMASK(7, 0)
#define RTL837X_L2_UC_D0_MAC4_MSK		GENMASK(15, 8)
#define RTL837X_L2_UC_D0_MAC3_MSK		GENMASK(23, 16)
#define RTL837X_L2_UC_D0_MAC2_MSK		GENMASK(31, 24)
#define RTL837X_L2_UC_D1_MAC1_MSK		GENMASK(7, 0)
#define RTL837X_L2_UC_D1_MAC0_MSK		GENMASK(15, 8)

#define RTL837X_L2_UC_D1_CVIDFID_MSK	GENMASK(27, 16)
#define RTL837X_L2_UC_D1_L3LOOKUP_MSK	GENMASK(28, 28)
#define RTL837X_L2_UC_D1_IVL_MSK		GENMASK(29, 29)

#define RTL837X_L2_UC_D1_SPORT_LO_MSK	GENMASK(31, 30)
#define  RTL837X_L2_UC_D2_SPORT_HI_MSK	GENMASK(1, 0)
#define  RTL837X_L2_UC_SPORT_GET(_D1, _D2) \
			((FIELD_GET(RTL837X_L2_UC_D1_SPORT_LO_MSK, _D1))| \
			 (FIELD_GET(RTL837X_L2_UC_D2_SPORT_HI_MSK, _D2) << 2))

#define RTL837X_L2_UC_D2_AGE_MSK		GENMASK(4, 2)
#define RTL837X_L2_UC_D2_AUTH_MSK		GENMASK(5, 5)
#define RTL837X_L2_UC_D2_STATIC_MSK		GENMASK(16, 16)

/*  multicast field define  */
#define RTL837X_L2_MC_D0_MAC5_MSK		GENMASK(7, 0)
#define RTL837X_L2_MC_D0_MAC4_MSK		GENMASK(15, 8)
#define RTL837X_L2_MC_D0_MAC3_MSK		GENMASK(23, 16)
#define RTL837X_L2_MC_D0_MAC2_MSK		GENMASK(31, 24)
#define RTL837X_L2_MC_D1_MAC1_MSK		GENMASK(7, 0)
#define RTL837X_L2_MC_D1_MAC0_MSK		GENMASK(15, 8)

#define RTL837X_L2_MC_D1_CVIDFID_MSK	GENMASK(27, 16)
#define RTL837X_L2_MC_D1_L3LOOKUP_MSK	GENMASK(28, 28)
#define RTL837X_L2_MC_D1_IVL_MSK		GENMASK(29, 29)

#define RTL837X_L2_MC_D1_MBR_LO_MSK	    GENMASK(31, 30)
#define  RTL837X_L2_MC_D2_MBR_HI_MSK	GENMASK(7, 0)
#define  RTL837X_L2_MC_MBR_GET(_D1, _D2) \
            ((FIELD_GET(RTL837X_L2_MC_D1_MBR_LO_MSK, _D1))| \
             (FIELD_GET(RTL837X_L2_MC_D2_MBR_HI_MSK, _D2) << 2))

#define RTL837X_L2_MC_D2_IGMPIDX_MSK	GENMASK(15, 8)
#define RTL837X_L2_MC_D2_IGMPASIC_MSK	GENMASK(16, 16)

/*  L3 field define  */
/* this chip support l3? Interesting */
#define RTL837X_L3_D0_SIP_MSK           GENMASK(31, 0)
#define RTL837X_L3_D1_DIP_MSK           GENMASK(27, 0)
#define RTL837X_L3_D1_L3LOOKUP_MSK      GENMASK(28, 28)

#define RTL837X_L3_D1_MBR_LO_MSK	    GENMASK(31, 30)
#define  RTL837X_L3_D2_MBR_HI_MSK	GENMASK(7, 0)
#define  RTL837X_L3_MBR_GET(_D1, _D2) \
            ((FIELD_GET(RTL837X_L3_D1_MBR_LO_MSK, _D1))| \
             (FIELD_GET(RTL837X_L3_D2_MBR_HI_MSK, _D2) << 2))

#define RTL837X_L3_D2_IGMPIDX_MSK	GENMASK(15, 8)
#define RTL837X_L3_D2_IGMPASIC_MSK	GENMASK(16, 16)

static void rtl837x_l2_data_to_uc(const u32 *data, struct rtl837x_l2_uc *uc)
{
	uc->key.mac_addr[5] = FIELD_GET(RTL837X_L2_UC_D0_MAC5_MSK, data[0]);
	uc->key.mac_addr[4] = FIELD_GET(RTL837X_L2_UC_D0_MAC4_MSK, data[0]);
	uc->key.mac_addr[3] = FIELD_GET(RTL837X_L2_UC_D0_MAC3_MSK, data[0]);
	uc->key.mac_addr[2] = FIELD_GET(RTL837X_L2_UC_D0_MAC2_MSK, data[0]);
	uc->key.mac_addr[1] = FIELD_GET(RTL837X_L2_UC_D1_MAC1_MSK, data[1]);
	uc->key.mac_addr[0] = FIELD_GET(RTL837X_L2_UC_D1_MAC0_MSK, data[1]);

	uc->key.vid_fid = FIELD_GET(RTL837X_L2_UC_D1_CVIDFID_MSK, data[1]);
	// shoule never be set in uc entry
	// uc->l3lookup = FIELD_GET(RTL837X_L2_UC_D1_L3LOOKUP_MSK, data[1]);
	uc->key.ivl = FIELD_GET(RTL837X_L2_UC_D1_IVL_MSK, data[1]);

	uc->port = RTL837X_L2_UC_SPORT_GET(data[1], data[2]);

	uc->age = FIELD_GET(RTL837X_L2_UC_D2_AGE_MSK, data[2]);
	uc->auth = FIELD_GET(RTL837X_L2_UC_D2_AUTH_MSK, data[2]);
	uc->is_static = FIELD_GET(RTL837X_L2_UC_D2_STATIC_MSK, data[2]);
}

static void rtl837x_l2_uc_to_data(const struct rtl837x_l2_uc *uc, u32 *data)
{
	memset(data, 0, RTL837X_LUT_TABLE_SIZE * sizeof(*data));
	data[0] |=
		FIELD_PREP(RTL837X_L2_UC_D0_MAC5_MSK, uc->key.mac_addr[5]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_UC_D0_MAC4_MSK, uc->key.mac_addr[4]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_UC_D0_MAC3_MSK, uc->key.mac_addr[3]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_UC_D0_MAC2_MSK, uc->key.mac_addr[2]);
	data[1] |=
		FIELD_PREP(RTL837X_L2_UC_D1_MAC1_MSK, uc->key.mac_addr[1]);
	data[1] |=
		FIELD_PREP(RTL837X_L2_UC_D1_MAC0_MSK, uc->key.mac_addr[0]);

	data[1] |= 
		FIELD_PREP(RTL837X_L2_UC_D1_CVIDFID_MSK, uc->key.vid_fid);
	data[1] |= 
		FIELD_PREP(RTL837X_L2_UC_D1_IVL_MSK, uc->key.ivl);
	// shoule never be set in uc entry
	data[1] |= 
		FIELD_PREP(RTL837X_L2_UC_D1_L3LOOKUP_MSK, 0);

	data[1] |=
		FIELD_PREP(RTL837X_L2_UC_D1_SPORT_LO_MSK, uc->port & 0x3);
	data[2] |=
		FIELD_PREP(RTL837X_L2_UC_D2_SPORT_HI_MSK, (uc->port >> 2) & 0x3);

	data[2] |=
		FIELD_PREP(RTL837X_L2_UC_D2_STATIC_MSK, uc->is_static);
	data[2] |=
		FIELD_PREP(RTL837X_L2_UC_D2_AGE_MSK, uc->age);
	data[2] |=
		FIELD_PREP(RTL837X_L2_UC_D2_AUTH_MSK, uc->auth);
}


static void rtl837x_l2_data_to_mc(const u32 *data, struct rtl837x_l2_mc *mc)
{
	mc->key.mac_addr[5] = FIELD_GET(RTL837X_L2_MC_D0_MAC5_MSK, data[0]);
	mc->key.mac_addr[4] = FIELD_GET(RTL837X_L2_MC_D0_MAC4_MSK, data[0]);
	mc->key.mac_addr[3] = FIELD_GET(RTL837X_L2_MC_D0_MAC3_MSK, data[0]);
	mc->key.mac_addr[2] = FIELD_GET(RTL837X_L2_MC_D0_MAC2_MSK, data[0]);
	mc->key.mac_addr[1] = FIELD_GET(RTL837X_L2_MC_D1_MAC1_MSK, data[1]);
	mc->key.mac_addr[0] = FIELD_GET(RTL837X_L2_MC_D1_MAC0_MSK, data[1]);

	mc->key.vid_fid = FIELD_GET(RTL837X_L2_MC_D1_CVIDFID_MSK, data[1]);
	// shoule never be set in mc entry
	// mc->l3lookup    = FIELD_GET(RTL837X_L2_MC_D1_L3LOOKUP_MSK, data[1]);
	mc->key.ivl     = FIELD_GET(RTL837X_L2_MC_D1_IVL_MSK, data[1]);

	mc->mbr = RTL837X_L2_MC_MBR_GET(data[1], data[2]);

	mc->igmp_asic = FIELD_GET(RTL837X_L2_MC_D2_IGMPASIC_MSK, data[2]);
	mc->igmp_idx  = FIELD_GET(RTL837X_L2_MC_D2_IGMPIDX_MSK, data[2]);
}

static void rtl837x_l2_mc_to_data(const struct rtl837x_l2_mc *mc, u32 *data)
{
	memset(data, 0, RTL837X_LUT_TABLE_SIZE * sizeof(*data));
	data[0] |=
		FIELD_PREP(RTL837X_L2_MC_D0_MAC5_MSK, mc->key.mac_addr[5]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_MC_D0_MAC4_MSK, mc->key.mac_addr[4]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_MC_D0_MAC3_MSK, mc->key.mac_addr[3]);
	data[0] |=
		FIELD_PREP(RTL837X_L2_MC_D0_MAC2_MSK, mc->key.mac_addr[2]);
	data[1] |=
		FIELD_PREP(RTL837X_L2_MC_D1_MAC1_MSK, mc->key.mac_addr[1]);
	data[1] |=
		FIELD_PREP(RTL837X_L2_MC_D1_MAC0_MSK, mc->key.mac_addr[0]);

	data[1] |= 
		FIELD_PREP(RTL837X_L2_MC_D1_CVIDFID_MSK, mc->key.vid_fid);
	data[1] |= 
		FIELD_PREP(RTL837X_L2_MC_D1_IVL_MSK, mc->key.ivl);

	// shoule never be set in mc entry
	data[1] |= 
		FIELD_PREP(RTL837X_L2_MC_D1_L3LOOKUP_MSK, 0);

	data[1] |=
		FIELD_PREP(RTL837X_L2_MC_D1_MBR_LO_MSK, mc->mbr & 0x3);
	data[2] |=
		FIELD_PREP(RTL837X_L2_MC_D2_MBR_HI_MSK, (mc->mbr >> 2) & 0xFF);

	data[2] |=
		FIELD_PREP(RTL837X_L2_MC_D2_IGMPIDX_MSK, mc->igmp_idx);
	data[2] |=
		FIELD_PREP(RTL837X_L2_MC_D2_IGMPASIC_MSK, mc->igmp_asic);
}

static void rtl837x_data_to_l3(const u32 *data, struct rtl837x_l3 *l3)
{
	l3->sip = FIELD_GET(RTL837X_L3_D0_SIP_MSK, data[0]);
	l3->dip = FIELD_GET(RTL837X_L3_D1_DIP_MSK, data[1]);

	l3->l3lookup = FIELD_GET(RTL837X_L3_D1_L3LOOKUP_MSK, data[1]);

	l3->mbr = RTL837X_L3_MBR_GET(data[1], data[2]);

	l3->igmp_idx = FIELD_GET(RTL837X_L3_D2_IGMPIDX_MSK, data[2]);
	l3->igmp_asic = FIELD_GET(RTL837X_L3_D2_IGMPASIC_MSK, data[2]);
}

static void rtl837x_l3_to_data(const struct rtl837x_l3 *l3, u32 *data)
{
	memset(data, 0, RTL837X_LUT_TABLE_SIZE * sizeof(*data));
	data[0] |=
		FIELD_PREP(RTL837X_L3_D0_SIP_MSK, l3->sip);
	data[1] |=
		FIELD_PREP(RTL837X_L3_D1_DIP_MSK, l3->dip);
	data[1] |=
		FIELD_PREP(RTL837X_L3_D1_L3LOOKUP_MSK, l3->l3lookup);

	data[1] |=
		FIELD_PREP(RTL837X_L3_D1_MBR_LO_MSK, l3->mbr & 0x3);
	data[2] |=
		FIELD_PREP(RTL837X_L3_D2_MBR_HI_MSK, (l3->mbr >> 2) & 0xFF);

	data[2] |=
		FIELD_PREP(RTL837X_L3_D2_IGMPIDX_MSK, l3->igmp_idx);
	data[2] |=
		FIELD_PREP(RTL837X_L3_D2_IGMPASIC_MSK, l3->igmp_asic);
}

// TODO: we need a lock to make sure the ITA read/write is atomic

int rtl837x_lut_query(struct rtl837x_priv *priv, 
                          enum rtl837x_l2_method method,
                          struct rtl837x_lut_entry *entry)
{
	int ret;
	u32 tmp, cmd;
	u32 tb_data[RTL837X_LUT_TABLE_SIZE] = {0};

	// check busy
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	ret = rtl837x_reg_bits_write(priv, RTL8373_ITA_L2_CTRL_ADDR,
					  RTL8373_ITA_L2_CTRL_READ_MTHD_MASK, method);
	if (ret)
		return ret;

	switch (method)
	{
	case LUT_READ_METHOD_ADDRESS:
	case LUT_READ_METHOD_NEXT_ADDRESS:
	case LUT_READ_METHOD_NEXT_L2UC:
	case LUT_READ_METHOD_NEXT_L2MC:
	case LUT_READ_METHOD_NEXT_L3MC:
	case LUT_READ_METHOD_NEXT_L2L3MC:
		cmd = FIELD_PREP(RTL8373_ITA_CTRL0_TBL_ADDR_MASK, entry->addr) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_L2) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_READ) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);
		break;
	case LUT_READ_METHOD_MAC:
		rtl837x_l2_uc_to_data(&entry->uc, tb_data);
		for(int i=0; i<RTL837X_LUT_TABLE_SIZE; i++)
		{
			ret = rtl837x_reg_write(priv, RTL8373_ITA_WRITE_DATA0_ADDR(i), tb_data[i]);
			if (ret)
				return ret;
		}
		cmd = FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_L2) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_READ) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);
		break;
	case LUT_READ_METHOD_NEXT_L2UCSPA:
		ret = rtl837x_reg_bits_write(priv, RTL8373_ITA_CTRL0_ADDR,
					  RTL8373_ITA_CTRL0_TBL_ADDR_MASK, entry->addr);
		if (ret)
			return ret;
		ret = rtl837x_reg_bits_write(priv, RTL8373_ITA_L2_CTRL_ADDR,
					  RTL8373_ITA_L2_CTRL_PORT_NUM_MASK, entry->uc.port);
		if (ret)
			return ret;
		cmd = FIELD_PREP(RTL8373_ITA_CTRL0_TBL_ADDR_MASK, entry->addr) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_L2) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_READ) |
			  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);
		break;
	default:
		return -EINVAL;
	}

	// execute the table cmd
	ret = rtl837x_reg_write(priv, RTL8373_ITA_CTRL0_ADDR, cmd);
	if (ret)
		return ret;

	// wait busy flag
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	// check did the query hit a entry
	ret = rtl837x_reg_bits_read(priv, RTL8373_ITA_L2_CTRL_ADDR,
				  RTL8373_ITA_L2_CTRL_ACT_STS_MASK, &tmp);
	if (ret)
		return ret;
	if (!tmp)
		return -ENOENT; // not hit

	ret = rtl837x_reg_bits_read(priv, RTL8373_ITA_L2_CTRL_ADDR,
				  RTL8373_ITA_L2_CTRL_TBL_ADDR_MASK, &tmp);
	if (ret)
		return ret;
	entry->addr = tmp;

	// read the table data
	for (int i=0; i<RTL837X_LUT_TABLE_SIZE; i++)
	{
		ret = rtl837x_reg_read(priv, RTL8373_ITA_READ_DATA0_ADDR(i), &tb_data[i]);
		if (ret)
			return ret;
	}

	// Get entry type
	if (FIELD_GET(RTL837X_L3_D1_L3LOOKUP_MSK, tb_data[1]))
		entry->type = LUT_TYPE_L3;
	else if (FIELD_GET(RTL837X_L2_MC_D1_MAC0_MSK, tb_data[1]) & 1) // Check mac0 bit0
		entry->type = LUT_TYPE_L2_MC;
	else
		entry->type = LUT_TYPE_L2_UC;

	switch (entry->type)
	{
	case LUT_TYPE_L2_UC:
		rtl837x_l2_data_to_uc(tb_data, &(entry->uc));
		break;
	case LUT_TYPE_L2_MC:
		rtl837x_l2_data_to_mc(tb_data, &(entry->mc));
		break;
	case LUT_TYPE_L3:
		rtl837x_data_to_l3(tb_data, &(entry->l3));
		break;
	}

	return 0;
}

/*
	something interesting:
	if  multicast macaddress(mac[0] bit0 == 1) is set in the unicast entry
	the hardware won't record this entry in the lut table
	it will throw an error (-ENOENT)
*/
int rtl837x_lut_set(struct rtl837x_priv *priv, 
                          struct rtl837x_lut_entry *entry)
{
	int ret;
	u32 tmp, cmd;
	u32 tb_data[RTL837X_LUT_TABLE_SIZE] = {0};

	// check busy
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	switch (entry->type)
	{
	case LUT_TYPE_L2_UC:
		rtl837x_l2_uc_to_data(&entry->uc, tb_data);
		break;
	case LUT_TYPE_L2_MC:
		rtl837x_l2_mc_to_data(&entry->mc, tb_data);
		break;
	case LUT_TYPE_L3:
		rtl837x_l3_to_data(&entry->l3, tb_data);
		break;
	default:
		return -EINVAL;
	}

	for (int i=0; i<RTL837X_LUT_TABLE_SIZE; i++)
	{
		ret = rtl837x_reg_write(priv, RTL8373_ITA_WRITE_DATA0_ADDR(i), tb_data[i]);
		if (ret)
			return ret;
	}

	cmd = FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_L2) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_WRITE) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);

	// execute the table cmd
	ret = rtl837x_reg_write(priv, RTL8373_ITA_CTRL0_ADDR, cmd);
	if (ret)
		return ret;

	// wait busy flag
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	// check did the query hit a entry
	ret = rtl837x_reg_bits_read(priv, RTL8373_ITA_L2_CTRL_ADDR,
				  RTL8373_ITA_L2_CTRL_ACT_STS_MASK, &tmp);
	if (ret)
		return ret;
	if (!tmp)
		return -ENOENT; // not hit

	ret = rtl837x_reg_bits_read(priv, RTL8373_ITA_L2_CTRL_ADDR,
				  RTL8373_ITA_L2_CTRL_TBL_ADDR_MASK, &tmp);
	if (ret)
		return ret;
	entry->addr = tmp;

	return 0;
}

int rtl837x_lut_del(struct rtl837x_priv *priv, 
                          u32 addr)
{
	int ret;
	u32 tmp, cmd;

	// check busy
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	ret = rtl837x_reg_bits_write(priv, RTL8373_ITA_L2_CTRL_ADDR,
						  RTL8373_ITA_L2_CTRL_ENTRY_CLR_MASK, 1);
	if (ret)
		return ret;

	cmd = FIELD_PREP(RTL8373_ITA_CTRL0_TBL_ADDR_MASK, addr) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_L2) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_ACT_MASK, TB_OP_WRITE) |
		  FIELD_PREP(RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK, TB_EXECUTE);

	// execute the table cmd
	ret = rtl837x_reg_write(priv, RTL8373_ITA_CTRL0_ADDR, cmd);
	if (ret)
		return ret;

	// wait busy flag
	ret = regmap_read_poll_timeout(priv->map, RTL8373_ITA_CTRL0_ADDR, tmp,
		  ((tmp & RTL8373_ITA_CTRL0_TLB_EXECUTE_MASK) == 0),
		  10, 10000);
	if (ret)
		return ret;

	ret = rtl837x_reg_bits_write(priv, RTL8373_ITA_L2_CTRL_ADDR,
						  RTL8373_ITA_L2_CTRL_ENTRY_CLR_MASK, 0);

	return ret;
}
