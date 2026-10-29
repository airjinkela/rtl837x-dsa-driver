// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/version.h>
#include <linux/gpio/driver.h>

#include "rtl837x.h"
#include "rtl837x_gpio.h"

#define RTL837X_MAX_GPIO 63

/* Pin multiplexing */
#define RTL837X_IO_MUX_SEL_0_ADDR                              0x7F8C
#define RTL837X_IO_MUX_SEL_1_ADDR                              0x7F90
#define   RTL837X_IO_MUX_SEL_1_GPIO_PWM_OUT_SEL_MASK           BIT(30)
#define   RTL837X_IO_MUX_SEL_1_GPIO_SDA4_SEL_MASK              BIT(29)
#define   RTL837X_IO_MUX_SEL_1_GPIO_MDX1_SEL_1_MASK            BIT(6)
#define   RTL837X_IO_MUX_SEL_1_GPIO_MDX1_SEL_0_MASK            BIT(5)
#define   RTL837X_IO_MUX_SEL_1_GPIO_MDIO0_SEL_MASK             BIT(4)
#define   RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_1_MASK            BIT(1)
#define   RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_0_MASK            BIT(0)
#define RTL837X_IO_MUX_SEL_2_ADDR                              0x7F94
#define   RTL837X_IO_MUX_SEL_2_ACL_BIT3_EN_MASK                BIT(3)

/* GPIO */
#define RTL837X_GPIO_OUT0_ADDR                  0x3C
#define RTL837X_GPIO_OUT1_ADDR                  0x40
#define RTL837X_GPIO_IN0_ADDR                   0x44
#define RTL837X_GPIO_IN1_ADDR                   0x48
#define RTL837X_GPIO_OE0_ADDR                   0x4C
#define RTL837X_GPIO_OE1_ADDR                   0x50
#define RTL837X_INI_MODE_ADDR                   0x58
#define   RTL837X_INI_MODE_INI_MODE_MASK        GENMASK(1, 0)

struct rtl837x_gpio {
	struct rtl837x_priv *priv;
	struct gpio_chip gp;
};

// I think this can be rewrite into linux pinctrl
// But there is no more info about the gpio mux or gpio func in the chip datasheet
// So just use gpio mode to simulate i2c or mdio

int rtl837x_gpio_led_mode_set(struct rtl837x_priv *priv, unsigned int offset, bool gpio_mode)
{
	if (offset > 29)
		return -EINVAL;
	return rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_0_ADDR,
				BIT(offset), !!gpio_mode);
}

int rtl837x_gpio_value_set(struct rtl837x_priv *priv, unsigned int offset, int val)
{
	int ret;

	if (offset > RTL837X_MAX_GPIO)
		return -EINVAL;

	if (offset < 32)
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OUT0_ADDR,
				BIT(offset), !!val);
	else
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OUT1_ADDR,
				BIT(offset-32), !!val);
	return ret;
}

int rtl837x_gpio_value_get(struct rtl837x_priv *priv, unsigned int offset, int *val)
{
	int ret;
	u32 tmp;

	if (offset > RTL837X_MAX_GPIO)
		return -EINVAL;

	if (offset < 32)
		ret = rtl837x_reg_bits_read(priv, RTL837X_GPIO_IN0_ADDR,
				BIT(offset), &tmp);
	else
		ret = rtl837x_reg_bits_read(priv, RTL837X_GPIO_IN1_ADDR,
				BIT(offset-32), &tmp);
	if (ret)
		return ret;

	*val = !!(tmp&1);
	return 0;
}

int rtl837x_gpio_direction_set(struct rtl837x_priv *priv, unsigned int offset, bool input)
{
	u32 value = input ? 0 : 1;
	int ret;

	if (offset > RTL837X_MAX_GPIO)
		return -EINVAL;


	if (offset < 32)
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OE0_ADDR,
				BIT(offset), value);
	else
		ret = rtl837x_reg_bits_write(priv, RTL837X_GPIO_OE1_ADDR,
				BIT(offset-32), value);
	return ret;
}

int rtl837x_gpio_direction_get(struct rtl837x_priv *priv, unsigned int offset, bool *input)
{
	int ret;
	u32 tmp;

	if (offset > RTL837X_MAX_GPIO)
		return -EINVAL;

	if (offset < 32)
		ret = rtl837x_reg_bits_read(priv, RTL837X_GPIO_OE0_ADDR,
				BIT(offset), &tmp);
	else
		ret = rtl837x_reg_bits_read(priv, RTL837X_GPIO_OE1_ADDR,
				BIT(offset-32), &tmp);
	if (ret)
		return ret;

	if (tmp&1)
		*input = false;
	else
		*input = true;

	return 0;
}

