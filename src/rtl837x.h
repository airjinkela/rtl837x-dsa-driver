/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef __RTL8372_COMMON_H__
#define __RTL8372_COMMON_H__

#include <linux/of_mdio.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/dsa/8021q.h>
#include <net/dsa.h>

#define MDC_MDIO_CTRL_REG           21
#define MDC_MDIO_ADDR_REG           22
#define MDC_MDIO_DATA_LOW           23
#define MDC_MDIO_DATA_HIGH          24
#define MDC_MDIO_READ_CMD           0x1B
#define MDC_MDIO_WRITE_CMD          0x19

#define RTL837x_C2SIDXMAX       127
#define RTL837x_FIDMAX          15
#define RTL837X_PORT_LED_COUNT  4
#define RTL837X_LED_SET_COUNT   4
#define RTL837X_MAX_PORT_COUNT  8

/* Chip identification */
#define RTL837X_MODEL_NAME_INFO_ADDR                   0x4
#define RTL837X_CHIP_INFO_ADDR                         0xC
#define   RTL837X_CHIP_INFO_RL_VID_MASK                GENMASK(31, 28)
#define   RTL837X_CHIP_INFO_CHIP_INFO_EN_MASK          GENMASK(19, 16)

/* Global reset */
#define RTL837X_RST_GLB_CTRL_0_ADDR                    0x24
#define   RTL837X_RST_GLB_CTRL_0_SDS_REG_RST_MASK      BIT(6)
#define   RTL837X_RST_GLB_CTRL_0_SW_CHIP_RST_MASK      BIT(0)

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

/* PHY configuration */
#define RTL837X_CFG_PHY_MDI_REVERSE_ADDR                              0xA90
#define   RTL837X_CFG_PHY_MDI_REVERSE_P3_MDI_REVERSE_MASK             BIT(3)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P2_MDI_REVERSE_MASK             BIT(2)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P1_MDI_REVERSE_MASK             BIT(1)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P0_MDI_REVERSE_MASK             BIT(0)
#define RTL837X_CFG_PHY_TX_POLARITY_SWAP_ADDR                         0xA94
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P3_TX_POLARITY_SWAP_MASK   GENMASK(15, 12)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P2_TX_POLARITY_SWAP_MASK   GENMASK(11, 8)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P1_TX_POLARITY_SWAP_MASK   GENMASK(7, 4)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P0_TX_POLARITY_SWAP_MASK   GENMASK(3, 0)
#define RTL837X_RS_LAYER_CONFIG_ADDR                                  0xB7C
#define   RTL837X_RS_LAYER_CONFIG_RS_LINK_FAULT_INDI_OFF_MASK         BIT(5)
#define RTL837X_EEE_LPI_DLY_CYCLE_ADDR                                0x954
#define   RTL837X_EEE_LPI_DLY_CYCLE_CFG_WATER_LEVEL_ST_MASK           GENMASK(13, 8)
#define   RTL837X_EEE_LPI_DLY_CYCLE_TX_LPI_DLY_CYCLE_MASK             GENMASK(7, 4)
#define   RTL837X_EEE_LPI_DLY_CYCLE_RX_LPI_DLY_CYCLE_MASK             GENMASK(3, 0)

/* Internal SerDes */
#define RTL837X_SDS_MODE_SEL_ADDR                          0x7B20
#define   RTL837X_SDS_MODE_SEL_CFG_MAC8_8221B_MASK         BIT(22)
#define   RTL837X_SDS_MODE_SEL_CFG_MAC3_8221B_MASK         BIT(21)
#define   RTL837X_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK      GENMASK(20, 16)
#define   RTL837X_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK      GENMASK(14, 10)
#define   RTL837X_SDS_MODE_SEL_SDS1_MODE_SEL_MASK          GENMASK(9, 5)
#define   RTL837X_SDS_MODE_SEL_SDS0_MODE_SEL_MASK          GENMASK(4, 0)
#define RTL837X_SDS_INDACS_CMD_ADDR                        0x3F8
#define   RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK              BIT(15)
#define   RTL837X_SDS_INDACS_CMD_SDS_RWOP_MASK             BIT(14)
#define   RTL837X_SDS_INDACS_CMD_SDS_REGAD_MASK            GENMASK(11, 7)
#define   RTL837X_SDS_INDACS_CMD_SDS_PAGE_MASK             GENMASK(6, 1)
#define   RTL837X_SDS_INDACS_CMD_SDS_INDEX_MASK            BIT(0)
#define RTL837X_SDS_INDACS_RD_ADDR                         0x3FC
#define RTL837X_SDS_INDACS_WD_ADDR                         0x400

