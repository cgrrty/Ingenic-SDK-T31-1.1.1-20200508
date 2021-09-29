#
# (C) Copyright 2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

LIB	:= $(obj)libsfc.o

COBJS-y += jz_sfc_common.o

#ifdef CONFIG_MTD_SFCNOR
#COBJS-y += jz_sfc_nor.o
#COBJS-y += jz_sfc_ops.o
#endif
ifdef CONFIG_MTD_SFCNAND
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/ato_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/gd_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/mxic_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/nand_common.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/winbond_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/xtx_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=nand_device/foresee_nand.o
COBJS-$(CONFIG_MTD_SFCNAND) +=jz_sfc_nand.o
endif


COBJS	:= $(COBJS-y)
SRCS	:= $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS))

all:	$(LIB)

$(LIB):	$(obj).depend $(OBJS)
	$(call cmd_link_o_target, $(OBJS))

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
