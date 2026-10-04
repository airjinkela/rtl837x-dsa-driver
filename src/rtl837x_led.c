// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include "rtl837x.h"

#define RTL837X_LED_GLB_IO_EN_ADDR    0x65DC
#define RTL837X_LED_GLB_MUX_ADDR(_led_pin) \
	    (0x65E0+((_led_pin/5)*4))
#define   RTL837X_LED_GLB_MUX_LEDx_MUX_MASK(_led_pin) \
	    (0x3F<<((_led_pin%5)*6))

#define RTL837X_PORT_LED_SET_SEL_ADDR        0x654C
#define   RTL837X_PORT_LED_SET_SEL_MASK(_p)  (0x3<<(_p<<1))

#define RTL837X_LED_SETx_SEL0_LEDx_ADDR(_led_set, _led_id) \
	    (0x652C+(((3-_led_set)*8)+((1-(_led_id/2))*4)))
#define RTL837X_LED_SETx_SEL0_LEDx_MASK(_led_id) \
	    (0xFFFF << (_led_id&1)*16)

#define RTL837X_LED_SETx_SEL1_LEDx_ADDR(_led_set) \
	    (0x6524+((1-(_led_set/2))*4))
#define RTL837X_LED_SETx_SEL1_LEDx_MASK(_led_set, _led_id) \
	    (0xF << (((_led_set&1)*16)+(_led_id*4)))

/*
 * Highest LED pad number this driver is willing to drive. Pads 0..27 are the
 * plain LED/GPIO pads; pads 28 and 29 double as SYS_LED_EN and
 * GLB_RLDP_LED_EN inside IO_MUX_SEL_0, so they are not usable as ordinary
 * LEDs here.
 */
#define RTL837X_LED_PIN_MAX  28

// SELx_0
#define RTL837X_LED_LINK_LINK_EN_MASK   BIT(6)
#define RTL837X_LED_LINK_10M_EN_MASK    BIT(5)
#define RTL837X_LED_LINK_100M_EN_MASK   BIT(4)
#define RTL837X_LED_LINK_1000M_EN_MASK  BIT(2)
#define RTL837X_LED_LINK_2500M_EN_MASK  BIT(0)
#define RTL837X_LED_LINK_ACT_EN_MASK    BIT(8)
#define RTL837X_LED_LINK_RX_EN_MASK     BIT(9)
#define RTL837X_LED_LINK_TX_EN_MASK     BIT(10)

// SELx_1
#define RTL837X_LED_LINK_5000M_EN_MASK  (BIT(2)<<16)
#define RTL837X_LED_LINK_10000M_EN_MASK (BIT(0)<<16)

#define RTL837X_LED_PORT_INDEX(_p, _l) \
		    ((_p*RTL837X_PORT_LED_COUNT)+_l)

#define RTL837X_LED_LINK_MASK \
	    (RTL837X_LED_LINK_10M_EN_MASK | \
	     RTL837X_LED_LINK_100M_EN_MASK | \
	     RTL837X_LED_LINK_1000M_EN_MASK | \
	     RTL837X_LED_LINK_2500M_EN_MASK | \
	     RTL837X_LED_LINK_5000M_EN_MASK | \
	     RTL837X_LED_LINK_10000M_EN_MASK)

static inline void led_set_refinc(struct rtl837x_led_set *led_set)
{
	atomic_inc(&led_set->refcnt);
}

static inline void led_set_refdec(struct rtl837x_led_set *led_set)
{
	/*
	 * The last user is gone, so the masks no longer describe anything:
	 * clear them here so that an idle set is always seen as all-zero
	 * (rtl837x_match_same_led_set() relies on that when probing for a
	 * matching set).
	 */
	if (atomic_dec_and_test(&led_set->refcnt))
		memset(led_set->led_cfg_mask, 0, sizeof(led_set->led_cfg_mask));
}

static inline int led_set_refread(struct rtl837x_led_set *led_set)
{
	return atomic_read(&led_set->refcnt);
}

static bool _match_led_cfg(struct rtl837x_led_set *led_set,
	       struct rtl837x_led_set *port_led_set,
		   u8 led_id, u32 offload_trigger)
{
	for (int i = 0; i < RTL837X_PORT_LED_COUNT; i++) {
		if (led_id == i) {
			if (led_set->led_cfg_mask[i] == offload_trigger)
				continue;
		} else {
			if (led_set->led_cfg_mask[i] == port_led_set->led_cfg_mask[i])
				continue;
		}

		return false;
	}
	return true;
}