/* MIB Control */
#define RTL837X_INDIRECT_ACCESS_CTRL_ADDR                     0xF60
#define   RTL837X_INDIRECT_ACCESS_CTRL_MIB_ID_MASK            GENMASK(10, 5)
#define   RTL837X_INDIRECT_ACCESS_CTRL_PORT_ID_MASK           GENMASK(4, 1)
#define   RTL837X_INDIRECT_ACCESS_CTRL_ACC_CMD_MASK           BIT(0)
#define RTL837X_INDIRECT_ACCESS_CNT_L_ADDR                    0xF64
#define RTL837X_INDIRECT_ACCESS_CNT_H_ADDR                    0xF68

/* Port MAC, link status */
#define RTL837X_MAC_PORT_CTRL_ADDR(_p)                          (0x122C + (((_p) << 8)))
#define   RTL837X_MAC_PORT_CTRL_BKPRES_EN_MASK                  BIT(0)
#define RTL837X_SMI_GLB_CTRL_ADDR                               0x632C
#define   RTL837X_SMI_GLB_CTRL_SMI_POLLING_MASK_MASK            GENMASK(20, 12)
#define RTL837X_SMI_MAC_TYPE_CTRL_ADDR                          0x6330
#define   RTL837X_SMI_MAC_TYPE_CTRL_MAC_PORT8_TYPE_MASK         GENMASK(17, 16)
#define   RTL837X_SMI_MAC_TYPE_CTRL_MAC_PORT3_TYPE_MASK         GENMASK(7, 6)
#define RTL837X_SMI_PORT_POLLING_SEL_ADDR                       0x6334
#define   RTL837X_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL7_MASK    BIT(7)
#define   RTL837X_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL6_MASK    BIT(6)
#define   RTL837X_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL5_MASK    BIT(5)
#define   RTL837X_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL4_MASK    BIT(4)
#define RTL837X_MAC_LINK_STS_ADDR                               0x63E8
#define RTL837X_MAC_LINK_SPD_STS_ADDR(_p)                       (0x63F0 + (((_p >> 3) << 2)))
#define   RTL837X_MAC_LINK_SPD_STS_SPD_STS_9_0_MASK(_p)         (GENMASK(3, 0) << ((_p & 0x7) << 2))
#define RTL837X_MAC_LINK_DUP_STS_ADDR                           0x63F8
#define RTL837X_MAC_TX_PAUSE_STS_ADDR                           0x63FC
#define RTL837X_MAC_RX_PAUSE_STS_ADDR                           0x6400
#define RTL837X_SMI_CTRL_ADDR                                   0x6454
#define   RTL837X_SMI_CTRL_SMI2_MDC_EN_MASK                     BIT(14)
#define   RTL837X_SMI_CTRL_SMI1_MDC_EN_MASK                     BIT(13)
#define   RTL837X_SMI_CTRL_SMI0_MDC_EN_MASK                     BIT(12)

/* Internal phy SMI access */
#define RTL837X_SMI_ACCESS_PHY_CTRL_0_ADDR                      0x6438
#define RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR                      0x643C
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK               GENMASK(26, 24)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK      GENMASK(23, 19)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK       GENMASK(18, 3)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK               BIT(2)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK               BIT(1)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK                BIT(0)
#define RTL837X_SMI_ACCESS_PHY_CTRL_2_ADDR                      0x6440
#define   RTL837X_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK          GENMASK(15, 0)
#define RTL837X_SMI_ACCESS_PHY_CTRL_3_ADDR                      0x6444
#define   RTL837X_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK        GENMASK(15, 0)

