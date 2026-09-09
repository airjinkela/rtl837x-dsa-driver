/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/debugfs.h>
#include <linux/regmap.h>

#include "./rtl837x.h"

#define TO_FOPS(name) _##name##_rw_fops

static int simple_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, inode->i_private);
}

#define MAKE_WRITE_FUNCNAME(_name) _##_name##_rw_write
#define MK_BUFNAME(_name) _buf_rd_##_name
#define MK_BUFLEN(_name) _buf_size_##_name

#define REGRWFUNC(name, BUF_SIZE) \
	static const int MK_BUFLEN(name) = BUF_SIZE; \
	static char MK_BUFNAME(name)[BUF_SIZE];  \
	static ssize_t _##name##_rw_read(struct file *filep, char __user *ubuf,  \
				  size_t count, loff_t *offp)   \
	{   \
		return simple_read_from_buffer(ubuf, count, offp, MK_BUFNAME(name), strlen(MK_BUFNAME(name)));   \
	}   \
	extern ssize_t MAKE_WRITE_FUNCNAME(name)(struct file *filep, const char __user *ubuf,   \
				   size_t count, loff_t *offp);   \
	static const struct file_operations _##name##_rw_fops = {   \
		.owner = THIS_MODULE,   \
		.open = simple_debugfs_open,   \
		.write = MAKE_WRITE_FUNCNAME(name),   \
		.read = _##name##_rw_read   \
	};

#define BUF_APPEND(_bname, fmt, ...) do { \
	size_t _l = strlen(MK_BUFNAME(_bname)); \
	if (_l + 1 < MK_BUFLEN(_bname)) \
		snprintf(MK_BUFNAME(_bname) + _l, MK_BUFLEN(_bname) - _l, \
			 fmt, ##__VA_ARGS__); \
} while (0)

#define BUF_PRINTF(_bname, fmt, ...) do { \
		snprintf(MK_BUFNAME(_bname), MK_BUFLEN(_bname), \
			 fmt, ##__VA_ARGS__); \
} while (0)

REGRWFUNC(vlan, 128)
REGRWFUNC(pvid, 64)
REGRWFUNC(reg, 64)
REGRWFUNC(phyreg_mmd, 64)
REGRWFUNC(phyreg_mii, 64)
REGRWFUNC(phyreg_ocp, 64)
REGRWFUNC(sdsreg, 64)
REGRWFUNC(l2uc, 256)
REGRWFUNC(vlan_trans, 64)
REGRWFUNC(vlan_tag_rewrite, 64)

