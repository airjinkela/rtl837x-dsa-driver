/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef _REALTEK_RTL837X_GPIO_H
#define _REALTEK_RTL837X_GPIO_H

#include <linux/types.h>

#include "rtl837x.h"

int rtl837x_gpio_led_mode_set(struct realtek_priv *priv, unsigned int offset, bool gpio_mode);
int rtl837x_gpio_direction_set(struct realtek_priv *priv, unsigned int offset, bool input);
int rtl837x_gpio_direction_get(struct realtek_priv *priv, unsigned int offset, bool *input);
int rtl837x_gpio_value_set(struct realtek_priv *priv, unsigned int offset, int val);
int rtl837x_gpio_value_get(struct realtek_priv *priv, unsigned int offset, int *val);

#ifdef CONFIG_GPIOLIB
int rtl837x_gpiochip_init(struct realtek_priv *priv);
#endif /* CONFIG_GPIOLIB */

#endif /* _REALTEK_RTL837X_GPIO_H */