/* Internal PHY OCP access */
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR                                  0xBC8
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK        GENMASK(31, 16)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK      GENMASK(8, 4)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK        BIT(2)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK          BIT(1)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK         BIT(0)
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_ADDR                                  0xBCC
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK      GENMASK(15, 0)
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_2_ADDR                                  0xBD0

/* L2 learning control */
#define RTL837X_MAC_L2_GLOBAL_CTRL0_ADDR                          0x5FD4
#define   RTL837X_MAC_L2_GLOBAL_CTRL0_FWD_INVLD_MAC_CTRL_EN_MASK  BIT(20)
#define   RTL837X_MAC_L2_GLOBAL_CTRL0_FWD_UNKN_OPCODE_EN_MASK     BIT(19)
#define RTL837X_MAC_L2_PORT_CTRL_ADDR(_p)                         (0x1238 + (((_p) << 8)))
#define   RTL837X_MAC_L2_PORT_CTRL_CLOCK_SWITCH_MASK              BIT(8)
#define   RTL837X_MAC_L2_PORT_CTRL_RX_CHK_CRC_EN_MASK             BIT(4)

/* EEE control */
#define RTL837X_EEE_CTRL_ADDR(_p)                   (0x125C + (((_p) << 8)))
#define   RTL837X_EEE_CTRL_EEE_RX_STS_MASK          BIT(3)
#define   RTL837X_EEE_CTRL_EEE_TX_STS_MASK          BIT(2)
#define   RTL837X_EEE_CTRL_EEE_PORT_TX_EN_MASK      BIT(1)
#define   RTL837X_EEE_CTRL_EEE_PORT_RX_EN_MASK      BIT(0)

/* CPU tagging */
#define RTL837X_CPU_TAG_CTRL_ADDR                           0x6720
#define   RTL837X_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK    GENMASK(11, 10)
#define   RTL837X_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK           BIT(1)
#define RTL837X_EXT_CPU_CTRL_ADDR                           0x6724
#define   RTL837X_EXT_CPU_CTRL_PORT_MASK                    GENMASK(3, 0)
#define RTL837X_CPU_TAG_AWARE_CTRL_ADDR                     0x603C

/* ITA table access */
#define RTL837X_ITA_CTRL0_ADDR                       0x5CAC
#define   RTL837X_ITA_CTRL0_TBL_ADDR_MASK            GENMASK(28, 16)
#define   RTL837X_ITA_CTRL0_TLB_TYPE_MASK            GENMASK(10, 8)
#define   RTL837X_ITA_CTRL0_TLB_ACT_MASK             BIT(1)
#define   RTL837X_ITA_CTRL0_TLB_EXECUTE_MASK         BIT(0)
#define RTL837X_ITA_L2_CTRL_ADDR                     0x5CB0
#define   RTL837X_ITA_L2_CTRL_PORT_NUM_MASK          GENMASK(22, 19)
#define   RTL837X_ITA_L2_CTRL_ENTRY_CLR_MASK         BIT(18)
#define   RTL837X_ITA_L2_CTRL_READ_MTHD_MASK         GENMASK(17, 14)
#define   RTL837X_ITA_L2_CTRL_ACT_STS_MASK           BIT(12)
#define   RTL837X_ITA_L2_CTRL_TBL_ADDR_MASK          GENMASK(11, 0)
#define RTL837X_ITA_WRITE_DATA0_ADDR(_i)             (0x5CB8 + (((_i) << 2))) /* index: 0-4 */
#define RTL837X_ITA_READ_DATA0_ADDR(_i)              (0x5CCC + (((_i) << 2))) /* index: 0-4 */