ssize_t MAKE_WRITE_FUNCNAME(vlan)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 vlan_id;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'r') {
		if(sscanf(buf, "r %d", &vlan_id) != 1) {
			kfree(buf);
			return -EFAULT;
		} else {
			struct rtl837x_vlan_4k vlan4k;
			memset(&vlan4k, 0, sizeof(vlan4k));
			priv->ops->get_vlan_4k(priv, vlan_id,  &vlan4k);
			BUF_PRINTF(vlan, "vid: %d, mbr: 0x%04X, utag: 0x%04X, fid: %d\n",
						  vlan4k.vid, vlan4k.member, vlan4k.untag, vlan4k.fid);
		}
	} else if(buf[0] == 'd') {
		if(sscanf(buf, "d %d", &vlan_id) != 1) {
			kfree(buf);
			return -EFAULT;
		} else {
			struct rtl837x_vlan_4k vlan4k;
			memset(&vlan4k, 0, sizeof(vlan4k));
			priv->ops->get_vlan_4k(priv, vlan_id, &vlan4k);
			BUF_PRINTF(vlan, "vid: %d, mbr: 0x%04X, utag: 0x%04X, fid: %d\n",
						  vlan4k.vid, vlan4k.member, vlan4k.untag, vlan4k.fid);
			vlan4k.member=0;
			vlan4k.untag=0;
			priv->ops->set_vlan_4k(priv, &vlan4k);
		}
	} else {
		BUF_PRINTF(vlan, "echo \"r/d <Dvlan_id>\" > vlan_dump\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(pvid)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	int ret;
	u32 port, pvid;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %d", &port, &pvid) != 2) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 8) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_PB_VLAN_ADDR(port),
				  RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(port), pvid);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %d", &port) != 1) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 8) {
			kfree(buf);
			return -EFAULT;
		}
		ret = rtl837x_reg_bits_read(priv, RTL8373_VLAN_PORT_PB_VLAN_ADDR(port),
				  RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(port), &pvid);
		if (ret) {
			kfree(buf);
			return -EIO;
		}
		BUF_PRINTF(pvid, "port: %d, pvid: %d\n", port, pvid);
	} else {
		BUF_PRINTF(pvid, "echo \"w/r <Dport> [<Dpvid>]\" > pvid\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(sdsreg)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 sds_id, page, reg, val;
	u16 tmp16;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x %x %x", &sds_id, &page, &reg, &val) != 4) {
			kfree(buf);
			return -EFAULT;
		}
		if (sds_id > 1) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_sds_reg_write(priv, sds_id, page, reg, val);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %d %x %x", &sds_id, &page, &reg) != 3) {
			kfree(buf);
			return -EFAULT;
		}
		if (sds_id > 1) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_sds_reg_read(priv, sds_id, page, reg, &tmp16);
		BUF_PRINTF(sdsreg, "sds_id: %d, page: 0x%08x, reg: 0x%08x, val: 0x%08x\n", sds_id, page, reg, tmp16);
	} else {
		BUF_PRINTF(sdsreg, "echo \"w/r <Dsds_id> <Xpage> <Xreg> [<Xval>]\" > sdsreg\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(phyreg_mmd)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 port, devad, reg, val;
	u16 tmp16;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x %x %x", &port, &devad, &reg, &val) != 4) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		priv->ops->phy_write_c45(priv, port, devad, reg, val);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %d %x %x", &port, &devad, &reg) != 3) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		if (priv->ops->phy_read_c45(priv, port, devad, reg, &tmp16)) {
			kfree(buf);
			return -EIO;
		}
		BUF_PRINTF(phyreg_mmd, "port: %d, devad: 0x%08x, reg: 0x%08x, val: 0x%08x\n", port, devad, reg, tmp16);
	} else {
		BUF_PRINTF(phyreg_mmd, "echo \"w/r <Dport> <Xdevad> <Xreg> [<Xval>]\" > phyreg_mmd\n");
	}
	kfree(buf);
	return count;
}