#ifdef CONFIG_GPIOLIB
static int rtl837x_gpiochip_request(struct gpio_chip *gc, unsigned int offset)
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	int ret = 0;

	switch (offset) {
	case 0 ... 29:
		ret = rtl837x_gpio_led_mode_set(priv, offset, true);
		break;
	case 30:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_2_ADDR,
				 RTL837X_IO_MUX_SEL_2_ACL_BIT3_EN_MASK, 1);
		break;
	case 31 ... 32:
		// we have to set RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_0_MASK and RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_1_MASK
		// only set RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_0_MASK or RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_1_MASK is not work
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_0_MASK | RTL837X_IO_MUX_SEL_1_PAD_UART0_SEL_1_MASK, 3);
		break;
	case 33 ... 35:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 BIT(offset-31), 1);
		break;
	case 36:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 RTL837X_IO_MUX_SEL_1_GPIO_PWM_OUT_SEL_MASK, 1);
		break;
	case 37 ... 38:
		break; //do nothing
	case 39:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 RTL837X_IO_MUX_SEL_1_GPIO_SDA4_SEL_MASK, 1);
		break;
	case 40:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 RTL837X_IO_MUX_SEL_1_GPIO_MDX1_SEL_0_MASK, 1);
		break;
	case 41:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 RTL837X_IO_MUX_SEL_1_GPIO_MDX1_SEL_1_MASK, 1);
		break;
	case 42 ... 45:
		// what fuck is this?
		// I don't know how does realtek use two bits to represent gpio 42 43 44 45
		// Is this a magic?????
		ret = rtl837x_reg_bits_write(priv, RTL837X_INI_MODE_ADDR,
				 RTL837X_INI_MODE_INI_MODE_MASK, 1);
		break;
	case 46 ... 51:
		// MSCK and MSDA
		// Humm. what val should I set to make the gpiomux mode to gpio?
		// Maybe is 0b0011 ? I'm not sure about this.
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 (0x180 << ((offset-46)*2)), 3);
		break;
	case 52 ... 54:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_2_ADDR,
				 BIT(offset-52), 1);
		break;
	case 55 ... 60:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 BIT(offset-36), 1);
		break;
	case 61 ... 62:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				 BIT(offset-34), 1);
		break;
	case 63:
		ret = rtl837x_reg_bits_write(priv, RTL837X_IO_MUX_SEL_1_ADDR,
				RTL837X_IO_MUX_SEL_1_GPIO_MDIO0_SEL_MASK, 1);
		break;
	default:
		dev_err(priv->dev, "gpio(%d): failed to request gpio, Out of range\n", offset);
		return -EINVAL;
	}

	if (ret) {
		dev_err(priv->dev, "gpio(%d): failed to request gpio err: %x", offset, ret);
		return -ENODEV;
	}
	return 0;
}

#if KERNEL_VERSION(6, 18, 0) <= LINUX_VERSION_CODE
static int rtl837x_gpiochip_set(struct gpio_chip *gc, unsigned int offset, int value)
#else
static void rtl837x_gpiochip_set(struct gpio_chip *gc, unsigned int offset, int value)
#endif
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	int ret;

	ret = rtl837x_gpio_value_set(priv, offset, value);
	if (ret) {
		dev_err(priv->dev, "gpio(%d): failed to write gpio val %x", offset, ret);
#if KERNEL_VERSION(6, 18, 0) <= LINUX_VERSION_CODE
		return -EIO;
#endif
	}
#if KERNEL_VERSION(6, 18, 0) <= LINUX_VERSION_CODE
	return 0;
#endif
}

static int rtl837x_gpiochip_get(struct gpio_chip *gc, unsigned int offset)
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	u32 value;
	int ret;

	ret = rtl837x_gpio_value_get(priv, offset, &value);
	if (ret) {
		dev_err(priv->dev, "gpio(%d): failed to read gpio val %x", offset, ret);
		return ret;
	}

	return value;
}

static int rtl837x_gpiochip_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	int ret;

	ret = rtl837x_gpio_direction_set(priv, offset, true);
	if (ret) {
		dev_err(priv->dev, "gpio(%d): failed to set gpio direction input %x", offset, ret);
		return -EINVAL;
	}
	return 0;
}

static int rtl837x_gpiochip_direction_output(struct gpio_chip *gc,
					unsigned int offset, int value)
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	int ret;

	ret = rtl837x_gpio_direction_set(priv, offset, false);
	if (ret) {
		dev_err(gpio->priv->dev, "gpio(%d): failed to set gpio direction output %x", offset, ret);
		return ret;
	}

	ret = rtl837x_gpio_value_set(priv, offset, value);
	if (ret) {
		dev_err(gpio->priv->dev, "gpio(%d): failed to write gpio val %x", offset, ret);
		return ret;
	}
	return 0;
}

static int rtl837x_gpiochip_get_direction(struct gpio_chip *gc,
						unsigned int offset)
{
	struct rtl837x_gpio *gpio = gpiochip_get_data(gc);
	struct rtl837x_priv *priv = gpio->priv;
	bool is_input;
	int ret;

	ret = rtl837x_gpio_direction_get(priv, offset, &is_input);
	if (ret)
		return ret;

	if (is_input)
		return GPIO_LINE_DIRECTION_IN;
	else
		return GPIO_LINE_DIRECTION_OUT;
}

static const struct gpio_chip rtl837x_gp = {
	.label = "rtl837x-gpio",
	.owner = THIS_MODULE,
	.request = rtl837x_gpiochip_request,
	.get = rtl837x_gpiochip_get,
	.set = rtl837x_gpiochip_set,
	.get_direction = rtl837x_gpiochip_get_direction,
	.direction_input = rtl837x_gpiochip_direction_input,
	.direction_output = rtl837x_gpiochip_direction_output,
	.can_sleep = true,
	.ngpio = RTL837X_MAX_GPIO,
};

int rtl837x_gpiochip_init(struct rtl837x_priv *priv)
{
	struct rtl837x_gpio *gpio;
	struct device_node *np;
	struct device *dev = priv->dev;
	int base = 0;

	np = dev->of_node;

	if (of_property_read_u32(np, "base", &base))
		base = -1;

	gpio = devm_kmalloc(dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	gpio->priv = priv;
	gpio->gp = rtl837x_gp;
	gpio->gp.base = base;
	gpio->gp.parent = dev;

	return devm_gpiochip_add_data(dev, &gpio->gp, gpio);
}
#endif /* CONFIG_GPIOLIB */