/* VLAN and VLAN stacking */
#define RTL837X_VLAN_PORT_AFT_ADDR(_p)                           (0x4E10 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_PORT_AFT_CTAG_ACCEPT_TYPE_MASK(_p)        (GENMASK(1, 0) << ((_p % 0xA) << 1))
#define RTL837X_VLAN_CTRL_ADDR                                   0x4E14
#define   RTL837X_VLAN_CTRL_TABLE_RST_MASK                       BIT(3)
#define   RTL837X_VLAN_CTRL_CVLAN_FILTER_MASK                    BIT(2)
#define RTL837X_VLAN_PORT_IGR_FLTR_ADDR(_p)                      (0x4E18 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_PORT_IGR_FLTR_IGR_FLTR_ACT_MASK(_p)       BIT(_p % 0xA)
#define RTL837X_VLAN_PORT_PB_VLAN_ADDR(_p)                       (0x4E1C + (((_p >> 1) << 2)))
#define   RTL837X_VLAN_PORT_PB_VLAN_PVID_MASK(_p)                (GENMASK(11, 0) << ((_p & 0x1) * 12))
#define RTL837X_VLAN_PORT_EGR_TRANS_ADDR(_p)                     (0x4EB8 + (((_p / 3) << 2)))
#define   RTL837X_VLAN_PORT_EGR_TRANS_PMSK_MASK(_p)              (GENMASK(9, 0) << ((_p % 0x3) * 10))
#define RTL837X_VLAN_PORT_EGR_KEEP_ADDR(_p)                      (0x6728 + (((_p / 3) << 2)))
#define   RTL837X_VLAN_PORT_EGR_KEEP_PMSK_MASK(_p)               (GENMASK(9, 0) << ((_p % 0x3) * 10))
#define RTL837X_VLAN_PORT_EGR_TAG_ADDR(_p)                       (0x6738 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_PORT_EGR_TAG_MODE_MASK(_p)                (GENMASK(1, 0) << ((_p % 0xA) << 1))
#define RTL837X_VLAN_L2_LRN_DIS_ADDR(_i)                         (0x4E30 + (((_i) << 2))) /* index: 0-1 */
#define RTL837X_VS_GLB_CTRL_ADDR                                 0x6044
#define RTL837X_VS_UPLINK_PORT_ADDR                              0x57C0
#define RTL837X_VS_CTRL_ADDR                                     0x57C4
#define   RTL837X_VS_CTRL_SPRISEL_MASK                           GENMASK(4, 3)
#define   RTL837X_VS_CTRL_UIFSEG_MASK                            BIT(2)
#define   RTL837X_VS_CTRL_UNTAG_MASK                             GENMASK(1, 0)
#define RTL837X_VS_PORT_DFLT_SVID_ADDR(_p)                       (0x57CC + (((_p >> 1) << 2)))
#define   RTL837X_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(_p)      (GENMASK(11, 0) << ((_p & 0x1) * 12))
#define RTL837X_SVLAN_TRAP_CTRL_ADDR                             0x4EC8
#define   RTL837X_SVLAN_TRAP_CTRL_CPU_PMSK_MASK                  GENMASK(17, 16)
#define RTL837X_VLAN_C2S_ENTRY_ADDR(_i)                          (0x57E0 + (((_i) << 3))) /* RTL837x_C2SIDXMAX */

/* Spanning tree */
#define RTL837X_MSPT_STATE_ADDR(_fid)       (0x5310 + (((_fid) << 2))) /* RTL837x_FIDMAX */
#define RTL837X_MSTP_STATE(port, state) \
	((state) << ((port) * 2))
#define RTL837X_MSTP_STATE_MASK(port) \
	RTL837X_MSTP_STATE((port), GENMASK(1, 0))

/* L2 table maintenance */
#define RTL837X_L2_LRN_PORT_CONSTRT_ACT_ADDR                   0x4F80
#define   RTL837X_L2_LRN_PORT_CONSTRT_ACT_LRN_ACT_MASK         GENMASK(1, 0)
#define RTL837X_L2_TBL_FLUSH_CMD_ADDR                          0x53D4
#define   RTL837X_L2_TBL_FLUSH_CMD_FLUSH_BUSY_MASK             BIT(17)
#define   RTL837X_L2_TBL_FLUSH_CMD_FLUSH_ACT_MASK              BIT(16)
#define   RTL837X_L2_TBL_FLUSH_CMD_FLUSH_PMSK_MASK             GENMASK(9, 0)
#define RTL837X_L2_TBL_FLUSH_ALL_ADDR                          0x53D8
#define   RTL837X_L2_TBL_FLUSH_ALL_FLUSH_ALL_MASK              BIT(0)
#define RTL837X_L2_TBL_FLUSH_MODE_ADDR                         0x53DC
#define   RTL837X_L2_TBL_FLUSH_MODE_FLUSH_MODE_MASK            GENMASK(1, 0)