ssize_t MAKE_WRITE_FUNCNAME(phyreg_mii)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 port, reg, val;
	u16 tmp16;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x %x", &port, &reg, &val) != 3) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_phy_write_c22(priv, port, reg, val);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %d %x", &port, &reg) != 2) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		if (rtl837x_phy_read_c22(priv, port, reg, &tmp16)) {
			kfree(buf);
			return -EIO;
		}
		BUF_PRINTF(phyreg_mii, "port: %d, reg: 0x%08x, val: 0x%08x\n", port, reg, tmp16);
	} else {
		BUF_PRINTF(phyreg_mii, "echo \"w/r <Dport> <Xreg> [<Xval>]\" > phyreg_mii\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(phyreg_ocp)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 port, reg, val;
	u16 tmp16;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x %x", &port, &reg, &val) != 3) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_phy_write_c22(priv, port, reg, val);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %d %x", &port, &reg) != 2) {
			kfree(buf);
			return -EFAULT;
		}
		if (port > 9) {
			kfree(buf);
			return -EFAULT;
		}
		if (rtl837x_phy_read_c22(priv, port, reg, &tmp16)) {
			kfree(buf);
			return -EIO;
		}
		BUF_PRINTF(phyreg_ocp, "port: %d, reg: 0x%08x, val: 0x%08x\n", port, reg, tmp16);
	} else {
		BUF_PRINTF(phyreg_ocp, "echo \"w/r <Dport> <Xreg> [<Xval>]\" > phyreg_ocp\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(reg)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	u32 reg, val;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;
	if (*offp)
		return 0;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	if(buf[0] == 'w') {
		if(sscanf(buf, "w %x %x", &reg, &val) != 2) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_reg_write(priv, reg, val);
	} else if(buf[0] == 'r') {
		if(sscanf(buf, "r %x", &reg) != 1) {
			kfree(buf);
			return -EFAULT;
		}
		rtl837x_reg_read(priv, reg, &val);
		BUF_PRINTF(reg, "reg: 0x%08x, val: 0x%08x\n", reg, val);
	} else {
		BUF_PRINTF(reg, "echo \"w/r <Xreg> [<Xval>]\" > reg\n");
	}
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(l2uc)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	int ret;
	u32 index;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;
	struct rtl837x_lut_entry entry = {0};

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	/* reset the static output buffer before generating new result */
	MK_BUFNAME(l2uc)[0] = '\0';
	
	if(buf[0] == 'r') {
		if(sscanf(buf, "r %d", &index) != 1) {
			kfree(buf);
			return -EFAULT;
		}
		entry.addr = index;
		ret = rtl837x_lut_query(priv, LUT_READ_METHOD_ADDRESS, &entry);
		if (ret)
		{
			BUF_APPEND(l2uc, "addr: %d Not Hit\n", index);
			goto out;
		}
		switch (entry.type)
		{
		case LUT_TYPE_L2_UC:
			BUF_APPEND(l2uc, "type:%s ", "l2uc");
			BUF_APPEND(l2uc, "%02X:%02X:%02X:%02X:%02X:%02X ", 
								entry.uc.key.mac_addr[0],
								entry.uc.key.mac_addr[1],
								entry.uc.key.mac_addr[2],
								entry.uc.key.mac_addr[3],
								entry.uc.key.mac_addr[4],
								entry.uc.key.mac_addr[5]
								);
			BUF_APPEND(l2uc, "ivl:%d ", entry.uc.key.ivl);
			BUF_APPEND(l2uc, "vid_fid:%-4d ", entry.uc.key.vid_fid);
			BUF_APPEND(l2uc, "port:%d ", entry.uc.port);
			BUF_APPEND(l2uc, "age:%03d ", entry.uc.age);
			BUF_APPEND(l2uc, "auth:%d ", entry.uc.auth);
			BUF_APPEND(l2uc, "is_static:%d\n", entry.uc.is_static);
			break;
		case LUT_TYPE_L2_MC:
			BUF_APPEND(l2uc, "type:%s ", "l2mc");
			BUF_APPEND(l2uc, "%02X:%02X:%02X:%02X:%02X:%02X ", 
								entry.mc.key.mac_addr[0],
								entry.mc.key.mac_addr[1],
								entry.mc.key.mac_addr[2],
								entry.mc.key.mac_addr[3],
								entry.mc.key.mac_addr[4],
								entry.mc.key.mac_addr[5]
								);
			BUF_APPEND(l2uc, "ivl:%d ", entry.mc.key.ivl);
			BUF_APPEND(l2uc, "vid_fid:%-4d ", entry.mc.key.vid_fid);
			BUF_APPEND(l2uc, "mbr:0x%04X ", entry.mc.mbr);
			BUF_APPEND(l2uc, "igmp_idx:%d ", entry.mc.igmp_idx);
			BUF_APPEND(l2uc, "igmp_asic:%d\n", entry.mc.igmp_asic);
			break;
		case LUT_TYPE_L3:
			BUF_APPEND(l2uc, "type:%s ", "l3");
			BUF_APPEND(l2uc, "sipaddr: 0x%08X", entry.l3.sip);
			BUF_APPEND(l2uc, "dipaddr: 0x%08X", entry.l3.dip);
			BUF_APPEND(l2uc, "mbr:0x%04X ", entry.l3.mbr);
			BUF_APPEND(l2uc, "igmp_idx:%d ", entry.l3.igmp_idx);
			BUF_APPEND(l2uc, "igmp_asic:%d ", entry.l3.igmp_asic);
			BUF_APPEND(l2uc, "l3lookup:%d\n", entry.l3.l3lookup);
			break;
		default:
			break;
		}
	} else if(buf[0] == 'd'){
		if(sscanf(buf, "d %d", &index) != 1) {
			kfree(buf);
			return -EFAULT;
		}
		ret = rtl837x_lut_del(priv, index);
		BUF_APPEND(l2uc, "addr:%d deleted %d\n", index, ret);
	} else {
		BUF_APPEND(l2uc, "echo \"r/d <Dindex>\" > l2uc\n");
	}
out:
	kfree(buf);
	return count;
}

ssize_t MAKE_WRITE_FUNCNAME(vlan_trans)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	int port;
	u32 mbr;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'r') {
		if(sscanf(buf, "r %d", &port) != 1) {
			kfree(buf);
			return -EFAULT;
		} else {
			rtl837x_reg_bits_read(priv, RTL8373_VLAN_PORT_EGR_TRANS_ADDR(port),
				  RTL8373_VLAN_PORT_EGR_TRANS_PMSK_MASK(port), &mbr);
			BUF_PRINTF(vlan_trans, "port: %d, mbr: 0x%04X\n",
						  port, mbr);
		}
	} else if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x", &port, &mbr) != 2) {
			kfree(buf);
			return -EFAULT;
		} else {
			rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_EGR_TRANS_ADDR(port),
				  RTL8373_VLAN_PORT_EGR_TRANS_PMSK_MASK(port), mbr);
			BUF_PRINTF(vlan_trans, "port: %d, mbr: 0x%04X\n",
						  port, mbr);
		}
	} else {
		BUF_PRINTF(vlan_trans, "echo \"r/w <Dport> [<Xval>]\" > vlan_trans\n");
	}
	kfree(buf);
	return count;
}


