// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Adaptrum Anarion DWMAC glue layer
 *
 * Copyright (C) 2017, Adaptrum, Inc.
 * (Written by Alexandru Gagniuc <alex.g at adaptrum.com> for Adaptrum, Inc.)
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/stmmac.h>

#include "stmmac.h"
#include "stmmac_platform.h"

#define GMAC_RESET_CONTROL_REG		0
#define GMAC_SW_CONFIG_REG		4
#define  GMAC_CONFIG_INTF_SEL_MASK	(0x7 << 0)
#define  GMAC_CONFIG_INTF_RGMII		(0x1 << 0)

struct anarion_gmac {
	void __iomem *ctl_block;
	uint32_t phy_intf_sel;
};

static uint32_t gmac_read_reg(struct anarion_gmac *gmac, uint8_t reg)
{
	return readl(gmac->ctl_block + reg);
};

static void gmac_write_reg(struct anarion_gmac *gmac, uint8_t reg, uint32_t val)
{
	writel(val, gmac->ctl_block + reg);
}

static int anarion_gmac_init(struct platform_device *pdev, void *priv)
{
	uint32_t sw_config;
	struct anarion_gmac *gmac = priv;

	/* Reset logic, configure interface mode, then release reset. SIMPLE! */
	gmac_write_reg(gmac, GMAC_RESET_CONTROL_REG, 1);

	sw_config = gmac_read_reg(gmac, GMAC_SW_CONFIG_REG);
	sw_config &= ~GMAC_CONFIG_INTF_SEL_MASK;
	sw_config |= (gmac->phy_intf_sel & GMAC_CONFIG_INTF_SEL_MASK);
	gmac_write_reg(gmac, GMAC_SW_CONFIG_REG, sw_config);

	gmac_write_reg(gmac, GMAC_RESET_CONTROL_REG, 0);

	return 0;
}

static void anarion_gmac_exit(struct platform_device *pdev, void *priv)
{
	struct anarion_gmac *gmac = priv;

	gmac_write_reg(gmac, GMAC_RESET_CONTROL_REG, 1);
}

static struct anarion_gmac *
anarion_config_dt(struct platform_device *pdev,
		  struct plat_stmmacenet_data *plat_dat)
{
	struct anarion_gmac *gmac;
	void __iomem *ctl_block;

	ctl_block = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(ctl_block)) {
		dev_err(&pdev->dev, "Cannot get reset region (%pe)!\n",
			ctl_block);
		return ERR_CAST(ctl_block);
	}

	gmac = devm_kzalloc(&pdev->dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac)
		return ERR_PTR(-ENOMEM);

	gmac->ctl_block = ctl_block;

	if (phy_interface_mode_is_rgmii(plat_dat->phy_interface)) {
		gmac->phy_intf_sel = GMAC_CONFIG_INTF_RGMII;
	} else {
		dev_err(&pdev->dev, "Unsupported phy-mode (%s)\n",
			phy_modes(plat_dat->phy_interface));
		return ERR_PTR(-ENOTSUPP);
	}

	return gmac;
}

static int anarion_dwmac_probe(struct platform_device *pdev)
{
	int ret;
	struct anarion_gmac *gmac;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = devm_stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	gmac = anarion_config_dt(pdev, plat_dat);
	if (IS_ERR(gmac))
		return PTR_ERR(gmac);

	plat_dat->init = anarion_gmac_init;
	plat_dat->exit = anarion_gmac_exit;
	plat_dat->bsp_priv = gmac;

	return devm_stmmac_pltfr_probe(pdev, plat_dat, &stmmac_res);
}

static const struct of_device_id anarion_dwmac_match[] = {
	{ .compatible = "adaptrum,anarion-gmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, anarion_dwmac_match);

static struct platform_driver anarion_dwmac_driver = {
	.probe  = anarion_dwmac_probe,
	.driver = {
		.name           = "anarion-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = anarion_dwmac_match,
	},
};
module_platform_driver(anarion_dwmac_driver);

MODULE_DESCRIPTION("Adaptrum Anarion DWMAC specific glue layer");
MODULE_AUTHOR("Alexandru Gagniuc <mr.nuke.me@gmail.com>");
MODULE_LICENSE("GPL v2");