/* Isolation */
#define RTL837X_PORT_ISO_PORT_PMSK_ADDR(_p)                    (0x50C0 + (((_p) << 2)))
#define   RTL837X_PORT_ISO_PORT_PMSK_PMSK_MASK                 GENMASK(9, 0)

/* Mirroring */
#define RTL837X_MIR_CTRL_ADDR                                  0x50E8
#define   RTL837X_MIR_CTRL_MIR_RX_ISOLATE_LKY_MASK             BIT(6)
#define   RTL837X_MIR_CTRL_MIR_TX_ISOLATE_LKY_MASK             BIT(5)
#define   RTL837X_MIR_CTRL_MIR_RX_VLAN_LKY_MASK                BIT(4)
#define   RTL837X_MIR_CTRL_MIR_TX_VLAN_LKY_MASK                BIT(3)

/* Flow control */
#define RTL837X_FC_PORT_ACT_CTRL_ADDR(_p)                      (0x7124 + (((_p) << 2)))
#define   RTL837X_FC_PORT_ACT_CTRL_ACT_MASK                                                                   BIT(12)
#define   RTL837X_FC_PORT_ACT_CTRL_ALLOW_PAGE_CNT_MASK                                                        GENMASK(11, 0)

/* Internal CPU */
#define RTL837X_DW8051_CFG_ADDR                             0x6040
#define   RTL837X_DW8051_CFG_DW8051_READY_MASK              BIT(0)

/*
 * As Realtek has not released the register manual
 * for the internal SerDes, the register definitions
 * below have been inferred from comments and names
 * found in OEM code and code for similar IP chips.
 */
/* PAGE_FRC */
#define SDS_PAGE_FRC            0x20
#define SDS_REG_FRC             0x00
#define   SDS_FRC_RX_EN_ON_MASK    BIT(4)
#define   SDS_FRC_RX_EN_VAL_MASK   BIT(5)
#define   SDS_FRC_PDOWN_ON_MASK    BIT(6)
#define   SDS_FRC_PDOWN_VAL_MASK   BIT(7)
#define   SDS_FRC_CMU_EN_ON_MASK   BIT(10)
#define   SDS_FRC_CMU_EN_VAL_MASK  BIT(11)

/* PAGE_NWAY_AN */
#define SDS_PAGE_NWAY_AN   0x07
#define SDS_REG_NWAY_AN    17
#define   SDS_NWAY_QHSG_AN_CH0_EN_MASK BIT(0)
#define   SDS_NWAY_QHSG_AN_CH1_EN_MASK BIT(1)
#define   SDS_NWAY_QHSG_AN_CH2_EN_MASK BIT(2)
#define   SDS_NWAY_QHSG_AN_CH3_EN_MASK BIT(3)

/* PAGE_CTRL00 */
#define SDS_PAGE_CTRL00    0x00
#define SDS_REG_CTRL00_REG00     0x00
#define   SDS_CTRL00_REG00_XSG_TX_INV_MASK  BIT(8)
#define   SDS_CTRL00_REG00_XSG_RX_INV_MASK  BIT(9)

/*	I Guess
 *                         Force_En  Force_Dis Auto
 * BIT8   SP_SDS_FRC_AN       1        1        0
 * BIT9   SP_SDS_FRC_AN_EN    1        0        x
 *                            3        1        0
 * 0: NWAY_AUTO
 * 1: NWAY_FORCE_DIS
 * 3: NWAY_FORCE_EN (maybe?)
 */
#define SDS_REG_CTRL00_REG02     0x02
#define   SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN    BIT(8)
#define   SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN_EN BIT(9)

#define SDS_REG_CTRL00_REG04     0x04
#define   SDS_CTRL00_REG04_SP_CFG_EN_LINK_FIB1G_MASK   BIT(2)