ssize_t MAKE_WRITE_FUNCNAME(vlan_tag_rewrite)(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	char *buf;
	int port;
	u32 tmp;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	
	if(buf[0] == 'r') {
		if(sscanf(buf, "r %d", &port) != 1) {
			kfree(buf);
			return -EFAULT;
		} else {

			rtl837x_reg_bits_read(priv, RTL8373_VLAN_PORT_EGR_TAG_ADDR(port),
				  RTL8373_VLAN_PORT_EGR_TAG_MODE_MASK(port), &tmp
			);
			const char *stmp = "Unknow";
			switch (tmp)
			{
			case 0:
				stmp = "ORIGINAL(0)";
				break;
			case 1:
				stmp = "KEEP_FORMAT(1)";
				break;
			case 2:
				stmp = "PRI(2)";
				break;
			case 3:
				stmp = "REAL_KEEP(3)";
				break;
			default:
				break;
			}
			BUF_PRINTF(vlan_tag_rewrite, "port: %d, mode: %s\n",
					  port, stmp);
		}
	} else if(buf[0] == 'w') {
		if(sscanf(buf, "w %d %x", &port, &tmp) != 2) {
			kfree(buf);
			return -EFAULT;
		} else {
			rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_EGR_TAG_ADDR(port),
				  RTL8373_VLAN_PORT_EGR_TAG_MODE_MASK(port), tmp
			);
		}
	} else {
		BUF_PRINTF(vlan_tag_rewrite, "echo \"r/w <Dport> [<Xval>]\" > vlan_tag_rewrite\n");
	}
	kfree(buf);
	return count;
}

