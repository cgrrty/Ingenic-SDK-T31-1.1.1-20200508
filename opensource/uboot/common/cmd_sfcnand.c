/*
 * cmd_nand_zm.c
 *
 * NAND cmd for which nand support the way of zone manager;
 *
 * Copyright (c) 2005-2008 Ingenic Semiconductor Inc.
 *
 */
#include <common.h>
#include <command.h>
#include <nand.h>
#define X_COMMAND_LENGTH 128


int do_sfcnand(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	char *cmd;
	unsigned int dst_addr,offset,len;
	char command[X_COMMAND_LENGTH];
	int ret;

	cmd = argv[1];

	if(argc != 5)
	{
		printf("ERROR: argv error,please check the param of cmd !!!\n");
		return CMD_RET_USAGE;
	}

	offset = (unsigned int)simple_strtoul(argv[2], NULL, 16);
	len = (unsigned int)simple_strtoul(argv[3], NULL, 16);
	dst_addr = (unsigned int)simple_strtoul(argv[4], NULL, 16);

	memset(command,0,X_COMMAND_LENGTH);
	sprintf(command,"nand %s.jffs2 0x%x 0x%x 0x%x",cmd,dst_addr,offset,len);

	ret = run_command(command,0);
	if(ret)
		printf("do sfcnand read error ! please check your param !!\n");

	return CMD_RET_SUCCESS;
}
U_BOOT_CMD(sfcnand, 5, 1, do_sfcnand,
		"sfcnand    - SFC_NAND sub-system\n",
		"sfcnand read from(offs) size dst_addr\n"
		);
void sfc_nand_init(void)
{
	struct nand_chip *chip;
	struct mtd_info *mtd;
	mtd = &nand_info[0];

	jz_sfc_nand_init(0,NULL);

	chip =mtd->priv;
	chip->scan_bbt(mtd);
	chip->options |= NAND_BBT_SCANNED;
}