/* PAGE_CTRL01 */
#define SDS_PAGE_CTRL01             0x01
#define SDS_REG_CTRL01_XSG_STS      0x1d     // I Guess
#define   SDS_CTRL01_XSG_STS_SYNC_OK   BIT(0) // I Guess
#define   SDS_CTRL01_XSG_STS_LINK_OK   BIT(4) // I Guess
#define   SDS_CTRL01_XSG_STS_SIG_OK    BIT(8) // I Guess

/* PAGE_CTRL02 */
#define SDS_PAGE_CTRL02       0x02
// It seems like 'Advertisement control register'
#define SDS_REG_CTRL02_XSG_AN   0x04
#define   SDS_CTRL02_XSG_AN_10_100_AsymmetricPause_MASK BIT(11)
#define   SDS_CTRL02_XSG_AN_10_100_Pause_MASK           BIT(10)
#define   SDS_CTRL02_XSG_AN_1G_AsymmetricPause_MASK BIT(8)
#define   SDS_CTRL02_XSG_AN_1G_Pause_MASK           BIT(7)
#define   SDS_CTRL02_XSG_AN_1G_HalfDuplex_MASK      BIT(6)
#define   SDS_CTRL02_XSG_AN_1G_FullDuplex_MASK      BIT(5)

/* PAGE_CTRL05 */
#define SDS_PAGE_CTRL05             0x05
#define SDS_REG_CTRL05_10GR_STS     0x00       // I Guess
#define   SDS_CTRL05_10GR_STS_SYNC_OK   BIT(0)  // I Guess
#define   SDS_CTRL05_10GR_STS_HI_BER    BIT(1)  // I Guess
#define   SDS_CTRL05_10GR_STS_LINK_OK   BIT(12) // I Guess

/* PAGE_CTRL06 */
#define SDS_PAGE_CTRL06       0x06
#define SDS_REG_CTRL06_REG02   0x02
#define   SDS_CTRL06_REG02_FSM_RESET_MASK    BIT(12)
#define   SDS_CTRL06_REG02_10GR_RX_INV_MASK  BIT(13)
#define   SDS_CTRL06_REG02_10GR_TX_INV_MASK  BIT(14)

/* PAGE_CTRL1F */
#define SDS_PAGE_CTRL1F       0x1f
#define SDS_REG_CTRL1F_10GR_AN  0x0B
#define   SDS_CTRL1F_10GR_AN_Pause_MASK     BIT(2)
#define   SDS_CTRL1F_10GR_AN_AsymmetricPause_MASK  BIT(3)

// deprecated
enum rtk_sds_mode {
	SERDES_10GQXG,
	SERDES_10GUSXG = 0xD,
	SERDES_10GR = 0x1A,
	SERDES_HSG = 0x12,
	SERDES_2500BASEX = 0x16,
	SERDES_SG = 2,
	SERDES_1000BASEX = 4,
	SERDES_100FX = 5,
	SERDES_OFF = 0x1F,
	SERDES_8221B = 0x21,
	SERDES_ON = 0x22,
	SERDES_END
};

enum _rtk_tb_op {
	TB_OP_READ = 0,
	TB_OP_WRITE
};

enum _rtk_tb_access_execute {
	TB_NOT_EXECUTE = 0,
	TB_EXECUTE,
};

enum _rtk_tb_access_target {
	TB_TARGET_ACLRULE = 1,
	TB_TARGET_ACLACT,
	TB_TARGET_CVLAN,
	TB_TARGET_L2,
	TB_TARGET_IGMP_GROUP,
	TB_TARGET_HSA,
	TB_TARGET_HSB
};

enum switch_chip {
	CHIP_RTL8373 = 0,
	CHIP_RTL8224 = 1,
	CHIP_RTL8372,
	CHIP_RTL8373N,
	CHIP_RTL8221B,
	CHIP_RTL8366U,
	CHIP_RTL8372N,
	CHIP_RTL8224N,
	CHIP_END
};

struct rtl837x_mib_counter {
	unsigned int	offset;
	unsigned int	length;
	const char	*name;
};