static struct rtl837x_led_set *rtl837x_match_same_led_set(
	    struct rtl837x_led *port_led,
	    struct rtl837x_led_set *port_led_set,
		u32 offload_trigger)
{
	struct rtl837x_priv *priv = port_led->priv;

	struct rtl837x_led_set *led_set;

	for (int set_idx = 0; set_idx < RTL837X_LED_SET_COUNT; set_idx++) {
		led_set = &(priv->led_set[set_idx]);
		if (led_set_refread(led_set) == 0)
			continue;
		if (_match_led_cfg(led_set, port_led_set, port_led->led_id, offload_trigger))
			return led_set;
	}
	return NULL;
}

static struct rtl837x_led_set *rtl837x_get_free_led_set(struct rtl837x_led *port_led)
{
	struct rtl837x_priv *priv = port_led->priv;
	struct rtl837x_led_set *led_set;

	for (int set_idx = 0; set_idx < RTL837X_LED_SET_COUNT; set_idx++) {
		led_set = &(priv->led_set[set_idx]);
		if (led_set_refread(led_set) == 0)
			return led_set;
	}
	return NULL;
}

static int rtl837x_apply_led_set(struct rtl837x_led_set *led_set)
{
	int ret;
	struct rtl837x_priv *priv = led_set->priv;

	for (int i = 0; i < RTL837X_PORT_LED_COUNT; i++) {
		ret = rtl837x_reg_bits_write(priv, RTL837X_LED_SETx_SEL0_LEDx_ADDR(led_set->idx, i),
					RTL837X_LED_SETx_SEL0_LEDx_MASK(i),
					led_set->led_cfg_mask[i] & 0xffff
				);
		if (ret)
			return ret;
		ret = rtl837x_reg_bits_write(priv, RTL837X_LED_SETx_SEL1_LEDx_ADDR(led_set->idx),
					RTL837X_LED_SETx_SEL1_LEDx_MASK(led_set->idx, i),
					(led_set->led_cfg_mask[i] >> 16) & 0xf
				);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtl837x_apply_port_led_set(struct rtl837x_led *port_led)
{
	struct rtl837x_priv *priv = port_led->priv;

	return rtl837x_reg_bits_write(priv, RTL837X_PORT_LED_SET_SEL_ADDR,
			    RTL837X_PORT_LED_SET_SEL_MASK(port_led->port_num),
			    port_led->led_set->idx
			);
}

static int rtl837x_set_port_led_hw_offload_trigger(struct rtl837x_led *port_led, u32 offload_trigger)
{
	struct rtl837x_priv *priv = port_led->priv;
	int ret;
	struct rtl837x_led_set *led_set;

	if (port_led->led_set == NULL) {
		struct rtl837x_led_set port_led_set = {0};

		led_set = rtl837x_match_same_led_set(port_led, &port_led_set, offload_trigger);
		if (led_set == NULL)
			led_set = rtl837x_get_free_led_set(port_led);
		if (led_set == NULL)
			return -ENOSPC;
		led_set_refinc(led_set);
		led_set->led_cfg_mask[port_led->led_id] = offload_trigger;
		ret = rtl837x_apply_led_set(led_set);
		if (ret)
			return ret;
		port_led->led_set = led_set;
		ret = rtl837x_apply_port_led_set(port_led);
		if (ret)
			return ret;
		return 0;
	}

	if (led_set_refread(port_led->led_set) == 1) {
		struct rtl837x_led_set *other;
		int i;

		led_set = port_led->led_set;
		led_set->led_cfg_mask[port_led->led_id] = offload_trigger;

		/*
		 * The new mask may now be identical to another set: in that case
		 * move this LED over and release the old one. Without this an LED
		 * keeps the set it grabbed while the trigger was still being
		 * configured attribute by attribute, so equal configurations
		 * never end up sharing a set.
		 */
		for (i = 0; i < RTL837X_LED_SET_COUNT; i++) {
			other = &priv->led_set[i];
			if (other == led_set || led_set_refread(other) == 0)
				continue;
			if (memcmp(other->led_cfg_mask, led_set->led_cfg_mask,
				   sizeof(led_set->led_cfg_mask)))
				continue;

			led_set_refinc(other);
			led_set_refdec(led_set);
			port_led->led_set = other;
			/* Hardware already matches; only the port mapping changes */
			return rtl837x_apply_port_led_set(port_led);
		}

		ret = rtl837x_apply_led_set(led_set);
		if (ret)
			return ret;
		return 0;
	}

	led_set = rtl837x_match_same_led_set(port_led, port_led->led_set, offload_trigger);
	if (led_set == NULL)
		led_set = rtl837x_get_free_led_set(port_led);
	if (led_set == NULL)
		return -ENOSPC;
	led_set_refinc(led_set);
	led_set_refdec(port_led->led_set);
	led_set->led_cfg_mask[port_led->led_id] = offload_trigger;
	ret = rtl837x_apply_led_set(led_set);
	if (ret)
		return ret;
	port_led->led_set = led_set;
	ret = rtl837x_apply_port_led_set(port_led);
	if (ret)
		return ret;
	return 0;
}

static int rtl837x_parse_netdev(unsigned long rules, u32 *offload_trigger)
{
	/* Parsing specific to netdev trigger */
	if (test_bit(TRIGGER_NETDEV_LINK, &rules))
		*offload_trigger |= RTL837X_LED_LINK_LINK_EN_MASK | RTL837X_LED_LINK_MASK;
	if (test_bit(TRIGGER_NETDEV_RX, &rules))
		*offload_trigger |= RTL837X_LED_LINK_RX_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_TX, &rules))
		*offload_trigger |= RTL837X_LED_LINK_TX_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_10, &rules))
		*offload_trigger |= RTL837X_LED_LINK_10M_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_100, &rules))
		*offload_trigger |= RTL837X_LED_LINK_100M_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_1000, &rules))
		*offload_trigger |= RTL837X_LED_LINK_1000M_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_2500, &rules))
		*offload_trigger |= RTL837X_LED_LINK_2500M_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_5000, &rules))
		*offload_trigger |= RTL837X_LED_LINK_5000M_EN_MASK;
	if (test_bit(TRIGGER_NETDEV_LINK_10000, &rules))
		*offload_trigger |= RTL837X_LED_LINK_10000M_EN_MASK;

	if ((*offload_trigger & RTL837X_LED_LINK_RX_EN_MASK) &&
		(*offload_trigger & RTL837X_LED_LINK_TX_EN_MASK))
		*offload_trigger |= RTL837X_LED_LINK_ACT_EN_MASK;

	if (rules && !*offload_trigger)
		return -EOPNOTSUPP;

	return 0;
}

