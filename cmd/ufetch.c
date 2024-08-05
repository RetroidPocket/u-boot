// SPDX-License-Identifier: GPL-2.0

/* Small "fetch" utility for U-Boot */

#include <mmc.h>
#include <time.h>
#include <asm/global_data.h>
#include <cli.h>
#include <command.h>
#include <dm/ofnode.h>
#include <env.h>
#include <rand.h>
#include <vsprintf.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <version.h>

DECLARE_GLOBAL_DATA_PTR;

#define LINE_WIDTH 40
#define BLUE "\033[1;34m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"
static const char *logo_lines[] = {
	BLUE "                  ......::......                   ",
	BLUE "             ...::::::::::::::::::...              ",
	BLUE "          ..::::::::::::::::::::::::::..           ",
	BLUE "        .::::.:::::::::::::::...::::.::::.         ",
	BLUE "      .::::::::::::::::::::..::::::::::::::.       ",
	BLUE "    .::.:::::::::::::::::::" YELLOW "=*%#*" BLUE "::::::::::.::.     ",
	BLUE "   .:::::::::::::::::....." YELLOW "*%%*-" BLUE ":....::::::::::.    ",
	BLUE "  .:.:::...:::::::::.:-" YELLOW "===##*---==-" BLUE "::::::::::.:.   ",
	BLUE " .::::..::::........" YELLOW "-***#****###****-" BLUE "...::::::.:.  ",
	BLUE " ::.:.-" YELLOW "+***+=" BLUE "::-" YELLOW "=+**#%%%%%%%%%%%%###*= " BLUE "-::...::::. ",
	BLUE ".:.::-" YELLOW "*****###%%%%%%%%%%%%%%%%%%%%%%%%%%#*=" BLUE ":..:::: ",
	BLUE ".::" YELLOW "##" BLUE ":" YELLOW "***#%%%%%%#####%%%%%%%####%%%%%####%%%*" BLUE "-.::. ",
	BLUE ":.:" YELLOW "#%" BLUE "::" YELLOW "*%%%%%%%#*****##%%%#*****##%%##*****#%%+" BLUE ".::.",
	BLUE ".::" YELLOW "**==#%%%%%%%##****#%%%%##****#%%%%#****###%%" BLUE ":.. ",
	BLUE "..:" YELLOW "#%" BLUE "::" YELLOW "*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#%%%%%+ " BLUE ".:.",
	BLUE " ::" YELLOW "##" BLUE ":" YELLOW "+**#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%* " BLUE "-.:: ",
	BLUE " ..::-" YELLOW "#****#%#%%%%%%%%%%%%%%%%%%%%%%%%%%#*=" BLUE "-..::.  ",
	BLUE "  ...:=" YELLOW "*****=" BLUE "::-\033[1;37m=+**###%%%%%%%%###**+=  " BLUE "--:...:::  ",
	BLUE "   .::.::--:........::::::--::::::......::::::.    ",
	BLUE "    .::.....::::::::::...........:::::::::.::.     ",
	BLUE "      .::::::::::::::::::::::::::::::::::::.       ",
	BLUE "        .::::.::::::::::::::::::::::.::::.         ",
	BLUE "          ..::::::::::::::::::::::::::..           ",
	BLUE "             ...::::::::::::::::::...              ",
	BLUE "                  ......::......                   ",
};

enum output_lines {
	FIRST,
	SECOND,
	KERNEL,
	SYSINFO,
	HOST,
	UPTIME,
	IP,
	CMDS,
	CONSOLES,
	DEVICES,
	FEATURES,
	RELOCATION,
	CORES,
	MEMORY,
	STORAGE,

	/* Up to 10 storage devices... Should be enough for anyone right? */
	_LAST_LINE = (STORAGE + 10),
#define LAST_LINE (_LAST_LINE - 1UL)
};