struct rtl837x_led_set {
	struct rtl837x_priv *priv;
	u8 idx;
	u32 led_cfg_mask[RTL837X_PORT_LED_COUNT];
	atomic_t refcnt;
};

struct rtl837x_led {
	u8 port_num;
	u8 led_id;
	u8 led_pin;
	bool is_hw_offload;
	struct rtl837x_led_set *led_set;
	struct rtl837x_priv *priv;
	struct led_classdev cdev;
};

struct rtl837x_priv {
	struct device *dev;
	struct gpio_desc	*reset;
	struct mii_bus *bus;
	struct regmap		*map;
	struct mutex		map_lock;
	struct regmap		*map_8224;
	struct mutex		map_8224_lock;

	struct mutex		ita_lock;

	int			mdio_addr;
	enum dsa_tag_protocol tag_proto;

	struct dentry *debugfs_parent;

	u32 chip_ver;
	u32 chip_ver_8224;

	unsigned int num_ports;

	struct dsa_switch	*ds;

	const struct rtl837x_mib_counter *mib_counters;
	unsigned int num_mib_counters;
	struct mutex mib_lock;

	struct rtl837x_led_set led_set[RTL837X_LED_SET_COUNT];
	struct rtl837x_led ports_led[RTL837X_PORT_LED_COUNT*RTL837X_MAX_PORT_COUNT];

	const struct rtl837x_ops *ops;
	int			(*write_reg_noack)(void *ctx, u32 addr, u32 data);

	void			*chip_data; /* Per-chip extra variant data */
};

struct rtl837x_vlan_4k {
	u16	vid;
	u16	untag;
	u16	member;
	u8	fid;
};

struct rtl837x_vlan_data {
	u16 vid;
	union {
		struct {
			u32 mbr  : 10;
			u32 untag: 10;
			u32 fid  : 4;
			u32 svlan_chk_ivl_svl: 1;
			u32 ivl_en: 1;
			u32 resv : 6;
		};
		u32 val;
	};
};

enum rtl837x_l2_method {
	LUT_READ_METHOD_MAC = 0,
	LUT_READ_METHOD_ADDRESS,
	LUT_READ_METHOD_NEXT_ADDRESS,
	LUT_READ_METHOD_NEXT_L2UC,
	LUT_READ_METHOD_NEXT_L2MC,
	LUT_READ_METHOD_NEXT_L3MC,
	LUT_READ_METHOD_NEXT_L2L3MC,
	LUT_READ_METHOD_NEXT_L2UCSPA,
};

enum rtl837x_lut_type {
	LUT_TYPE_L2_UC = 1,
	LUT_TYPE_L2_MC,
	LUT_TYPE_L3,
};

struct rtl837x_l2_key {
	u8 mac_addr[ETH_ALEN];
	u16 vid_fid;
	bool ivl;
};

struct rtl837x_l2_uc {
	struct rtl837x_l2_key key;
	u8 port;
	u8 age;

	bool auth; // 802.1X: not used in this driver
	bool is_static;
};

struct rtl837x_l2_mc {
	struct rtl837x_l2_key key;
	u16 mbr;

	bool igmp_asic;
	u8 igmp_idx;
};

struct rtl837x_l3 {
	u32 sip;
	u32 dip;

	bool l3lookup;
	u16 mbr;
	bool igmp_asic;
	u8 igmp_idx;
};

struct rtl837x_lut_entry {
	enum rtl837x_lut_type type;
	u16 addr;
	union {
		struct rtl837x_l2_uc uc;
		struct rtl837x_l2_mc mc;
		struct rtl837x_l3 l3;
	};
};

struct rtl837x_variant {
	const struct dsa_switch_ops *ds_ops_mdio;
	const struct rtl837x_ops *ops;
	const enum dsa_tag_protocol def_tag_proto;
	const struct phylink_mac_ops *pl_mac_ops;
	const bool have_8224;
	size_t chip_data_sz;
};

struct rtl837x_ops {
	int	(*detect)(struct rtl837x_priv *priv);
	int	(*reset_chip)(struct rtl837x_priv *priv);

	int	(*get_mib_counter)(struct rtl837x_priv *priv,
					int port,
					const struct rtl837x_mib_counter *mib,
					u64 *mibvalue);

