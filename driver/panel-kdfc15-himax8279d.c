// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, Huaqin Telecom Technology Co., Ltd
 *
 * Author: Jerry Han <jerry.han.hq@gmail.com>
 * Author: qing yu tian <1741706321@qq.com>
 * Reworked to follow the same panel lifecycle structure as panel-ili9881.c.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define KDXS_PANEL_DEBUG 1
#define KDXS_INIT_SEND_DCS_WRITE 1
#define KDXS_INIT_SEND_DCS_BUFFER 2
#define KDXS_INIT_SEND_GENERIC 3
#define KDXS_INIT_SEND_MODE KDXS_INIT_SEND_DCS_WRITE
#define KDXS_INIT_INTER_CMD_DELAY 0
#define KDXS_INIT_INTER_CMD_DELAY_MS 10

#if KDXS_PANEL_DEBUG
#define kdxs_dbg(dev, fmt, ...) \
	dev_info(dev, "[kdxs] " fmt, ##__VA_ARGS__)
#else
#define kdxs_dbg(dev, fmt, ...) \
	do { \
	} while (0)
#endif

enum kdxs_init_cmd_type {
	KDXS_INIT_DCS_CMD,
	KDXS_DELAY_CMD,
};

struct kdxs_init_cmd {
	u8 type;
	u8 len;
	const u8 *data;
};

#define _INIT_DCS_CMD(...) \
	{ \
		.type = KDXS_INIT_DCS_CMD, \
		.len = sizeof((u8[]){ __VA_ARGS__ }), \
		.data = (u8[]){ __VA_ARGS__ }, \
	}

#define _INIT_DELAY_CMD(...) \
	{ \
		.type = KDXS_DELAY_CMD, \
		.len = sizeof((u8[]){ __VA_ARGS__ }), \
		.data = (u8[]){ __VA_ARGS__ }, \
	}

enum kdxs_desc_flags {
	KDXS_FLAGS_NO_SHUTDOWN_CMDS = BIT(0),
	KDXS_FLAGS_PANEL_ON_IN_PREPARE = BIT(1),
	KDXS_FLAGS_MAX = BIT(31),
};

struct kdxs_panel_desc {
	const struct kdxs_init_cmd *init;
	size_t init_length;
	const struct drm_display_mode *mode;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	unsigned int bpc;
	unsigned int width_mm;
	unsigned int height_mm;
	enum kdxs_desc_flags flags;
};

struct kdxs_panel {
	struct drm_panel panel;                 
	struct mipi_dsi_device *dsi;            
	const struct kdxs_panel_desc *desc;     

	struct regulator *power;               
	struct gpio_desc *reset;                

	enum drm_panel_orientation orientation;
};

static inline struct kdxs_panel *panel_to_kdxs(struct drm_panel *panel)
{
	return container_of(panel, struct kdxs_panel, panel);
}

static int kdxs_send_init_cmds(struct kdxs_panel *ctx)
{
	unsigned int i;

	if (!ctx->desc->init_length)
		return 0;

	kdxs_dbg(&ctx->dsi->dev, "send init cmds start, count=%zu\n",
		 ctx->desc->init_length);

	for (i = 0; i < ctx->desc->init_length; i++) {
		const struct kdxs_init_cmd *cmd = &ctx->desc->init[i];
		int ret;

		if (cmd->type == KDXS_DELAY_CMD) {
			u32 delay_ms = cmd->data[0];

			kdxs_dbg(&ctx->dsi->dev, "init delay[%u]: %u ms\n", i,
				 (unsigned int)delay_ms);
			msleep(delay_ms);
			continue;
		}

#if KDXS_INIT_SEND_MODE == KDXS_INIT_SEND_DCS_WRITE
		ret = mipi_dsi_dcs_write(ctx->dsi, cmd->data[0],
					 &cmd->data[1], cmd->len - 1);
#elif KDXS_INIT_SEND_MODE == KDXS_INIT_SEND_DCS_BUFFER
		ret = mipi_dsi_dcs_write_buffer(ctx->dsi, cmd->data, cmd->len);
#elif KDXS_INIT_SEND_MODE == KDXS_INIT_SEND_GENERIC
		ret = mipi_dsi_generic_write(ctx->dsi, cmd->data, cmd->len);
#else
#error "Unsupported KDXS_INIT_SEND_MODE"
#endif
		if (ret < 0) {
			dev_err(&ctx->dsi->dev,
				"[kdxs] init cmd[%u] failed: cmd=0x%02x len=%u err=%d\n",
				i, cmd->data[0], cmd->len, ret);
			return ret;
		}

#if KDXS_INIT_INTER_CMD_DELAY
		msleep(KDXS_INIT_INTER_CMD_DELAY_MS);
#endif
	}

	kdxs_dbg(&ctx->dsi->dev, "send init cmds done\n");
	return 0;
}

static int kdxs_panel_prepare(struct drm_panel *panel)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);
	int ret;

	kdxs_dbg(&ctx->dsi->dev, "prepare enter\n");

	ret = regulator_enable(ctx->power);
	if (ret) {
		dev_err(&ctx->dsi->dev, "failed to enable power: %d\n", ret);
		return ret;
	}

	msleep(20);

	if (ctx->reset) {
		/* ACTIVE_LOW reset: logical 1 drives the pin low. */
		gpiod_set_value_cansleep(ctx->reset, 1);
		kdxs_dbg(&ctx->dsi->dev, "reset assert\n");
		msleep(10);

		gpiod_set_value_cansleep(ctx->reset, 0);
		kdxs_dbg(&ctx->dsi->dev, "reset release\n");
		msleep(20);
	}

	ret = kdxs_send_init_cmds(ctx);
	if (ret) {
		dev_err(&ctx->dsi->dev, "failed to send DCS Init Code: %d\n", ret);
		goto poweroff;
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
	if (ret) {
		dev_err(&ctx->dsi->dev, "failed to exit sleep mode: %d\n", ret);
		goto poweroff;
	}

	kdxs_dbg(&ctx->dsi->dev, "exit sleep mode done\n");
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(ctx->dsi);
	if (ret) {
		dev_err(&ctx->dsi->dev, "failed to set display on: %d\n", ret);
		goto poweroff;
	}

	kdxs_dbg(&ctx->dsi->dev, "display on done\n");
	msleep(20);
	return 0;

poweroff:
	if (ctx->reset)
		gpiod_set_value_cansleep(ctx->reset, 1);
	regulator_disable(ctx->power);
	return ret;
}

