/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/gpio/consumer.h>

#include "rtl837x.h"

static int rtl837x_mdio_write(void *ctx, u32 reg, u32 val)
{
	struct rtl837x_priv *priv = ctx;
	struct mii_bus *bus = priv->bus;
	int ret;

	mutex_lock(&bus->mdio_lock);

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_ADDR_REG, reg);
	if (unlikely(ret))
		goto out_unlock;

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_DATA_LOW, (val & 0xFFFF));
	if (unlikely(ret))
		goto out_unlock;

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_DATA_HIGH, ((val >> 16) & 0xFFFF));
	if (unlikely(ret))
		goto out_unlock;

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_CTRL_REG, MDC_MDIO_WRITE_CMD);
	if (unlikely(ret))
		goto out_unlock;

	ret = 0;
out_unlock:
	mutex_unlock(&bus->mdio_lock);
	// printk("rtl837x_mdio_write ret:%d\n", ret);
	return ret;
}

static int rtl837x_mdio_read(void *ctx, u32 reg, u32 *val)
{
	struct rtl837x_priv *priv = ctx;
	struct mii_bus *bus = priv->bus;
	int ret, val_l, val_h;

	mutex_lock(&bus->mdio_lock);

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_ADDR_REG, reg);
	if (unlikely(ret))
		goto out_unlock;

	ret = bus->write(bus, priv->mdio_addr, MDC_MDIO_CTRL_REG, MDC_MDIO_READ_CMD);
	if (unlikely(ret))
		goto out_unlock;

	val_l = bus->read(bus, priv->mdio_addr, MDC_MDIO_DATA_LOW);
	val_h = bus->read(bus, priv->mdio_addr, MDC_MDIO_DATA_HIGH);

    *val = val_l & 0xffff;
    *val |= (val_h & 0xffff) << 16;
	ret = 0;

out_unlock:
	mutex_unlock(&bus->mdio_lock);
	// printk("rtl837x_mdio_read ret:%d\n", ret);
	return ret;
}

static void rtl837x_mdio_lock(void *ctx)
{
	struct rtl837x_priv *priv = ctx;
	mutex_lock(&priv->map_lock);
}

static void rtl837x_mdio_unlock(void *ctx)
{
	struct rtl837x_priv *priv = ctx;
	mutex_unlock(&priv->map_lock);
}

static int rtl837x_rtl8224_write(void *ctx, u32 reg, u32 val)
{
	struct rtl837x_priv *priv = ctx;
	int ret;
	u16 vall = val & 0xffff;
	u16 valh = (val >> 16) & 0xffff;

	ret = rtl837x_phy_write_c45(priv, 0, 30, reg, vall);
	if (ret)
		return ret;

	ret = rtl837x_phy_write_c45(priv, 0, 30, reg+1, valh);
	if (ret)
		return ret;

	return 0;
}

static int rtl837x_rtl8224_read(void *ctx, u32 reg, u32 *val)
{
	struct rtl837x_priv *priv = ctx;
	int ret;
	u16 vall, valh;

	ret = rtl837x_phy_read_c45(priv, 0, 30, reg, &vall);
	if (ret)
		return ret;
	ret = rtl837x_phy_read_c45(priv, 0, 30, reg+1, &valh);
	if (ret)
		return ret;
	*val = (vall & 0xffff) | ((valh &0xffff) << 16);

	return 0;
}

static void rtl837x_rtl8224_lock(void *ctx)
{
	struct rtl837x_priv *priv = ctx;
	mutex_lock(&priv->map_8224_lock);
}
static void rtl837x_rtl8224_unlock(void *ctx)
{
	struct rtl837x_priv *priv = ctx;
	mutex_unlock(&priv->map_8224_lock);
}

static const struct regmap_config rtl837x_mdio_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
	.reg_stride = 4,

	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.reg_read = rtl837x_mdio_read,
	.reg_write = rtl837x_mdio_write,
	.cache_type = REGCACHE_NONE,
	.lock = rtl837x_mdio_lock,
	.unlock = rtl837x_mdio_unlock,
};

static const struct regmap_config rtl837x_rtl8224_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
	.reg_stride = 4,

	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.reg_read = rtl837x_rtl8224_read,
	.reg_write = rtl837x_rtl8224_write,
	.cache_type = REGCACHE_NONE,
	.lock = rtl837x_rtl8224_lock,
	.unlock = rtl837x_rtl8224_unlock,
};

