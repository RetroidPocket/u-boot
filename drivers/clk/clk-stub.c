// SPDX-License-Identifier: GPL-2.0
/*
 * Stub clk driver for non-essential clocks.
 *
 * This driver should be used for clock controllers
 * which are described as dependencies in DT but aren't
 * actually necessary for hardware functionality.
 */

#include <dm/lists.h>
#include <dm/device-internal.h>
#include <power-domain-uclass.h>
#include <clk-uclass.h>
#include <dm.h>

/* NOP parent nodes to stub clocks */
static const struct udevice_id nop_parent_ids[] = {
	{ .compatible = "qcom,rpm-proc" },
	{ .compatible = "qcom,glink-rpm" },
	{ .compatible = "qcom,rpm-sm6115" },
	{ }
};

U_BOOT_DRIVER(nop_parent) = {
	.name = "nop_parent",
	.id = UCLASS_NOP,
	.of_match = nop_parent_ids,
	.bind = dm_scan_fdt_dev,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

static ulong stub_clk_set_rate(struct clk *clk, ulong rate)
{
	return (clk->rate = rate);
}

static ulong stub_clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int stub_clk_nop(struct clk *clk)
{
	return 0;
}

static struct clk_ops stub_clk_ops = {
	.set_rate = stub_clk_set_rate,
	.get_rate = stub_clk_get_rate,
	.enable = stub_clk_nop,
	.disable = stub_clk_nop,
};

static int stub_pd_nop(struct power_domain *pd)
{
	return 0;
}

static struct power_domain_ops stub_pd_ops = {
	.request = stub_pd_nop,
	.on = stub_pd_nop,
	.off = stub_pd_nop,
	.rfree = stub_pd_nop,
};

static int stub_clk_bind(struct udevice *dev)
{
	struct driver *drv;
	ofnode node = dev_ofnode(dev);

	/*
	 * If the clock controller is also a power domain controller, additionally
	 * bind a stub power domain controllers.
	 */
	if (!ofnode_get_property(node, "#power-domain-cells", NULL))
		return 0;

	drv = lists_driver_lookup_name("pd_stub");
	if (!drv)
		return -ENOENT;

	return device_bind_with_driver_data(dev, drv,
					    ofnode_get_name(node),
					    0, node, NULL);
}

static const struct udevice_id stub_clk_ids[] = {
	{ .compatible = "qcom,rpmcc" },
	{ .compatible = "qcom,sm8250-rpmh-clk" },
	{ }
};

U_BOOT_DRIVER(clk_stub) = {
	.name = "clk_stub",
	.id = UCLASS_CLK,
	.ops = &stub_clk_ops,
	.of_match = stub_clk_ids,
	.bind = stub_clk_bind,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

U_BOOT_DRIVER(pd_stub) = {
	.name = "pd_stub",
	.id = UCLASS_POWER_DOMAIN,
	.ops = &stub_pd_ops,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