static int do_ufetch(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	int num_lines = max(LAST_LINE + 1, ARRAY_SIZE(logo_lines));
	const char *model, *compatible;
	char *ipaddr;
	int n_cmds, n_cpus = 0, ret, compatlen;
	ofnode np;
	struct mmc *mmc;
	struct blk_desc *scsi_desc;
	bool skip_ascii = false;

	if (argc > 1 && strcmp(argv[1], "-n") == 0) {
		skip_ascii = true;
		num_lines = LAST_LINE;
	}

	for (int i = 0; i < num_lines; i++) {
		if (!skip_ascii) {
			if (i < ARRAY_SIZE(logo_lines))
				printf("%s ", logo_lines[i]);
			else
				printf("%*c ", LINE_WIDTH, ' ');
		}
		switch (i) {
		case FIRST:
			compatible = ofnode_read_string(ofnode_root(), "compatible");
			if (!compatible)
				compatible = "unknown";
			printf(RESET "%s\n", compatible);
			compatlen = strlen(compatible);
			break;
		case SECOND:
			for (int j = 0; j < compatlen; j++)
				putc('-');
			putc('\n');
			break;
		case KERNEL:
			printf("Kernel:" RESET " %s\n", U_BOOT_VERSION);
			break;
		case SYSINFO:
			printf("Config:" RESET " %s_defconfig\n", CONFIG_SYS_CONFIG_NAME);
			break;
		case HOST:
			model = ofnode_read_string(ofnode_root(), "model");
			if (model)
				printf("Host:" RESET " %s\n", model);
			break;
		case UPTIME:
			printf("Uptime:" RESET " %ld seconds\n", get_timer(0) / 1000);
			break;
		case IP:
			ipaddr = env_get("ipvaddr");
			if (!ipaddr)
				ipaddr = "none";
			printf("IP Address:" RESET " %s", ipaddr);
			ipaddr = env_get("ipv6addr");
			if (ipaddr)
				printf(", %s\n", ipaddr);
			else
				putc('\n');
			break;
		case CMDS:
			n_cmds = ll_entry_count(struct cmd_tbl, cmd);
			printf("Commands:" RESET " %d (help)\n", n_cmds);
			break;
		case CONSOLES:
			printf("Consoles:" RESET " %s (%d baud)\n", env_get("stdout"),
			       gd->baudrate);
			break;
		case DEVICES:
			printf("Devices:" RESET " ");
			/* TODO: walk the DM tree */
			printf("Uncountable!\n");
			break;
		case FEATURES:
			printf("Features:" RESET " ");
			if (IS_ENABLED(CONFIG_NET))
				printf("Net");
			if (IS_ENABLED(CONFIG_EFI_LOADER))
				printf(", EFI");
			printf("\n");
			break;
		case RELOCATION:
			if (gd->flags & GD_FLG_SKIP_RELOC)
				printf("Relocated:" RESET " no\n");
			else
				printf("Relocated:" RESET " to %#09lx\n", gd->relocaddr);
			break;
		case CORES:
			ofnode_for_each_subnode(np, ofnode_path("/cpus")) {
				if (ofnode_name_eq(np, "cpu"))
					n_cpus++;
			}
			printf("CPU:" RESET " %d (1 in use)\n", n_cpus);
			break;
		case MEMORY:
			printf("Memory:" RESET " %lu MB\n", (ulong)gd->ram_size >> 20);
			break;
		case STORAGE:
		default:
			if (i == STORAGE && get_mmc_num() > 0) {
				mmc = find_mmc_device(0);
				if (mmc)
					printf("Storage:" RESET "  mmc 0: %llu MB", mmc->capacity >> 20);
			}
			if (i >= STORAGE + 1) {
				ret = blk_get_desc(UCLASS_SCSI, i - (STORAGE + 1), &scsi_desc);
				if (!ret && scsi_desc->type != DEV_TYPE_UNKNOWN)
					/* The calculation here is really lossy but close enough */
					printf("Storage:" RESET " scsi %d: %lu MB", i - (STORAGE + 1),
					       ((scsi_desc->lba >> 9) * scsi_desc->blksz) >> 11);
			}
			printf("\n");
		}
	}

	printf(RESET "\n\n");

	return 0;
}

U_BOOT_CMD(ufetch, 2, 1, do_ufetch,
	"U-Boot fetch utility",
	"Print information about your device.\n"
	"    -n    Don't print the ASCII logo"
);