static int kdxs_panel_enable(struct drm_panel *panel)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);

	kdxs_dbg(&ctx->dsi->dev, "enable enter\n");
	return 0;
}

static int kdxs_panel_disable(struct drm_panel *panel)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);

	kdxs_dbg(&ctx->dsi->dev, "disable enter\n");

	if (!(ctx->desc->flags & KDXS_FLAGS_PANEL_ON_IN_PREPARE))
		mipi_dsi_dcs_set_display_off(ctx->dsi);

	return 0;
}

static int kdxs_panel_unprepare(struct drm_panel *panel)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);

	kdxs_dbg(&ctx->dsi->dev, "unprepare enter\n");

	if (!(ctx->desc->flags & KDXS_FLAGS_NO_SHUTDOWN_CMDS)) {
		if (ctx->desc->flags & KDXS_FLAGS_PANEL_ON_IN_PREPARE)
			mipi_dsi_dcs_set_display_off(ctx->dsi);

		mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
		usleep_range(1000, 2000);
	}

	if (ctx->reset)
		gpiod_set_value_cansleep(ctx->reset, 1);

	regulator_disable(ctx->power);
	return 0;
}

static int kdxs_panel_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->desc->mode);
	if (!mode) {
		dev_err(&ctx->dsi->dev, "failed to duplicate mode %ux%u@%u\n",
			ctx->desc->mode->hdisplay,
			ctx->desc->mode->vdisplay,
			drm_mode_vrefresh(ctx->desc->mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = ctx->desc->width_mm;
	connector->display_info.height_mm = ctx->desc->height_mm;
	connector->display_info.bpc = ctx->desc->bpc;
	drm_connector_set_panel_orientation(connector, ctx->orientation);

	return 1;
}

static enum drm_panel_orientation
kdxs_panel_get_orientation(struct drm_panel *panel)
{
	struct kdxs_panel *ctx = panel_to_kdxs(panel);

	return ctx->orientation;
}

static const struct drm_panel_funcs kdxs_panel_funcs = {
	.prepare = kdxs_panel_prepare,
	.unprepare = kdxs_panel_unprepare,
	.enable = kdxs_panel_enable,
	.disable = kdxs_panel_disable,
	.get_modes = kdxs_panel_get_modes,
	.get_orientation = kdxs_panel_get_orientation,
};

static const struct drm_display_mode kdxs_default_mode = {
	.clock = 160000, // 160MHz 整数时钟，VC4完美支持
	.hdisplay = 1200,
	.hsync_start = 1200 + 80,  // 增加前肩
	.hsync_end = 1200 + 80 + 40, // 缩短同步宽度
	.htotal = 1200 + 80 + 40 + 40, // 总1360
	.vdisplay = 1920,
	.vsync_start = 1920 + 20,  // 增加前肩
	.vsync_end = 1920 + 20 + 4, // 缩短同步宽度
	.vtotal = 1920 + 20 + 4 + 16, // 总1960
};



static const struct kdxs_init_cmd kdxs_himax8279_init[] = {
	/* page 1 */
	_INIT_DCS_CMD(0xB0, 0x01),
	_INIT_DCS_CMD(0xC3, 0x0F),
	_INIT_DCS_CMD(0xC4, 0x00),
	_INIT_DCS_CMD(0xC5, 0x00),
	_INIT_DCS_CMD(0xC6, 0x00),
	_INIT_DCS_CMD(0xC7, 0x00),
	_INIT_DCS_CMD(0xC8, 0x0D),
	_INIT_DCS_CMD(0xC9, 0x12),
	_INIT_DCS_CMD(0xCA, 0x11),
	_INIT_DCS_CMD(0xCD, 0x1D),
	_INIT_DCS_CMD(0xCE, 0x1B),
	_INIT_DCS_CMD(0xCF, 0x0B),
	_INIT_DCS_CMD(0xD0, 0x09),
	_INIT_DCS_CMD(0xD1, 0x07),
	_INIT_DCS_CMD(0xD2, 0x05),
	_INIT_DCS_CMD(0xD3, 0x01),
	_INIT_DCS_CMD(0xD7, 0x10),
	_INIT_DCS_CMD(0xD8, 0x00),
	_INIT_DCS_CMD(0xD9, 0x00),
	_INIT_DCS_CMD(0xDA, 0x00),
	_INIT_DCS_CMD(0xDB, 0x00),
	_INIT_DCS_CMD(0xDC, 0x0E),
	_INIT_DCS_CMD(0xDD, 0x12),
	_INIT_DCS_CMD(0xDE, 0x11),
	_INIT_DCS_CMD(0xE1, 0x1E),
	_INIT_DCS_CMD(0xE2, 0x1C),
	_INIT_DCS_CMD(0xE3, 0x0C),
	_INIT_DCS_CMD(0xE4, 0x0A),
	_INIT_DCS_CMD(0xE5, 0x08),
	_INIT_DCS_CMD(0xE6, 0x06),
	_INIT_DCS_CMD(0xE7, 0x02),
	/* page 3 */
	_INIT_DCS_CMD(0xB0, 0x03),
	_INIT_DCS_CMD(0xBE, 0x03),
	_INIT_DCS_CMD(0xCC, 0x44),
	_INIT_DCS_CMD(0xC8, 0x07),
	_INIT_DCS_CMD(0xC9, 0x05),
	_INIT_DCS_CMD(0xCA, 0x42),
	_INIT_DCS_CMD(0xCD, 0x3E),
	_INIT_DCS_CMD(0xCF, 0x60),
	_INIT_DCS_CMD(0xD2, 0x04),
	_INIT_DCS_CMD(0xD3, 0x04),
	_INIT_DCS_CMD(0xD4, 0x01),
	_INIT_DCS_CMD(0xD5, 0x00),
	_INIT_DCS_CMD(0xD6, 0x03),
	_INIT_DCS_CMD(0xD7, 0x04),
	_INIT_DCS_CMD(0xD9, 0x01),
	_INIT_DCS_CMD(0xDB, 0x01),
	_INIT_DCS_CMD(0xE4, 0xF0),
	_INIT_DCS_CMD(0xE5, 0x0A),
	/* page 0 */
	_INIT_DCS_CMD(0xB0, 0x00),
	_INIT_DCS_CMD(0xBD, 0x63),
	_INIT_DCS_CMD(0xC2, 0x08),
	_INIT_DCS_CMD(0xC4, 0x10),
	/* page 2 */
	_INIT_DCS_CMD(0xB0, 0x02),
	_INIT_DCS_CMD(0xC0, 0x00),
	_INIT_DCS_CMD(0xC1, 0x0C),
	_INIT_DCS_CMD(0xC2, 0x14),
	_INIT_DCS_CMD(0xC3, 0x21),
	_INIT_DCS_CMD(0xC4, 0x21),
	_INIT_DCS_CMD(0xC5, 0x20),
	_INIT_DCS_CMD(0xC6, 0x1F),
	_INIT_DCS_CMD(0xC7, 0x1E),
	_INIT_DCS_CMD(0xC8, 0x1A),
	_INIT_DCS_CMD(0xC9, 0x16),
	_INIT_DCS_CMD(0xCA, 0x15),
	_INIT_DCS_CMD(0xCB, 0x15),
	_INIT_DCS_CMD(0xCC, 0x16),
	_INIT_DCS_CMD(0xCD, 0x1B),
	_INIT_DCS_CMD(0xCE, 0x1C),
	_INIT_DCS_CMD(0xCF, 0x1E),
	_INIT_DCS_CMD(0xD0, 0x07),
	_INIT_DCS_CMD(0xD1, 0x00),
	_INIT_DCS_CMD(0xD2, 0x00),
	_INIT_DCS_CMD(0xD3, 0x0C),
	_INIT_DCS_CMD(0xD4, 0x14),
	_INIT_DCS_CMD(0xD5, 0x21),
	_INIT_DCS_CMD(0xD6, 0x21),
	_INIT_DCS_CMD(0xD7, 0x20),
	_INIT_DCS_CMD(0xD8, 0x1F),
	_INIT_DCS_CMD(0xD9, 0x1E),
	_INIT_DCS_CMD(0xDA, 0x1A),
	_INIT_DCS_CMD(0xDB, 0x16),
	_INIT_DCS_CMD(0xDC, 0x15),
	_INIT_DCS_CMD(0xDD, 0x15),
	_INIT_DCS_CMD(0xDE, 0x16),
	_INIT_DCS_CMD(0xDF, 0x1B),
	_INIT_DCS_CMD(0xE0, 0x1C),
	_INIT_DCS_CMD(0xE1, 0x1E),
	_INIT_DCS_CMD(0xE2, 0x07),
};

static const struct kdxs_panel_desc kdxs_himax8279_desc = {
	.init = kdxs_himax8279_init,
	.init_length = ARRAY_SIZE(kdxs_himax8279_init),
	.mode = &kdxs_default_mode,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		      MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
	.bpc = 8,
	.width_mm = 135,
	.height_mm = 216,
	.flags = KDXS_FLAGS_PANEL_ON_IN_PREPARE,
};

static const struct of_device_id kdxs_panel_of_match[] = {
	{ .compatible = "kdxs,himax8279", .data = &kdxs_himax8279_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, kdxs_panel_of_match);

static int kdxs_panel_probe(struct mipi_dsi_device *dsi)
{
	struct kdxs_panel *ctx;
	int ret;

	kdxs_dbg(&dsi->dev, "probe enter\n");


	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;


	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	ctx->desc = of_device_get_match_data(&dsi->dev);


	ctx->panel.prepare_prev_first = true;


	drm_panel_init(&ctx->panel, &dsi->dev, &kdxs_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);


	ctx->power = devm_regulator_get(&dsi->dev, "power");
	if (IS_ERR(ctx->power))
		return dev_err_probe(&dsi->dev, PTR_ERR(ctx->power),
				     "failed to get power regulator\n");
	kdxs_dbg(&dsi->dev, "got power regulator\n");


	ctx->reset = devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset))
		return dev_err_probe(&dsi->dev, PTR_ERR(ctx->reset),
				     "failed to get reset gpio\n");
	kdxs_dbg(&dsi->dev, "got reset gpio\n");


	ret = of_drm_get_panel_orientation(dsi->dev.of_node, &ctx->orientation);
	if (ret)
		return ret;


	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;


	drm_panel_add(&ctx->panel);


	dsi->mode_flags = ctx->desc->mode_flags;
	dsi->format = ctx->desc->format;
	dsi->lanes = ctx->desc->lanes;


	ret = mipi_dsi_attach(dsi);
	if (ret) {
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	kdxs_dbg(&dsi->dev, "dsi attach done\n");
	kdxs_dbg(&dsi->dev, "probe done\n");
	return 0;
}

static void kdxs_panel_remove(struct mipi_dsi_device *dsi)
{
	struct kdxs_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	if (!IS_ERR_OR_NULL(ctx->reset))
		gpiod_set_value_cansleep(ctx->reset, 1);

	if (!IS_ERR_OR_NULL(ctx->power))
		regulator_disable(ctx->power);
}

static struct mipi_dsi_driver kdxs_panel_driver = {
	.probe = kdxs_panel_probe,
	.remove = kdxs_panel_remove,
	.driver = {
		.name = "panel-kdxs-himax8279",
		.of_match_table = kdxs_panel_of_match,
	},
};
module_mipi_dsi_driver(kdxs_panel_driver);

MODULE_AUTHOR("Jerry Han <jerry.han.hq@gmail.com>");
MODULE_AUTHOR("qing yu tian <1741706321@qq.com>");
MODULE_DESCRIPTION("KDXS Himax8279 driver");
MODULE_LICENSE("GPL v2");