static int rtl837x_set_led_hw_offload(struct rtl837x_led *port_led, bool enable)
{
	int ret;
	struct rtl837x_priv *priv = port_led->priv;

	port_led->is_hw_offload = enable;

	if (enable) {

		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_0_ADDR,
			    BIT(port_led->led_pin), 0);
		if (ret)
			return ret;
		ret = rtl837x_reg_bits_write(priv, RTL837X_LED_GLB_IO_EN_ADDR,
			    BIT(port_led->led_pin), 1);
	} else {
		/*
		 * Back to GPIO control: this LED no longer references its set,
		 * so drop the reference (and the pointer) to let it be recycled.
		 */
		if (port_led->led_set) {
			led_set_refdec(port_led->led_set);
			port_led->led_set = NULL;
		}

		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_0_ADDR,
			    BIT(port_led->led_pin), 1);
		if (ret)
			return ret;
		ret = rtl837x_reg_bits_write(priv, RTL837X_LED_GLB_IO_EN_ADDR,
			    BIT(port_led->led_pin), 0);
		if (ret)
			return ret;
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OE0_ADDR,
				BIT(port_led->led_pin), 1);
	}

	return ret;
}

static int rtl837x_led_set_brightness(struct rtl837x_led *port_led,
			 enum led_brightness brightness)
{
	int ret;
	struct rtl837x_priv *priv = port_led->priv;

	ret = rtl837x_set_led_hw_offload(port_led, false);
	if (brightness)
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OUT0_ADDR,
				BIT(port_led->led_pin), 1);
	else
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OUT0_ADDR,
				BIT(port_led->led_pin), 0);
	return ret;
}

static int
rtl837x_cled_hw_control_is_supported(struct led_classdev *ldev, unsigned long rules)
{
	u32 offload_trigger = 0;

	return rtl837x_parse_netdev(rules, &offload_trigger);
}