static int rtl837x_mdio_probe(struct mdio_device *mdiodev)
{
	struct rtl837x_priv *priv;
	struct device *dev = &mdiodev->dev;
	struct device_node *np = dev->of_node;
	const struct rtl837x_variant *var;
	struct regmap_config rc;
	int ret;

	var = of_device_get_match_data(dev);
	if (!var){
		ret = -EINVAL;
		goto err;
	}
	
	priv = devm_kzalloc(dev,
				size_add(sizeof(*priv), var->chip_data_sz),
				GFP_KERNEL);
	if (!priv){
		ret = -ENOMEM;
		goto err;
	}

	mutex_init(&priv->map_lock);
	
	rc = rtl837x_mdio_regmap_config;
	rc.lock_arg = priv;
	priv->map = devm_regmap_init(dev, NULL, priv, &rc);
	if (IS_ERR(priv->map)) {
		ret = PTR_ERR(priv->map);
		dev_err(dev, "regmap init failed: %d\n", ret);
		goto err;
	}

	if (var->have_8224)
	{
		mutex_init(&priv->map_8224_lock);

		rc = rtl837x_rtl8224_regmap_config;
		priv->map_8224 = devm_regmap_init(dev, NULL, priv, &rc);
		if (IS_ERR(priv->map_8224)) {
			ret = PTR_ERR(priv->map_8224);
			dev_err(dev, "regmap 8224 init failed: %d\n", ret);
			goto err;
		}
	}else
	{
		priv->map_8224 = NULL;
	}

	priv->mdio_addr = mdiodev->addr;
	priv->bus = mdiodev->bus;
	priv->dev = dev;
	priv->chip_data = (void *)priv + sizeof(*priv);
	priv->tag_proto = var->def_tag_proto;

	priv->ops = var->ops;

	priv->write_reg_noack = rtl837x_mdio_write;


	dev_set_drvdata(dev, priv);

	priv->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(priv->reset)) {
		dev_err(dev, "failed to get RESET GPIO\n");
		ret = PTR_ERR(priv->reset);
		goto err;
	}

	if (priv->reset) {
		// This chip takes a long time to fully reset
		dev_dbg(dev, "asserted RESET\n");
		gpiod_set_value_cansleep(priv->reset, 0);
		usleep_range(50000, 51100);
		gpiod_set_value_cansleep(priv->reset, 1);
		usleep_range(50000, 51100);
		dev_dbg(dev, "deasserted RESET\n");
	}

	ret = priv->ops->detect(priv);
	if (ret) {
		dev_err(dev, "unable to detect switch\n");
		goto err;
	}

	ret = rtl837x_reg_bits_write(priv, RTL8373_CHIP_INFO_ADDR,
			  RTL8373_CHIP_INFO_CHIP_INFO_EN_MASK, 0xa);
	if (ret) return ret;
	ret = rtl837x_reg_bits_read(priv, RTL8373_CHIP_INFO_ADDR,
			  RTL8373_CHIP_INFO_RL_VID_MASK, &priv->chip_ver);
	if (ret) return ret;
	ret = rtl837x_reg_bits_write(priv, RTL8373_CHIP_INFO_ADDR,
			  RTL8373_CHIP_INFO_CHIP_INFO_EN_MASK, 0);
	if (ret) return ret;
	dev_dbg(priv->dev, "[%s] chip ver: %d\n", __func__, priv->chip_ver);

	if (var->have_8224)
	{
		ret = rtl837x_rtl8224_reg_bits_write(priv, RTL8373_CHIP_INFO_ADDR,
				  RTL8373_CHIP_INFO_CHIP_INFO_EN_MASK, 0xa);
		if (ret) return ret;
		ret = rtl837x_rtl8224_reg_bits_read(priv, RTL8373_CHIP_INFO_ADDR,
				  RTL8373_CHIP_INFO_RL_VID_MASK, &priv->chip_ver_8224);
		if (ret) return ret;
		ret = rtl837x_rtl8224_reg_bits_write(priv, RTL8373_CHIP_INFO_ADDR,
				  RTL8373_CHIP_INFO_CHIP_INFO_EN_MASK, 0);
		if (ret) return ret;
		dev_dbg(priv->dev, "[%s] chip ver 8224: %d\n", __func__, priv->chip_ver_8224);
	}

	mutex_init(&priv->mib_lock);

	priv->ds = devm_kzalloc(dev, sizeof(*priv->ds), GFP_KERNEL);
	if (!priv->ds){
		ret =  -ENOMEM;
		goto err;
	}

	priv->ds->dev = dev;
	priv->ds->num_ports = priv->num_ports;
	priv->ds->priv = priv;
	priv->ds->ops = var->ds_ops_mdio;
	if (var->pl_mac_ops)
		priv->ds->phylink_mac_ops = var->pl_mac_ops;
	
	ret = dsa_register_switch(priv->ds);
	if (ret) {
		dev_err(priv->dev, "unable to register switch ret = %d\n", ret);
		goto err;
	}

#ifdef CONFIG_GPIOLIB
	if (of_property_read_bool(np, "gpio-controller"))
	{
		ret = rtl837x_gpiochip_init(priv);
		if (ret) 
			dev_err(priv->dev, "Failed to register gpiochip. ret = %d\n", ret);
	}
#endif /* CONFIG_GPIOLIB */

	rtl837x_debug_proc_init(priv);

	return 0;
err:
	return ret;
}

static void rtl837x_mdio_remove(struct mdio_device *mdiodev)
{
	struct rtl837x_priv *priv = dev_get_drvdata(&mdiodev->dev);

	if (!priv)
		return;

	rtl837x_debug_proc_deinit(priv);

	if (priv->ds->tag_8021q_ctx) {
		rtnl_lock();
		dsa_tag_8021q_unregister(priv->ds);
		rtnl_unlock();
	}

	dsa_unregister_switch(priv->ds);
	
	/* leave the device reset asserted */
	if (priv->reset)
		gpiod_set_value(priv->reset, 1);
}

static void rtl837x_mdio_shutdown(struct mdio_device *mdiodev)
{
	struct rtl837x_priv *priv = dev_get_drvdata(&mdiodev->dev);

	if (!priv)
		return;

	dsa_switch_shutdown(priv->ds);

	dev_set_drvdata(&mdiodev->dev, NULL);
}

static const struct of_device_id rtk_mdio_match[] = {
	{ .compatible = "realtek,rtl8372n", .data = &rtl8372n_variant},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_mdio_match);

static struct mdio_driver rtl837x_mdio_driver = {
	.mdiodrv.driver = {
		.name = "rtl837x-mdio",
		.of_match_table = rtk_mdio_match,
	},
	.probe  = rtl837x_mdio_probe,
	.remove = rtl837x_mdio_remove,
	.shutdown = rtl837x_mdio_shutdown,
};

mdio_module_driver(rtl837x_mdio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("StarField Xu <air_jinkela@163.com>");
MODULE_DESCRIPTION("rtl8372n switch driver for MT7988");