	int	(*get_vlan_4k)(struct rtl837x_priv *priv, u32 vid,
			       struct rtl837x_vlan_4k *vlan4k);
	int	(*set_vlan_4k)(struct rtl837x_priv *priv,
			       const struct rtl837x_vlan_4k *vlan4k);
	int	(*phy_read_c22)(struct rtl837x_priv *priv, u16 phy, int regnum,
				u16 *pval);
	int	(*phy_write_c22)(struct rtl837x_priv *priv, u16 phy, int regnum,
				u16 val);

	int	(*phy_read_c45)(struct rtl837x_priv *priv, int phy, int devad, int regnum,
				u16 *pval);
	int	(*phy_write_c45)(struct rtl837x_priv *priv, int phy, int devad, int regnum,
				u16 val);
};

char *chipid_to_chip_name(enum switch_chip id);

#define rtl837x_reg_read(priv, reg, pval) regmap_read(priv->map, reg, pval)
#define rtl837x_reg_write(priv, reg, val) regmap_write(priv->map, reg, val)

extern int rtl837x_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);

extern int rtl837x_gpiochip_init(struct rtl837x_priv *priv);
extern int rtl837x_set_led(struct rtl837x_priv *priv);
extern enum rtk_sds_mode phy_interface_to_rtk_sds_mode(phy_interface_t interface);

extern int rtl837x_debug_proc_init(struct rtl837x_priv *priv);
extern int rtl837x_debug_proc_deinit(struct rtl837x_priv *priv);

extern int rtl837x_phy_read_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval);
extern int rtl837x_phy_write_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val);
extern int rtl837x_phy_read_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval);
extern int rtl837x_phy_write_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val);
extern int rtl837x_phy_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 *pval);
extern int rtl837x_phys_write_c45(struct rtl837x_priv *priv, u16 phy_mask, int devad, int regnum, u16 val);
extern int rtl837x_phy_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 val);
extern int rtl837x_phy_bits_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 *pdata);
extern int rtl837x_phy_bits_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 data);

extern int rtl837x_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata);
extern int rtl837x_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data);
extern int rtl837x_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data);
extern int rtl837x_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata);

#define rtl837x_rtl8224_reg_read(priv, reg, pval) (priv->map_8224 ? regmap_read(priv->map_8224, reg, pval) : -ENODEV)
#define rtl837x_rtl8224_reg_write(priv, reg, val) (priv->map_8224 ? regmap_write(priv->map_8224, reg, val) : -ENODEV)
extern int rtl837x_rtl8224_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_rtl8224_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);
extern int rtl837x_rtl8224_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata);
extern int rtl837x_rtl8224_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data);
extern int rtl837x_rtl8224_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata);
extern int rtl837x_rtl8224_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data);

extern int rtl837x_vlan_set(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan);
extern int rtl837x_vlan_get(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan);

extern int rtl837x_lut_query(struct rtl837x_priv *priv,
			  enum rtl837x_l2_method method,
			  struct rtl837x_lut_entry *entry);
extern int rtl837x_lut_set(struct rtl837x_priv *priv,
			  struct rtl837x_lut_entry *entry);
extern int rtl837x_lut_del(struct rtl837x_priv *priv,
				  u32 addr);

extern int rtl837x_sds_reset_X(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_rtl8224_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx);

extern int __deprecated rtl837x_serdes_set_mode(struct rtl837x_priv *priv, u8 sds_idx, enum rtk_sds_mode mode);

extern int rtl837x_serdes_on(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_serdes_off(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_serdes_an_patch(struct rtl837x_priv *priv, u8 sds_idx, phy_interface_t interface);
extern int rtl837x_serdes_mac_patch(struct rtl837x_priv *priv, u8 sds_idx);

#if defined(RTL837X_PHY_PATCH)
extern int patch_phys_v008(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v008_rls_lockmain(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v009(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v009_rls_lockmain(struct rtl837x_priv *priv, u16 phy_mask);
#endif

extern const struct rtl837x_variant rtl8372n_variant;

#endif