static int rtl837x_brightness_set_blocking(struct led_classdev *ldev,
						  enum led_brightness brightness)
{
	struct rtl837x_led *port_led = container_of(ldev, struct rtl837x_led, cdev);

	return rtl837x_led_set_brightness(port_led, brightness);
}

static int
rtl837x_cled_hw_control_set(struct led_classdev *ldev, unsigned long rules)
{
	int ret;
	struct rtl837x_led *port_led = container_of(ldev, struct rtl837x_led, cdev);
	struct rtl837x_priv *priv = port_led->priv;
	u32 offload_trigger = 0;


	ret = rtl837x_parse_netdev(rules, &offload_trigger);
	if (ret)
		return ret;

	ret = rtl837x_set_port_led_hw_offload_trigger(port_led, offload_trigger);
	if (ret)
		return ret;
	dev_dbg(priv->dev, "[%s]: set led hardware offload: 0x%x\n", __func__,
			  offload_trigger
			);

	return rtl837x_set_led_hw_offload(port_led, true);
}

static int
rtl837x_cled_hw_control_get(struct led_classdev *ldev, unsigned long *rules)
{
	int ret;
	struct rtl837x_led *port_led = container_of(ldev, struct rtl837x_led, cdev);
	struct rtl837x_priv *priv = port_led->priv;
	u32 offload_trigger = 0;
	u32 tmp;

	if (!port_led->is_hw_offload)
		return -EINVAL;

	ret = rtl837x_reg_bits_read(priv,
		  RTL837X_LED_SETx_SEL0_LEDx_ADDR(port_led->led_set->idx, port_led->led_id),
		  RTL837X_LED_SETx_SEL0_LEDx_MASK(port_led->led_id),
		  &tmp
		);
	if (ret)
		return ret;
	offload_trigger |= tmp;

	ret = rtl837x_reg_bits_read(priv,
		  RTL837X_LED_SETx_SEL1_LEDx_ADDR(port_led->led_set->idx),
		  RTL837X_LED_SETx_SEL1_LEDx_MASK(port_led->led_set->idx, port_led->led_id),
		  &tmp
		);
	if (ret)
		return ret;
	offload_trigger |= (0xf&tmp)<<16;

	/* Parsing specific to netdev trigger */
	if (offload_trigger & RTL837X_LED_LINK_TX_EN_MASK)
		set_bit(TRIGGER_NETDEV_TX, rules);
	if (offload_trigger & RTL837X_LED_LINK_RX_EN_MASK)
		set_bit(TRIGGER_NETDEV_RX, rules);
	if (offload_trigger & RTL837X_LED_LINK_10M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_10, rules);
	if (offload_trigger & RTL837X_LED_LINK_100M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_100, rules);
	if (offload_trigger & RTL837X_LED_LINK_1000M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_1000, rules);
	if (offload_trigger & RTL837X_LED_LINK_2500M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_2500, rules);
	if (offload_trigger & RTL837X_LED_LINK_5000M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_5000, rules);
	if (offload_trigger & RTL837X_LED_LINK_10000M_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK_10000, rules);
	if (offload_trigger & RTL837X_LED_LINK_LINK_EN_MASK)
		set_bit(TRIGGER_NETDEV_LINK, rules);
	if (offload_trigger & RTL837X_LED_LINK_ACT_EN_MASK) {
		set_bit(TRIGGER_NETDEV_TX, rules);
		set_bit(TRIGGER_NETDEV_RX, rules);
	}
	return 0;
}

static struct device *rtl837x_cled_hw_control_get_device(struct led_classdev *ldev)
{
	struct rtl837x_led *port_led = container_of(ldev, struct rtl837x_led, cdev);
	struct rtl837x_priv *priv = port_led->priv;
	struct dsa_port *dp;

	dp = dsa_to_port(priv->ds, port_led->port_num);
	if (!dp)
		return NULL;
	if (dp->user)
		return &dp->user->dev;
	return NULL;
}

static int rtl837x_set_led_mux(struct rtl837x_led *port_led)
{
	struct rtl837x_priv *priv = port_led->priv;

	u32 tmp = FIELD_PREP(0x03, port_led->led_id) |
		  FIELD_PREP(0x3c, port_led->port_num);

	return rtl837x_reg_bits_write(priv, RTL837X_LED_GLB_MUX_ADDR(port_led->led_pin),
			RTL837X_LED_GLB_MUX_LEDx_MUX_MASK(port_led->led_pin), tmp);
}