static ssize_t _sds_page_dump_read(struct file *filep, char __user *ubuf,
				size_t count, loff_t *offp)
{
	int ret, len = 0;
#define SDS_DUMP_BUF_SIZE 2048
	char *buf;
	u16 tmp16;
	u32 tmp32;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = kmalloc(SDS_DUMP_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

#define SDS_DUMP_APPEND(fmt, ...) do { \
		if (len < SDS_DUMP_BUF_SIZE) { \
			int _n = snprintf(buf + len, SDS_DUMP_BUF_SIZE - len, \
					  fmt, ##__VA_ARGS__); \
			if (_n > 0) \
				len += _n; \
			if (len >= SDS_DUMP_BUF_SIZE) \
				len = SDS_DUMP_BUF_SIZE - 1; \
		} \
	} while (0)

	rtl837x_reg_read(priv, RTL8373_SDS_MODE_SEL_ADDR, &tmp32);
	SDS_DUMP_APPEND("reg 0x7b20: %#08x\n", tmp32);
	rtl837x_sds_reg_read(priv, 0, 0x21, 0x10, &tmp16);
	SDS_DUMP_APPEND("sds page 0x21  reg 0x10; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x21, 0x13, &tmp16);
	SDS_DUMP_APPEND("sds page 0x21  reg 0x13; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x21, 0x18, &tmp16);
	SDS_DUMP_APPEND("sds page 0x21  reg 0x18; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x21, 0x1B, &tmp16);
	SDS_DUMP_APPEND("sds page 0x21  reg 0x1b; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x21, 0x1D, &tmp16);
	SDS_DUMP_APPEND("sds page 0x21  reg 0x1d; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x36, 0x1C, &tmp16);
	SDS_DUMP_APPEND("sds page 0x36  reg 0x1c; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x36, 0x14, &tmp16);
	SDS_DUMP_APPEND("sds page 0x36  reg 0x14; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x36, 0x10, &tmp16);
	SDS_DUMP_APPEND("sds page 0x36  reg 0x10; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 4, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x04; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 6, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x06; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 7, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x07; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 9, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x09; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0xB, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x0b; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0xC, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x0c; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0xD, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x0d; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0x15, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x15; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0x16, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x16; data = 0x%04x\n", tmp16);
	rtl837x_sds_reg_read(priv, 0, 0x2E, 0x1D, &tmp16);
	SDS_DUMP_APPEND("sds page 0x2e  reg 0x1d; data = 0x%04x\n", tmp16);

	rtl837x_sds_reg_read(priv, 0, 0x05, 0x00, &tmp16);
	SDS_DUMP_APPEND("sds page 0x05  reg 0x00; data = 0x%04x\n", tmp16);

#undef SDS_DUMP_APPEND

	ret = simple_read_from_buffer(ubuf, count, offp, buf, strlen(buf));
	kfree(buf);
	return ret;
}

static const struct file_operations _sds_page_dump_fops = {
	.owner = THIS_MODULE,
	.open = simple_debugfs_open,
	.read = _sds_page_dump_read
};

static ssize_t _vlan_dump_read(struct file *filep, char __user *ubuf,
			       size_t count, loff_t *offp)
{
	int ret, len = 0;
	char *buf;
	u32 vid;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;
	struct rtl837x_vlan_4k vlan4k;

	sfile = filep->private_data;
	priv = sfile->private;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (vid = 0; vid <= 4095; vid++) {
		memset(&vlan4k, 0, sizeof(vlan4k));
		ret = priv->ops->get_vlan_4k(priv, vid, &vlan4k);
		if (ret)
			continue;

		if (vlan4k.member == 0)
			continue;

		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "vid: %d, mbr: 0x%04X, untag: 0x%04X, fid: %d\n",
				 vlan4k.vid, vlan4k.member, vlan4k.untag, vlan4k.fid);

		if (len >= PAGE_SIZE - 64)
			break;
	}

	ret = simple_read_from_buffer(ubuf, count, offp, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations _vlan_dump_fops = {
	.owner = THIS_MODULE,
	.open = simple_debugfs_open,
	.read = _vlan_dump_read
};


static ssize_t _l2uc_dump_read(struct file *filep, char __user *ubuf,
			       size_t count, loff_t *offp)
{
	int ret;
	struct seq_file *sfile;
	struct rtl837x_priv *priv;
	struct rtl837x_lut_entry entry = {0};

	sfile = filep->private_data;
	priv = sfile->private;

	const int MK_BUFLEN(l2uc_dump) = PAGE_SIZE*64; 
	char *MK_BUFNAME(l2uc_dump) = kmalloc(MK_BUFLEN(l2uc_dump), GFP_KERNEL);

	if (!MK_BUFNAME(l2uc_dump))
		return -ENOMEM;

	for (int i = 0; i < 4160; i++) {
		entry.addr = i;
		ret = rtl837x_lut_query(priv, LUT_READ_METHOD_NEXT_ADDRESS, &entry);
		if (ret)
			break;
		if (entry.addr < i)
			break;

		BUF_APPEND(l2uc_dump, "addr:%-4d ", entry.addr);
		switch (entry.type)
		{
		case LUT_TYPE_L2_UC:
			BUF_APPEND(l2uc_dump, "type:%-4s ", "l2uc");
			BUF_APPEND(l2uc_dump, "%02X:%02X:%02X:%02X:%02X:%02X ", 
								entry.uc.key.mac_addr[0],
								entry.uc.key.mac_addr[1],
								entry.uc.key.mac_addr[2],
								entry.uc.key.mac_addr[3],
								entry.uc.key.mac_addr[4],
								entry.uc.key.mac_addr[5]
								);
			BUF_APPEND(l2uc_dump, "ivl:%d ", entry.uc.key.ivl);
			BUF_APPEND(l2uc_dump, "vid_fid:% -5d ", entry.uc.key.vid_fid);
			BUF_APPEND(l2uc_dump, "port:%d ", entry.uc.port);
			BUF_APPEND(l2uc_dump, "age:%03d ", entry.uc.age);
			BUF_APPEND(l2uc_dump, "auth:%d ", entry.uc.auth);
			BUF_APPEND(l2uc_dump, "is_static:%d\n", entry.uc.is_static);
			break;
		case LUT_TYPE_L2_MC:
			BUF_APPEND(l2uc_dump, "type:%-4s ", "l2mc");
			BUF_APPEND(l2uc_dump, "%02X:%02X:%02X:%02X:%02X:%02X ", 
								entry.mc.key.mac_addr[0],
								entry.mc.key.mac_addr[1],
								entry.mc.key.mac_addr[2],
								entry.mc.key.mac_addr[3],
								entry.mc.key.mac_addr[4],
								entry.mc.key.mac_addr[5]
								);
			BUF_APPEND(l2uc_dump, "ivl:%d ", entry.mc.key.ivl);
			BUF_APPEND(l2uc_dump, "vid_fid:%-4d ", entry.mc.key.vid_fid);
			BUF_APPEND(l2uc_dump, "mbr:0x%04X ", entry.mc.mbr);
			BUF_APPEND(l2uc_dump, "igmp_idx:%d ", entry.mc.igmp_idx);
			BUF_APPEND(l2uc_dump, "igmp_asic:%d\n", entry.mc.igmp_asic);
			break;
		case LUT_TYPE_L3:
			BUF_APPEND(l2uc_dump, "type:%-4s ", "l3");
			BUF_APPEND(l2uc_dump, "sipaddr: 0x%08X", entry.l3.sip);
			BUF_APPEND(l2uc_dump, "dipaddr: 0x%08X", entry.l3.dip);
			BUF_APPEND(l2uc_dump, "mbr:0x%04X ", entry.l3.mbr);
			BUF_APPEND(l2uc_dump, "igmp_idx:%d ", entry.l3.igmp_idx);
			BUF_APPEND(l2uc_dump, "igmp_asic:%d ", entry.l3.igmp_asic);
			BUF_APPEND(l2uc_dump, "l3lookup:%d\n", entry.l3.l3lookup);
			break;
		default:
			break;
		}

		i = entry.addr;
	}

	ret = simple_read_from_buffer(ubuf, count, offp, MK_BUFNAME(l2uc_dump), strlen(MK_BUFNAME(l2uc_dump)));
	kfree(MK_BUFNAME(l2uc_dump));
	return ret;
}

static const struct file_operations _l2uc_dump_fops = {
	.owner = THIS_MODULE,
	.open = simple_debugfs_open,
	.read = _l2uc_dump_read
};

int rtl837x_debug_proc_init(struct rtl837x_priv *priv)
{
	char name[64];
	snprintf(name, 64, "rtl837x-%d-ds", priv->ds->index);
	priv->debugfs_parent = debugfs_create_dir(name, NULL);
	debugfs_create_file("reg", 0600,
			priv->debugfs_parent, priv,
			&TO_FOPS(reg));

	debugfs_create_file("phy_mmd", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(phyreg_mmd));

	debugfs_create_file("phy_mii", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(phyreg_mii));

	debugfs_create_file("phy_ocp", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(phyreg_ocp));

	debugfs_create_file("sdsreg", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(sdsreg));

	debugfs_create_file("vlan", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(vlan));

	debugfs_create_file("pvid", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(pvid));

	debugfs_create_file("vlan_trans", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(vlan_trans));

	debugfs_create_file("vlan_tag_rewrite", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(vlan_tag_rewrite));

	debugfs_create_file("l2uc", 0600,
		priv->debugfs_parent, priv,
		&TO_FOPS(l2uc));

	debugfs_create_file("l2uc_dump", 0400,
		priv->debugfs_parent, priv,
		&_l2uc_dump_fops);

	debugfs_create_file("vlan_dump", 0400,
		priv->debugfs_parent, priv,
		&_vlan_dump_fops);

	debugfs_create_file("sds_page_dump", 0400,
		priv->debugfs_parent, priv,
		&_sds_page_dump_fops);

	return 0;
}

int rtl837x_debug_proc_deinit(struct rtl837x_priv *priv)
{
	debugfs_remove_recursive(priv->debugfs_parent);
	return 0;
}