static int rtl837x_parse_port_leds(struct rtl837x_priv *priv, struct fwnode_handle *port, int port_num)
{
	struct fwnode_handle *led = NULL, *leds = NULL;
	struct led_init_data init_data = { };
	enum led_default_state state;
	struct rtl837x_led *port_led;
	int led_id, led_index, led_pin;
	int ret;

	leds = fwnode_get_named_child_node(port, "leds");
	if (!leds) {
		dev_dbg(priv->dev, "No Leds node specified in device tree for port %d!\n",
			port_num);
		return 0;
	}

	fwnode_for_each_child_node(leds, led) {
		if (fwnode_property_read_u32(led, "reg", &led_id))
			continue;

		if (fwnode_property_read_u32(led, "led-pin", &led_pin)) {
			dev_warn(priv->dev, "led-pin for port %d led %d is missing\n",
				 port_num, led_id);
			continue;
		}

		if (led_id >= RTL837X_PORT_LED_COUNT) {
			dev_warn(priv->dev, "Invalid LED reg %d defined for port %d\n",
				 led_id, port_num);
			continue;
		}

		if (led_pin >= RTL837X_LED_PIN_MAX) {
			dev_warn(priv->dev, "Invalid LED pin %d for port %d led %d\n",
				 led_pin, port_num, led_id);
			continue;
		}

		led_index = RTL837X_LED_PORT_INDEX(port_num, led_id);

		port_led = &priv->ports_led[led_index];
		port_led->port_num = port_num;
		port_led->led_id = led_id;
		port_led->led_pin = led_pin;
		port_led->priv = priv;
		port_led->led_set = NULL;

		ret = rtl837x_set_led_mux(port_led);
		if (ret) {
			dev_warn(priv->dev, "Failed to set LED pin mux for port %d led %d\n",
				 port_num, led_id);
			continue;
		}

		state = led_init_default_state_get(led);
		switch (state) {
		case LEDS_DEFSTATE_ON:
			port_led->cdev.brightness = 1;
			rtl837x_led_set_brightness(port_led, 1);
			break;
		case LEDS_DEFSTATE_KEEP:
			port_led->cdev.brightness = 1;
			break;
		default:
			port_led->cdev.brightness = 0;
			rtl837x_led_set_brightness(port_led, 0);
		}

		port_led->cdev.max_brightness = 1;
		port_led->cdev.brightness_set_blocking = rtl837x_brightness_set_blocking;
		port_led->cdev.hw_control_is_supported = rtl837x_cled_hw_control_is_supported;
		port_led->cdev.hw_control_set = rtl837x_cled_hw_control_set;
		port_led->cdev.hw_control_get = rtl837x_cled_hw_control_get;
		port_led->cdev.hw_control_get_device = rtl837x_cled_hw_control_get_device;
		port_led->cdev.hw_control_trigger = "netdev";
		init_data.default_label = ":port";
		init_data.fwnode = led;
		init_data.devname_mandatory = true;
		init_data.devicename = kasprintf(GFP_KERNEL, "%s:0%d",
						 priv->bus->id,
						 port_num);
		if (!init_data.devicename) {
			fwnode_handle_put(led);
			fwnode_handle_put(leds);
			return -ENOMEM;
		}

		ret = devm_led_classdev_register_ext(priv->dev, &port_led->cdev, &init_data);
		if (ret)
			dev_warn(priv->dev, "Failed to init LED %d for port %d", led_id, port_num);

		kfree(init_data.devicename);
	}

	fwnode_handle_put(leds);
	return 0;
}

int rtl837x_set_led(struct rtl837x_priv *priv)
{
	struct fwnode_handle *ports, *port;
	int port_num;
	int ret;

	ports = device_get_named_child_node(priv->dev, "ports");
	if (!ports)
		ports = device_get_named_child_node(priv->dev, "ethernet-ports");

	if (!ports) {
		dev_info(priv->dev, "No ports node specified in device tree!");
		return 0;
	}

	fwnode_for_each_child_node(ports, port) {
		if (fwnode_property_read_u32(port, "reg", &port_num))
			continue;

		ret = rtl837x_parse_port_leds(priv, port, port_num);
		if (ret) {
			fwnode_handle_put(port);
			fwnode_handle_put(ports);
			return ret;
		}
	}

	fwnode_handle_put(ports);
	return 0;
}
