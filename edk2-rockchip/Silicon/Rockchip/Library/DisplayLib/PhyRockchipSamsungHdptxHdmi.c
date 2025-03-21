/** @file

  Rockchip HDMI/DP Combo PHY with Samsung IP block
  Copyright (c) 2022, Linaro, Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DwHdmiQpLib.h>
#include <Library/PWMLib.h>
#include <Library/DrmModes.h>

#include <Library/uboot-env.h>

#define UPDATE(x, h, l)  (((x) << (l)) & GENMASK((h), (l)))

#define GRF_HDPTX_CON0         0x00
#define LC_REF_CLK_SEL         BIT(11)
#define HDPTX_I_PLL_EN         BIT(7)
#define HDPTX_I_BIAS_EN        BIT(6)
#define HDPTX_I_BGR_EN         BIT(5)
#define GRF_HDPTX_STATUS       0x80
#define HDPTX_O_PLL_LOCK_DONE  BIT(3)
#define HDPTX_O_PHY_CLK_RDY    BIT(2)
#define HDPTX_O_PHY_RDY        BIT(1)
#define HDPTX_O_SB_RDY         BIT(0)

#define CMN_REG0000    0x0000
#define CMN_REG0001    0x0004
#define CMN_REG0002    0x0008
#define CMN_REG0003    0x000C
#define CMN_REG0004    0x0010
#define CMN_REG0005    0x0014
#define CMN_REG0006    0x0018
#define CMN_REG0007    0x001C
#define CMN_REG0008    0x0020
#define LCPLL_EN_MASK  BIT(6)
#define LCPLL_EN(x)  UPDATE(x, 4, 4)
#define LCPLL_LCVCO_MODE_EN_MASK  BIT(4)
#define LCPLL_LCVCO_MODE_EN(x)  UPDATE(x, 4, 4)
#define CMN_REG0009       0x0024
#define CMN_REG000A       0x0028
#define CMN_REG000B       0x002C
#define CMN_REG000C       0x0030
#define CMN_REG000D       0x0034
#define CMN_REG000E       0x0038
#define CMN_REG000F       0x003C
#define CMN_REG0010       0x0040
#define CMN_REG0011       0x0044
#define CMN_REG0012       0x0048
#define CMN_REG0013       0x004C
#define CMN_REG0014       0x0050
#define CMN_REG0015       0x0054
#define CMN_REG0016       0x0058
#define CMN_REG0017       0x005C
#define CMN_REG0018       0x0060
#define CMN_REG0019       0x0064
#define CMN_REG001A       0x0068
#define CMN_REG001B       0x006C
#define CMN_REG001C       0x0070
#define CMN_REG001D       0x0074
#define CMN_REG001E       0x0078
#define LCPLL_PI_EN_MASK  BIT(5)
#define LCPLL_PI_EN(x)  UPDATE(x, 5, 5)
#define LCPLL_100M_CLK_EN_MASK  BIT(0)
#define LCPLL_100M_CLK_EN(x)  UPDATE(x, 0, 0)
#define CMN_REG001F           0x007C
#define CMN_REG0020           0x0080
#define CMN_REG0021           0x0084
#define CMN_REG0022           0x0088
#define CMN_REG0023           0x008C
#define CMN_REG0024           0x0090
#define CMN_REG0025           0x0094
#define LCPLL_PMS_IQDIV_RSTN  BIT(4)
#define CMN_REG0026           0x0098
#define CMN_REG0027           0x009C
#define CMN_REG0028           0x00A0
#define LCPLL_SDC_FRAC_EN     BIT(2)
#define LCPLL_SDC_FRAC_RSTN   BIT(0)
#define CMN_REG0029           0x00A4
#define CMN_REG002A           0x00A8
#define CMN_REG002B           0x00AC
#define CMN_REG002C           0x00B0
#define CMN_REG002D           0x00B4
#define LCPLL_SDC_N_MASK      GENMASK(3, 1)
#define LCPLL_SDC_N(x)  UPDATE(x, 3, 1)
#define CMN_REG002E                0x00B8
#define LCPLL_SDC_NUMBERATOR_MASK  GENMASK(5, 0)
#define LCPLL_SDC_NUMBERATOR(x)  UPDATE(x, 5, 0)
#define CMN_REG002F                 0x00BC
#define LCPLL_SDC_DENOMINATOR_MASK  GENMASK(7, 2)
#define LCPLL_SDC_DENOMINATOR(x)  UPDATE(x, 7, 2)
#define LCPLL_SDC_NDIV_RSTN   BIT(0)
#define CMN_REG0030           0x00C0
#define CMN_REG0031           0x00C4
#define CMN_REG0032           0x00C8
#define CMN_REG0033           0x00CC
#define CMN_REG0034           0x00D0
#define CMN_REG0035           0x00D4
#define CMN_REG0036           0x00D8
#define CMN_REG0037           0x00DC
#define CMN_REG0038           0x00E0
#define CMN_REG0039           0x00E4
#define CMN_REG003A           0x00E8
#define CMN_REG003B           0x00EC
#define CMN_REG003C           0x00F0
#define CMN_REG003D           0x00F4
#define ROPLL_LCVCO_EN        BIT(4)
#define CMN_REG003E           0x00F8
#define CMN_REG003F           0x00FC
#define CMN_REG0040           0x0100
#define CMN_REG0041           0x0104
#define CMN_REG0042           0x0108
#define CMN_REG0043           0x010C
#define CMN_REG0044           0x0110
#define CMN_REG0045           0x0114
#define CMN_REG0046           0x0118
#define CMN_REG0047           0x011C
#define CMN_REG0048           0x0120
#define CMN_REG0049           0x0124
#define CMN_REG004A           0x0128
#define CMN_REG004B           0x012C
#define CMN_REG004C           0x0130
#define CMN_REG004D           0x0134
#define CMN_REG004E           0x0138
#define ROPLL_PI_EN           BIT(5)
#define CMN_REG004F           0x013C
#define CMN_REG0050           0x0140
#define CMN_REG0051           0x0144
#define CMN_REG0052           0x0148
#define CMN_REG0053           0x014C
#define CMN_REG0054           0x0150
#define CMN_REG0055           0x0154
#define CMN_REG0056           0x0158
#define CMN_REG0057           0x015C
#define CMN_REG0058           0x0160
#define CMN_REG0059           0x0164
#define CMN_REG005A           0x0168
#define CMN_REG005B           0x016C
#define CMN_REG005C           0x0170
#define ROPLL_PMS_IQDIV_RSTN  BIT(5)
#define CMN_REG005D           0x0174
#define CMN_REG005E           0x0178
#define ROPLL_SDM_EN_MASK     BIT(6)
#define ROPLL_SDM_EN(x)  UPDATE(x, 6, 6)
#define ROPLL_SDM_FRAC_EN_RBR        BIT(3)
#define ROPLL_SDM_FRAC_EN_HBR        BIT(2)
#define ROPLL_SDM_FRAC_EN_HBR2       BIT(1)
#define ROPLL_SDM_FRAC_EN_HBR3       BIT(0)
#define CMN_REG005F                  0x017C
#define CMN_REG0060                  0x0180
#define CMN_REG0061                  0x0184
#define CMN_REG0062                  0x0188
#define CMN_REG0063                  0x018C
#define CMN_REG0064                  0x0190
#define ROPLL_SDM_NUM_SIGN_RBR_MASK  BIT(3)
#define ROPLL_SDM_NUM_SIGN_RBR(x)  UPDATE(x, 3, 3)
#define CMN_REG0065           0x0194
#define CMN_REG0066           0x0198
#define CMN_REG0067           0x019C
#define CMN_REG0068           0x01A0
#define CMN_REG0069           0x01A4
#define ROPLL_SDC_N_RBR_MASK  GENMASK(2, 0)
#define ROPLL_SDC_N_RBR(x)  UPDATE(x, 2, 0)
#define CMN_REG006A               0x01A8
#define CMN_REG006B               0x01AC
#define CMN_REG006C               0x01B0
#define CMN_REG006D               0x01B4
#define CMN_REG006E               0x01B8
#define CMN_REG006F               0x01BC
#define CMN_REG0070               0x01C0
#define CMN_REG0071               0x01C4
#define CMN_REG0072               0x01C8
#define CMN_REG0073               0x01CC
#define CMN_REG0074               0x01D0
#define ROPLL_SDC_NDIV_RSTN       BIT(2)
#define ROPLL_SSC_EN              BIT(0)
#define CMN_REG0075               0x01D4
#define CMN_REG0076               0x01D8
#define CMN_REG0077               0x01DC
#define CMN_REG0078               0x01E0
#define CMN_REG0079               0x01E4
#define CMN_REG007A               0x01E8
#define CMN_REG007B               0x01EC
#define CMN_REG007C               0x01F0
#define CMN_REG007D               0x01F4
#define CMN_REG007E               0x01F8
#define CMN_REG007F               0x01FC
#define CMN_REG0080               0x0200
#define CMN_REG0081               0x0204
#define OVRD_PLL_CD_CLK_EN        BIT(8)
#define PLL_CD_HSCLK_EAST_EN      BIT(0)
#define CMN_REG0082               0x0208
#define CMN_REG0083               0x020C
#define CMN_REG0084               0x0210
#define CMN_REG0085               0x0214
#define CMN_REG0086               0x0218
#define PLL_PCG_POSTDIV_SEL_MASK  GENMASK(7, 4)
#define PLL_PCG_POSTDIV_SEL(x)  UPDATE(x, 7, 4)
#define PLL_PCG_CLK_SEL_MASK  GENMASK(3, 1)
#define PLL_PCG_CLK_SEL(x)  UPDATE(x, 3, 1)
#define PLL_PCG_CLK_EN          BIT(0)
#define CMN_REG0087             0x021C
#define PLL_FRL_MODE_EN         BIT(3)
#define PLL_TX_HS_CLK_EN        BIT(2)
#define CMN_REG0088             0x0220
#define CMN_REG0089             0x0224
#define LCPLL_ALONE_MODE        BIT(1)
#define CMN_REG008A             0x0228
#define CMN_REG008B             0x022C
#define CMN_REG008C             0x0230
#define CMN_REG008D             0x0234
#define CMN_REG008E             0x0238
#define CMN_REG008F             0x023C
#define CMN_REG0090             0x0240
#define CMN_REG0091             0x0244
#define CMN_REG0092             0x0248
#define CMN_REG0093             0x024C
#define CMN_REG0094             0x0250
#define CMN_REG0095             0x0254
#define CMN_REG0096             0x0258
#define CMN_REG0097             0x025C
#define DIG_CLK_SEL             BIT(1)
#define ROPLL_REF               BIT(1)
#define LCPLL_REF               0
#define CMN_REG0098             0x0260
#define CMN_REG0099             0x0264
#define CMN_ROPLL_ALONE_MODE    BIT(2)
#define ROPLL_ALONE_MODE        BIT(2)
#define CMN_REG009A             0x0268
#define HS_SPEED_SEL            BIT(0)
#define DIV_10_CLOCK            BIT(0)
#define CMN_REG009B             0x026C
#define IS_SPEED_SEL            BIT(4)
#define LINK_SYMBOL_CLOCK       BIT(4)
#define LINK_SYMBOL_CLOCK1_2    0
#define CMN_REG009C             0x0270
#define CMN_REG009D             0x0274
#define CMN_REG009E             0x0278
#define CMN_REG009F             0x027C
#define CMN_REG00A0             0x0280
#define CMN_REG00A1             0x0284
#define CMN_REG00A2             0x0288
#define CMN_REG00A3             0x028C
#define CMN_REG00AD             0x0290
#define CMN_REG00A5             0x0294
#define CMN_REG00A6             0x0298
#define CMN_REG00A7             0x029C
#define SB_REG0100              0x0400
#define SB_REG0101              0x0404
#define SB_REG0102              0x0408
#define OVRD_SB_RXTERM_EN_MASK  BIT(5)
#define OVRD_SB_RXTERM_EN(x)  UPDATE(x, 5, 5)
#define SB_RXTERM_EN_MASK  BIT(4)
#define SB_RXTERM_EN(x)  UPDATE(x, 4, 4)
#define ANA_SB_RXTERM_OFFSP_MASK  GENMASK(3, 0)
#define ANA_SB_RXTERM_OFFSP(x)  UPDATE(x, 3, 0)
#define SB_REG0103                0x040C
#define ANA_SB_RXTERM_OFFSN_MASK  GENMASK(6, 3)
#define ANA_SB_RXTERM_OFFSN(x)  UPDATE(x, 6, 3)
#define OVRD_SB_RX_RESCAL_DONE_MASK  BIT(1)
#define OVRD_SB_RX_RESCAL_DONE(x)  UPDATE(x, 1, 1)
#define SB_RX_RESCAL_DONE_MASK  BIT(0)
#define SB_RX_RESCAL_DONE(x)  UPDATE(x, 0, 0)
#define SB_REG0104       0x0410
#define OVRD_SB_EN_MASK  BIT(5)
#define OVRD_SB_EN(x)  UPDATE(x, 5, 5)
#define SB_EN_MASK  BIT(4)
#define SB_EN(x)  UPDATE(x, 4, 4)
#define SB_REG0105                 0x0414
#define OVRD_SB_EARC_CMDC_EN_MASK  BIT(6)
#define OVRD_SB_EARC_CMDC_EN(x)  UPDATE(x, 6, 6)
#define SB_EARC_CMDC_EN_MASK  BIT(5)
#define SB_EARC_CMDC_EN(x)  UPDATE(x, 5, 5)
#define ANA_SB_TX_HLVL_PROG_MASK  GENMASK(2, 0)
#define ANA_SB_TX_HLVL_PROG(x)  UPDATE(x, 2, 0)
#define SB_REG0106                0x0418
#define ANA_SB_TX_LLVL_PROG_MASK  GENMASK(6, 4)
#define ANA_SB_TX_LLVL_PROG(x)  UPDATE(x, 6, 4)
#define SB_REG0107                      0x041C
#define SB_REG0108                      0x0420
#define SB_REG0109                      0x0424
#define ANA_SB_DMRX_AFC_DIV_RATIO_MASK  GENMASK(2, 0)
#define ANA_SB_DMRX_AFC_DIV_RATIO(x)  UPDATE(x, 2, 0)
#define SB_REG010A            0x0428
#define SB_REG010B            0x042C
#define SB_REG010C            0x0430
#define SB_REG010D            0x0434
#define SB_REG010E            0x0438
#define SB_REG010F            0x043C
#define OVRD_SB_VREG_EN_MASK  BIT(7)
#define OVRD_SB_VREG_EN(x)  UPDATE(x, 7, 7)
#define SB_VREG_EN_MASK  BIT(6)
#define SB_VREG_EN(x)  UPDATE(x, 6, 6)
#define OVRD_SB_VREG_LPF_BYPASS_MASK  BIT(5)
#define OVRD_SB_VREG_LPF_BYPASS(x)  UPDATE(x, 5, 5)
#define SB_VREG_LPF_BYPASS_MASK  BIT(4)
#define SB_VREG_LPF_BYPASS(x)  UPDATE(x, 4, 4)
#define ANA_SB_VREG_GAIN_CTRL_MASK  GENMASK(3, 0)
#define ANA_SB_VREG_GAIN_CTRL(x)  UPDATE(x, 3, 0)
#define SB_REG0110                0x0440
#define ANA_SB_VREG_REF_SEL_MASK  BIT(0)
#define ANA_SB_VREG_REF_SEL(x)  UPDATE(x, 0, 0)
#define SB_REG0111                0x0444
#define SB_REG0112                0x0448
#define SB_REG0113                0x044C
#define SB_RX_RCAL_OPT_CODE_MASK  GENMASK(5, 4)
#define SB_RX_RCAL_OPT_CODE(x)  UPDATE(x, 5, 4)
#define SB_RX_RTERM_CTRL_MASK  GENMASK(3, 0)
#define SB_RX_RTERM_CTRL(x)  UPDATE(x, 3, 0)
#define SB_REG0114                   0x0450
#define SB_TG_SB_EN_DELAY_TIME_MASK  GENMASK(5, 3)
#define SB_TG_SB_EN_DELAY_TIME(x)  UPDATE(x, 5, 3)
#define SB_TG_RXTERM_EN_DELAY_TIME_MASK  GENMASK(2, 0)
#define SB_TG_RXTERM_EN_DELAY_TIME(x)  UPDATE(x, 2, 0)
#define SB_REG0115                0x0454
#define SB_READY_DELAY_TIME_MASK  GENMASK(5, 3)
#define SB_READY_DELAY_TIME(x)  UPDATE(x, 5, 3)
#define SB_TG_OSC_EN_DELAY_TIME_MASK  GENMASK(2, 0)
#define SB_TG_OSC_EN_DELAY_TIME(x)  UPDATE(x, 2, 0)
#define SB_REG0116                0x0458
#define AFC_RSTN_DELAY_TIME_MASK  GENMASK(6, 4)
#define AFC_RSTN_DELAY_TIME(x)  UPDATE(x, 6, 4)
#define SB_REG0117            0x045C
#define FAST_PULSE_TIME_MASK  GENMASK(3, 0)
#define FAST_PULSE_TIME(x)  UPDATE(x, 3, 0)
#define SB_REG0118                   0x0460
#define SB_REG0119                   0x0464
#define SB_REG011A                   0x0468
#define SB_REG011B                   0x046C
#define SB_EARC_SIG_DET_BYPASS_MASK  BIT(4)
#define SB_EARC_SIG_DET_BYPASS(x)  UPDATE(x, 4, 4)
#define SB_AFC_TOL_MASK  GENMASK(3, 0)
#define SB_AFC_TOL(x)  UPDATE(x, 3, 0)
#define SB_REG011C            0x0470
#define SB_REG011D            0x0474
#define SB_REG011E            0x0478
#define SB_REG011F            0x047C
#define SB_PWM_AFC_CTRL_MASK  GENMASK(7, 2)
#define SB_PWM_AFC_CTRL(x)  UPDATE(x, 7, 2)
#define SB_RCAL_RSTN_MASK  BIT(1)
#define SB_RCAL_RSTN(x)  UPDATE(x, 1, 1)
#define SB_REG0120       0x0480
#define SB_EARC_EN_MASK  BIT(1)
#define SB_EARC_EN(x)  UPDATE(x, 1, 1)
#define SB_EARC_AFC_EN_MASK  BIT(2)
#define SB_EARC_AFC_EN(x)  UPDATE(x, 2, 2)
#define SB_REG0121          0x0484
#define SB_REG0122          0x0488
#define SB_REG0123          0x048C
#define OVRD_SB_READY_MASK  BIT(5)
#define OVRD_SB_READY(x)  UPDATE(x, 5, 5)
#define SB_READY_MASK  BIT(4)
#define SB_READY(x)  UPDATE(x, 4, 4)
#define SB_REG0124                0x0490
#define SB_REG0125                0x0494
#define SB_REG0126                0x0498
#define SB_REG0127                0x049C
#define SB_REG0128                0x04A0
#define SB_REG0129                0x04AD
#define LNTOP_REG0200             0x0800
#define PROTOCOL_SEL              BIT(2)
#define HDMI_MODE                 BIT(2)
#define HDMI_TMDS_FRL_SEL         BIT(1)
#define LNTOP_REG0201             0x0804
#define LNTOP_REG0202             0x0808
#define LNTOP_REG0203             0x080C
#define LNTOP_REG0204             0x0810
#define LNTOP_REG0205             0x0814
#define LNTOP_REG0206             0x0818
#define DATA_BUS_WIDTH            (0x3 << 1)
#define WIDTH_40BIT               (0x3 << 1)
#define WIDTH_36BIT               (0x2 << 1)
#define DATA_BUS_SEL              BIT(0)
#define DATA_BUS_36_40            BIT(0)
#define LNTOP_REG0207             0x081C
#define LANE_EN                   0xf
#define ALL_LANE_EN               0xf
#define LNTOP_REG0208             0x0820
#define LNTOP_REG0209             0x0824
#define LNTOP_REG020A             0x0828
#define LNTOP_REG020B             0x082C
#define LNTOP_REG020C             0x0830
#define LNTOP_REG020D             0x0834
#define LNTOP_REG020E             0x0838
#define LNTOP_REG020F             0x083C
#define LNTOP_REG0210             0x0840
#define LNTOP_REG0211             0x0844
#define LNTOP_REG0212             0x0848
#define LNTOP_REG0213             0x084C
#define LNTOP_REG0214             0x0850
#define LNTOP_REG0215             0x0854
#define LNTOP_REG0216             0x0858
#define LNTOP_REG0217             0x085C
#define LNTOP_REG0218             0x0860
#define LNTOP_REG0219             0x0864
#define LNTOP_REG021A             0x0868
#define LNTOP_REG021B             0x086C
#define LNTOP_REG021C             0x0870
#define LNTOP_REG021D             0x0874
#define LNTOP_REG021E             0x0878
#define LNTOP_REG021F             0x087C
#define LNTOP_REG0220             0x0880
#define LNTOP_REG0221             0x0884
#define LNTOP_REG0222             0x0888
#define LNTOP_REG0223             0x088C
#define LNTOP_REG0224             0x0890
#define LNTOP_REG0225             0x0894
#define LNTOP_REG0226             0x0898
#define LNTOP_REG0227             0x089C
#define LNTOP_REG0228             0x08A0
#define LNTOP_REG0229             0x08A4
#define LANE_REG0300              0x0C00
#define LANE_REG0301              0x0C04
#define LANE_REG0302              0x0C08
#define LANE_REG0303              0x0C0C
#define LANE_REG0304              0x0C10
#define LANE_REG0305              0x0C14
#define LANE_REG0306              0x0C18
#define LANE_REG0307              0x0C1C
#define LANE_REG0308              0x0C20
#define LANE_REG0309              0x0C24
#define LANE_REG030A              0x0C28
#define LANE_REG030B              0x0C2C
#define LANE_REG030C              0x0C30
#define LANE_REG030D              0x0C34
#define LANE_REG030E              0x0C38
#define LANE_REG030F              0x0C3C
#define LANE_REG0310              0x0C40
#define LANE_REG0311              0x0C44
#define LANE_REG0312              0x0C48
#define LN0_TX_SER_RATE_SEL_RBR   BIT(5)
#define LN0_TX_SER_RATE_SEL_HBR   BIT(4)
#define LN0_TX_SER_RATE_SEL_HBR2  BIT(3)
#define LN0_TX_SER_RATE_SEL_HBR3  BIT(2)
#define LANE_REG0313              0x0C4C
#define LANE_REG0314              0x0C50
#define LANE_REG0315              0x0C54
#define LANE_REG0316              0x0C58
#define LANE_REG0317              0x0C5C
#define LANE_REG0318              0x0C60
#define LANE_REG0319              0x0C64
#define LANE_REG031A              0x0C68
#define LANE_REG031B              0x0C6C
#define LANE_REG031C              0x0C70
#define LANE_REG031D              0x0C74
#define LANE_REG031E              0x0C78
#define LANE_REG031F              0x0C7C
#define LANE_REG0320              0x0C80
#define LANE_REG0321              0x0C84
#define LANE_REG0322              0x0C88
#define LANE_REG0323              0x0C8C
#define LANE_REG0324              0x0C90
#define LANE_REG0325              0x0C94
#define LANE_REG0326              0x0C98
#define LANE_REG0327              0x0C9C
#define LANE_REG0328              0x0CA0
#define LANE_REG0329              0x0CA4
#define LANE_REG032A              0x0CA8
#define LANE_REG032B              0x0CAC
#define LANE_REG032C              0x0CB0
#define LANE_REG032D              0x0CB4
#define LANE_REG0400              0x1000
#define LANE_REG0401              0x1004
#define LANE_REG0402              0x1008
#define LANE_REG0403              0x100C
#define LANE_REG0404              0x1010
#define LANE_REG0405              0x1014
#define LANE_REG0406              0x1018
#define LANE_REG0407              0x101C
#define LANE_REG0408              0x1020
#define LANE_REG0409              0x1024
#define LANE_REG040A              0x1028
#define LANE_REG040B              0x102C
#define LANE_REG040C              0x1030
#define LANE_REG040D              0x1034
#define LANE_REG040E              0x1038
#define LANE_REG040F              0x103C
#define LANE_REG0410              0x1040
#define LANE_REG0411              0x1044
#define LANE_REG0412              0x1048
#define LN1_TX_SER_RATE_SEL_RBR   BIT(5)
#define LN1_TX_SER_RATE_SEL_HBR   BIT(4)
#define LN1_TX_SER_RATE_SEL_HBR2  BIT(3)
#define LN1_TX_SER_RATE_SEL_HBR3  BIT(2)
#define LANE_REG0413              0x104C
#define LANE_REG0414              0x1050
#define LANE_REG0415              0x1054
#define LANE_REG0416              0x1058
#define LANE_REG0417              0x105C
#define LANE_REG0418              0x1060
#define LANE_REG0419              0x1064
#define LANE_REG041A              0x1068
#define LANE_REG041B              0x106C
#define LANE_REG041C              0x1070
#define LANE_REG041D              0x1074
#define LANE_REG041E              0x1078
#define LANE_REG041F              0x107C
#define LANE_REG0420              0x1080
#define LANE_REG0421              0x1084
#define LANE_REG0422              0x1088
#define LANE_REG0423              0x108C
#define LANE_REG0424              0x1090
#define LANE_REG0425              0x1094
#define LANE_REG0426              0x1098
#define LANE_REG0427              0x109C
#define LANE_REG0428              0x10A0
#define LANE_REG0429              0x10A4
#define LANE_REG042A              0x10A8
#define LANE_REG042B              0x10AC
#define LANE_REG042C              0x10B0
#define LANE_REG042D              0x10B4
#define LANE_REG0500              0x1400
#define LANE_REG0501              0x1404
#define LANE_REG0502              0x1408
#define LANE_REG0503              0x140C
#define LANE_REG0504              0x1410
#define LANE_REG0505              0x1414
#define LANE_REG0506              0x1418
#define LANE_REG0507              0x141C
#define LANE_REG0508              0x1420
#define LANE_REG0509              0x1424
#define LANE_REG050A              0x1428
#define LANE_REG050B              0x142C
#define LANE_REG050C              0x1430
#define LANE_REG050D              0x1434
#define LANE_REG050E              0x1438
#define LANE_REG050F              0x143C
#define LANE_REG0510              0x1440
#define LANE_REG0511              0x1444
#define LANE_REG0512              0x1448
#define LN2_TX_SER_RATE_SEL_RBR   BIT(5)
#define LN2_TX_SER_RATE_SEL_HBR   BIT(4)
#define LN2_TX_SER_RATE_SEL_HBR2  BIT(3)
#define LN2_TX_SER_RATE_SEL_HBR3  BIT(2)
#define LANE_REG0513              0x144C
#define LANE_REG0514              0x1450
#define LANE_REG0515              0x1454
#define LANE_REG0516              0x1458
#define LANE_REG0517              0x145C
#define LANE_REG0518              0x1460
#define LANE_REG0519              0x1464
#define LANE_REG051A              0x1468
#define LANE_REG051B              0x146C
#define LANE_REG051C              0x1470
#define LANE_REG051D              0x1474
#define LANE_REG051E              0x1478
#define LANE_REG051F              0x147C
#define LANE_REG0520              0x1480
#define LANE_REG0521              0x1484
#define LANE_REG0522              0x1488
#define LANE_REG0523              0x148C
#define LANE_REG0524              0x1490
#define LANE_REG0525              0x1494
#define LANE_REG0526              0x1498
#define LANE_REG0527              0x149C
#define LANE_REG0528              0x14A0
#define LANE_REG0529              0x14AD
#define LANE_REG052A              0x14A8
#define LANE_REG052B              0x14AC
#define LANE_REG052C              0x14B0
#define LANE_REG052D              0x14B4
#define LANE_REG0600              0x1800
#define LANE_REG0601              0x1804
#define LANE_REG0602              0x1808
#define LANE_REG0603              0x180C
#define LANE_REG0604              0x1810
#define LANE_REG0605              0x1814
#define LANE_REG0606              0x1818
#define LANE_REG0607              0x181C
#define LANE_REG0608              0x1820
#define LANE_REG0609              0x1824
#define LANE_REG060A              0x1828
#define LANE_REG060B              0x182C
#define LANE_REG060C              0x1830
#define LANE_REG060D              0x1834
#define LANE_REG060E              0x1838
#define LANE_REG060F              0x183C
#define LANE_REG0610              0x1840
#define LANE_REG0611              0x1844
#define LANE_REG0612              0x1848
#define LN3_TX_SER_RATE_SEL_RBR   BIT(5)
#define LN3_TX_SER_RATE_SEL_HBR   BIT(4)
#define LN3_TX_SER_RATE_SEL_HBR2  BIT(3)
#define LN3_TX_SER_RATE_SEL_HBR3  BIT(2)
#define LANE_REG0613              0x184C
#define LANE_REG0614              0x1850
#define LANE_REG0615              0x1854
#define LANE_REG0616              0x1858
#define LANE_REG0617              0x185C
#define LANE_REG0618              0x1860
#define LANE_REG0619              0x1864
#define LANE_REG061A              0x1868
#define LANE_REG061B              0x186C
#define LANE_REG061C              0x1870
#define LANE_REG061D              0x1874
#define LANE_REG061E              0x1878
#define LANE_REG061F              0x187C
#define LANE_REG0620              0x1880
#define LANE_REG0621              0x1884
#define LANE_REG0622              0x1888
#define LANE_REG0623              0x188C
#define LANE_REG0624              0x1890
#define LANE_REG0625              0x1894
#define LANE_REG0626              0x1898
#define LANE_REG0627              0x189C
#define LANE_REG0628              0x18A0
#define LANE_REG0629              0x18A4
#define LANE_REG062A              0x18A8
#define LANE_REG062B              0x18AC
#define LANE_REG062C              0x18B0
#define LANE_REG062D              0x18B4

#define HDMI20_MAX_RATE   600000000
#define DATA_RATE_MASK    0xFFFFFFF
#define COLOR_DEPTH_MASK  BIT(31)
#define HDMI_MODE_MASK    BIT(30)
#define HDMI_EARC_MASK    BIT(29)

#define FRL_8G_4LANES  3200000000ULL
#define FRL_6G_3LANES  1800000000
#define FRL_3G_3LANES  900000000

struct RoPllConfig {
  UINT32    Bit_Rate;
  UINT8     Pms_Mdiv;
  UINT8     Pms_Mdiv_Afc;
  UINT8     Pms_Pdiv;
  UINT8     Pms_Refdiv;
  UINT8     Pms_Sdiv;
  UINT8     Pms_Iqdiv_Rstn;
  UINT8     Ref_Clk_Sel;
  UINT8     Sdm_En;
  UINT8     Sdm_Rstn;
  UINT8     Sdc_Frac_En;
  UINT8     Sdc_Rstn;
  UINT8     Sdm_Clk_Div;
  UINT8     Sdm_Deno;
  UINT8     Sdm_Num_Sign;
  UINT8     Sdm_Num;
  UINT8     Sdc_N;
  UINT8     Sdc_Num;
  UINT8     Sdc_Deno;
  UINT8     Sdc_Ndiv_Rstn;
  UINT8     Ssc_En;
  UINT8     Ssc_Fm_Dev;
  UINT8     Ssc_Fm_Freq;
  UINT8     Ssc_Clk_Div_Sel;
  UINT8     Ana_Cpp_Ctrl;
  UINT8     Ana_Lpf_C_Sel;
  UINT8     Cd_Tx_Ser_Rate_Sel;
};

STATIC CONST struct RoPllConfig  ROPLL_TMDS_CONFIG[] = {
  { 5940000, 124,  124,  1, 1, 0,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 3712500, 155,  155,  1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 2970000, 124,  124,  1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1620000, 135,  135,  1, 1, 3,   1, 1, 0, 1, 1, 1, 1, 4,   0, 3,    5, 5,   0x10,
    1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1856250, 155,  155,  1, 1, 3,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1540000, 193,  193,  1, 1, 5,   1, 1, 1, 1, 1, 1, 1, 193, 1, 32,   2, 1,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1485000, 0x7b, 0x7b, 1, 1, 3,   1, 1, 1, 1, 1, 1, 1, 4,   0, 3,    5, 5,   0x10,
    1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1462500, 122,  122,  1, 1, 3,   1, 1, 1, 1, 1, 1, 1, 244, 1, 16,   2, 1,   1,
    1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1190000, 149,  149,  1, 1, 5,   1, 1, 1, 1, 1, 1, 1, 149, 1, 16,   2, 1,   1,
    1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1065000, 89,   89,   1, 1, 3,   1, 1, 1, 1, 1, 1, 1, 89,  1, 16,   1, 0,   1,
    1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 1080000, 135,  135,  1, 1, 5,   1, 1, 0, 1, 0, 1, 1, 0x9, 0, 0x05, 0, 0x14,
    0x18, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 855000,  214,  214,  1, 1, 11,  1, 1, 1, 1, 1, 1, 1, 214, 1, 16,   2, 1,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 835000,  105,  105,  1, 1, 5,   1, 1, 1, 1, 1, 1, 1, 42,  1, 16,   1, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 928125,  155,  155,  1, 1, 7,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 742500,  124,  124,  1, 1, 7,   1, 1, 1, 1, 1, 1, 1, 62,  1, 16,   5, 0,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 650000,  162,  162,  1, 1, 11,  1, 1, 1, 1, 1, 1, 1, 54,  0, 16,   4, 1,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 337500,  0x70, 0x70, 1, 1, 0xf, 1, 1, 1, 1, 1, 1, 1, 0x2, 0, 0x01, 5, 1,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 400000,  100,  100,  1, 1, 11,  1, 1, 0, 1, 0, 1, 1, 0x9, 0, 0x05, 0, 0x14,
    0x18, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 270000,  0x5a, 0x5a, 1, 1, 0xf, 1, 1, 0, 1, 0, 1, 1, 0x9, 0, 0x05, 0, 0x14,
    0x18, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { 251750,  84,   84,   1, 1, 0xf, 1, 1, 1, 1, 1, 1, 1, 168, 1, 16,   4, 1,
    1, 1, 0, 0x20, 0x0c, 1, 0x0e, 0, 0, },
  { ~0,      0,    0,    0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,   0, 0,    0, 0,   0,    0,0, 0, 0,
    0, 0, 0, 0, },
};

INLINE
VOID
PhyWrite (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  UINTN                            Reg,
  UINTN                            Val
  )
{
  UINT32  Shift;

  if (!Hdptx->Id) {
    Shift = HDMI0TX_PHY_BASE;
  } else {
    Shift = HDMI1TX_PHY_BASE;
  }

  MmioWrite32 (Shift + Reg, Val);
}

INLINE
UINTN
PhyRead (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  UINTN                            Reg
  )
{
  UINT32  Shift;

  if (!Hdptx->Id) {
    Shift = HDMI0TX_PHY_BASE;
  } else {
    Shift = HDMI1TX_PHY_BASE;
  }

  return MmioRead32 (Shift + Reg);
}

STATIC
VOID
PhyUpdateBits (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  UINTN                            Reg,
  UINTN                            Mask,
  UINTN                            Val
  )
{
  UINTN  Orig, Tmp;

  Orig = PhyRead (Hdptx, Reg);
  Tmp  = Orig & ~Mask;
  Tmp |= Val & Mask;
  PhyWrite (Hdptx, Reg, Tmp);
}

STATIC
VOID
GrfWrite (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  UINTN                            Reg,
  UINTN                            Mask,
  UINTN                            Val
  )
{
  UINT32  TempVal = 0;
  UINT32  Shift;

  if (!Hdptx->Id) {
    Shift = HDPTXPHY0_GRF_BASE;
  } else {
    Shift = HDPTXPHY1_GRF_BASE;
  }

  TempVal = (Mask << 16) | (Val & Mask);
  MmioWrite32 (Shift + Reg, TempVal);
}

STATIC
UINT32
GrfRead (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  UINTN                            Reg
  )
{
  UINT32  Shift;

  if (!Hdptx->Id) {
    Shift = HDPTXPHY0_GRF_BASE;
  } else {
    Shift = HDPTXPHY1_GRF_BASE;
  }

  return MmioRead32 (Shift + Reg);
}

STATIC
VOID
CruWrite (
  UINTN  Reg,
  UINTN  Mask,
  UINTN  Val
  )
{
  UINT32  TempVal = 0;

  TempVal = (Mask << 16) | (Val & Mask);
  MmioWrite32 (PMU1CRU_BASE + Reg, TempVal);
}

STATIC
VOID
HdptxPrePowerUp (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx
  )
{
  UINT32  Val = 0;

  /* assert lane/cmn/init reset */
  if (!Hdptx->Id) {
    CruWrite (PMU1CRU_SOFTRST_CON03, 0x3800, 0x3800);
  } else {
    CruWrite (PMU1CRU_SOFTRST_CON03, BIT (15), BIT (15));
    CruWrite (PMU1CRU_SOFTRST_CON04, 0x3, 0x3);
  }

  Val = HDPTX_I_PLL_EN | HDPTX_I_BIAS_EN | HDPTX_I_BGR_EN;
  GrfWrite (Hdptx, GRF_HDPTX_CON0, Val, 0);
}

STATIC
EFI_STATUS
HdptxPostEnablePll (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx
  )
{
  UINT32  Val = 0;
  UINT32  i;

  Val = HDPTX_I_BIAS_EN | HDPTX_I_BGR_EN;
  GrfWrite (Hdptx, GRF_HDPTX_CON0, Val, Val);
  NanoSecondDelay (10000);
  /* deassert init reset */
  if (!Hdptx->Id) {
    CruWrite (PMU1CRU_SOFTRST_CON03, BIT (11), 0);
  } else {
    CruWrite (PMU1CRU_SOFTRST_CON03, BIT (15), 0);
  }

  NanoSecondDelay (10000);
  Val = HDPTX_I_PLL_EN;
  GrfWrite (Hdptx, GRF_HDPTX_CON0, Val, Val);
  NanoSecondDelay (10000);
  /* deassert cmn reset */
  if (!Hdptx->Id) {
    CruWrite (PMU1CRU_SOFTRST_CON03, BIT (12), 0);
  } else {
    CruWrite (PMU1CRU_SOFTRST_CON04, BIT (0), 0);
  }

  Val = 0;
  for (i = 0; i < 50; i++) {
    Val = GrfRead (Hdptx, GRF_HDPTX_STATUS);

    if (Val & HDPTX_O_PHY_CLK_RDY) {
      break;
    }

    NanoSecondDelay (20000);
  }

  if (i == 50) {
    DEBUG ((DEBUG_INIT, "%a hdptx phy pll can't lock!\n", __func__));
    return EFI_TIMEOUT;
  }

  DEBUG ((DEBUG_INIT, "%a hdptx phy pll locked!\n", __func__));

  return EFI_SUCCESS;
}

STATIC
VOID
RationalBestApproximation (
  UINT32  given_numerator,
  UINT32  given_denominator,
  UINT32  max_numerator,
  UINT32  max_denominator,
  UINT32  *best_numerator,
  UINT32  *best_denominator
  )
{
  /* n/d is the starting rational, which is continually
   * decreased each iteration using the Euclidean algorithm.
   *
   * dp is the value of d from the prior iteration.
   *
   * n2/d2, n1/d1, and n0/d0 are our successively more accurate
   * approximations of the rational.  They are, respectively,
   * the current, previous, and two prior iterations of it.
   *
   * a is current term of the continued fraction.
   */
  UINT32  n, d, n0, d0, n1, d1, n2, d2;

  n  = given_numerator;
  d  = given_denominator;
  n0 = d1 = 0;
  n1 = d0 = 1;

  for ( ; ;) {
    UINT32  dp, a;

    if (d == 0) {
      break;
    }

    /* Find next term in continued fraction, 'a', via
     * Euclidean algorithm.
     */
    dp = d;
    a  = n / d;
    d  = n % d;
    n  = dp;

    /* Calculate the current rational approximation (aka
     * convergent), n2/d2, using the term just found and
     * the two prior approximations.
     */
    n2 = n0 + a * n1;
    d2 = d0 + a * d1;

    /* If the current convergent exceeds the maxes, then
     * return either the previous convergent or the
     * largest semi-convergent, the final term of which is
     * found below as 't'.
     */
    if ((n2 > max_numerator) || (d2 > max_denominator)) {
      UINT32  t = min (
                    (max_numerator - n0) / n1,
                    (max_denominator - d0) / d1
                    );

      /* This tests if the semi-convergent is closer
       * than the previous convergent.
       */
      if ((2u * t > a) || ((2u * t == a) && (d0 * dp > d1 * d))) {
        n1 = n0 + t * n1;
        d1 = d0 + t * d1;
      }

      break;
    }

    n0 = n1;
    n1 = n2;
    d0 = d1;
    d1 = d2;
  }

  *best_numerator   = n1;
  *best_denominator = d1;
}

STATIC
BOOLEAN
HdptxPhyClkPllCalc (
  IN  UINT32              DataRate,
  OUT struct RoPllConfig  *Cfg
  )
{
  UINT32  Fref = 24000;
  UINT32  Sdc;
  UINT32  Fout = DataRate / 2;
  UINT32  Fvco;
  UINT32  Mdiv, Sdiv, N = 8;
  UINT32  K = 0, Lc, K_Sub, Lc_Sub;

  for (Sdiv = 1; Sdiv <= 16; Sdiv++) {
    if (Sdiv % 2 && (Sdiv != 1)) {
      continue;
    }

    Fvco = Fout * Sdiv;

    if ((Fvco < 2000000) || (Fvco > 4000000)) {
      continue;
    }

    Mdiv = DIV_ROUND_UP (Fvco, Fref);
    if ((Mdiv < 20) || (Mdiv > 255)) {
      continue;
    }

    if (Fref * Mdiv - Fvco) {
      for (Sdc = 264000; Sdc <= 750000; Sdc += Fref) {
        if (Sdc * N > Fref * Mdiv) {
          break;
        }
      }

      if (Sdc > 750000) {
        continue;
      }

      RationalBestApproximation (
        Fref * Mdiv - Fvco,
        Sdc / 16,
        GENMASK (6, 0),
        GENMASK (7, 0),
        &K,
        &Lc
        );

      RationalBestApproximation (
        Sdc * N - Fref * Mdiv,
        Sdc,
        GENMASK (6, 0),
        GENMASK (7, 0),
        &K_Sub,
        &Lc_Sub
        );
    }

    break;
  }

  if (Sdiv > 16) {
    return FALSE;
  }

  if (Cfg) {
    Cfg->Pms_Mdiv     = Mdiv;
    Cfg->Pms_Mdiv_Afc = Mdiv;
    Cfg->Pms_Pdiv     = 1;
    Cfg->Pms_Refdiv   = 1;
    Cfg->Pms_Sdiv     = Sdiv - 1;

    Cfg->Sdm_En = K > 0 ? 1 : 0;
    if (Cfg->Sdm_En) {
      Cfg->Sdm_Deno     = Lc;
      Cfg->Sdm_Num_Sign = 1;
      Cfg->Sdm_Num      = K;
      Cfg->Sdc_N        = N - 3;
      Cfg->Sdc_Num      = K_Sub;
      Cfg->Sdc_Deno     = Lc_Sub;
    }
  }

  return TRUE;
}

EFI_STATUS
HdptxRopllCmnConfig (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  IN  UINT32                       BitRate
  )
{
  CONST struct RoPllConfig  *Cfg = ROPLL_TMDS_CONFIG;
  struct RoPllConfig        Rc   = { 0 };

  for ( ; Cfg->Bit_Rate != ~0; Cfg++) {
    if (Cfg->Bit_Rate == BitRate) {
      break;
    }
  }

  if (Cfg->Bit_Rate == ~0) {
    if (HdptxPhyClkPllCalc (BitRate, &Rc)) {
      Cfg = &Rc;
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Couldn't find RoPllConfig!\n", __func__));
      return EFI_INVALID_PARAMETER;
    }
  }

  DEBUG ((DEBUG_INIT, "%a HdptxRopllCmnConfig start\n", __func__));
  HdptxPrePowerUp (Hdptx);
  DEBUG ((DEBUG_INIT, "%a HdptxRopllCmnConfig %d\n", __func__, Cfg->Bit_Rate));
  GrfWrite (Hdptx, GRF_HDPTX_CON0, LC_REF_CLK_SEL, 0);

  PhyWrite (Hdptx, CMN_REG0008, 0x00);
  PhyWrite (Hdptx, CMN_REG0009, 0x0c);
  PhyWrite (Hdptx, CMN_REG000A, 0x83);
  PhyWrite (Hdptx, CMN_REG000B, 0x06);
  PhyWrite (Hdptx, CMN_REG000C, 0x20);
  PhyWrite (Hdptx, CMN_REG000D, 0xb8);
  PhyWrite (Hdptx, CMN_REG000E, 0x0f);
  PhyWrite (Hdptx, CMN_REG000F, 0x0f);
  PhyWrite (Hdptx, CMN_REG0010, 0x04);
  PhyWrite (Hdptx, CMN_REG0011, 0x01);
  PhyWrite (Hdptx, CMN_REG0012, 0x26);
  PhyWrite (Hdptx, CMN_REG0013, 0x22);
  PhyWrite (Hdptx, CMN_REG0014, 0x24);
  PhyWrite (Hdptx, CMN_REG0015, 0x77);
  PhyWrite (Hdptx, CMN_REG0016, 0x08);
  PhyWrite (Hdptx, CMN_REG0017, 0x20);
  PhyWrite (Hdptx, CMN_REG0018, 0x04);
  PhyWrite (Hdptx, CMN_REG0019, 0x48);
  PhyWrite (Hdptx, CMN_REG001A, 0x01);
  PhyWrite (Hdptx, CMN_REG001B, 0x00);
  PhyWrite (Hdptx, CMN_REG001C, 0x01);
  PhyWrite (Hdptx, CMN_REG001D, 0x64);
  PhyWrite (Hdptx, CMN_REG001E, 0x14);
  PhyWrite (Hdptx, CMN_REG001F, 0x00);
  PhyWrite (Hdptx, CMN_REG0020, 0x00);
  PhyWrite (Hdptx, CMN_REG0021, 0x00);
  PhyWrite (Hdptx, CMN_REG0022, 0x11);
  PhyWrite (Hdptx, CMN_REG0023, 0x00);
  PhyWrite (Hdptx, CMN_REG0024, 0x00);
  PhyWrite (Hdptx, CMN_REG0025, 0x53);
  PhyWrite (Hdptx, CMN_REG0026, 0x00);
  PhyWrite (Hdptx, CMN_REG0027, 0x00);
  PhyWrite (Hdptx, CMN_REG0028, 0x01);
  PhyWrite (Hdptx, CMN_REG0029, 0x01);
  PhyWrite (Hdptx, CMN_REG002A, 0x00);
  PhyWrite (Hdptx, CMN_REG002B, 0x00);
  PhyWrite (Hdptx, CMN_REG002C, 0x00);
  PhyWrite (Hdptx, CMN_REG002D, 0x00);
  PhyWrite (Hdptx, CMN_REG002E, 0x04);
  PhyWrite (Hdptx, CMN_REG002F, 0x00);
  PhyWrite (Hdptx, CMN_REG0030, 0x20);
  PhyWrite (Hdptx, CMN_REG0031, 0x30);
  PhyWrite (Hdptx, CMN_REG0032, 0x0b);
  PhyWrite (Hdptx, CMN_REG0033, 0x23);
  PhyWrite (Hdptx, CMN_REG0034, 0x00);
  PhyWrite (Hdptx, CMN_REG0035, 0x00);
  PhyWrite (Hdptx, CMN_REG0038, 0x00);
  PhyWrite (Hdptx, CMN_REG0039, 0x00);
  PhyWrite (Hdptx, CMN_REG003A, 0x00);
  PhyWrite (Hdptx, CMN_REG003B, 0x00);
  PhyWrite (Hdptx, CMN_REG003C, 0x80);
  PhyWrite (Hdptx, CMN_REG003D, 0x40);
  PhyWrite (Hdptx, CMN_REG003E, 0x0c);
  PhyWrite (Hdptx, CMN_REG003F, 0x83);
  PhyWrite (Hdptx, CMN_REG0040, 0x06);
  PhyWrite (Hdptx, CMN_REG0041, 0x20);
  PhyWrite (Hdptx, CMN_REG0042, 0x78);
  PhyWrite (Hdptx, CMN_REG0043, 0x00);
  PhyWrite (Hdptx, CMN_REG0044, 0x46);
  PhyWrite (Hdptx, CMN_REG0045, 0x24);
  PhyWrite (Hdptx, CMN_REG0046, 0xdd);
  PhyWrite (Hdptx, CMN_REG0047, 0x00);
  PhyWrite (Hdptx, CMN_REG0048, 0x11);
  PhyWrite (Hdptx, CMN_REG0049, 0xfa);
  PhyWrite (Hdptx, CMN_REG004A, 0x08);
  PhyWrite (Hdptx, CMN_REG004B, 0x00);
  PhyWrite (Hdptx, CMN_REG004C, 0x01);
  PhyWrite (Hdptx, CMN_REG004D, 0x64);
  PhyWrite (Hdptx, CMN_REG004E, 0x34);
  PhyWrite (Hdptx, CMN_REG004F, 0x00);
  PhyWrite (Hdptx, CMN_REG0050, 0x00);
  DEBUG ((DEBUG_INIT, "%a HdptxRopllCmnConfig 2\n", __func__));
  PhyWrite (Hdptx, CMN_REG0051, Cfg->Pms_Mdiv);
  PhyWrite (Hdptx, CMN_REG0055, Cfg->Pms_Mdiv_Afc);

  PhyWrite (Hdptx, CMN_REG0059, (Cfg->Pms_Pdiv << 4) | Cfg->Pms_Refdiv);

  PhyWrite (Hdptx, CMN_REG005A, (Cfg->Pms_Sdiv << 4));

  PhyWrite (Hdptx, CMN_REG005C, 0x25);
  PhyWrite (Hdptx, CMN_REG005D, 0x0c);
  PhyWrite (Hdptx, CMN_REG005E, 0x4f);
  PhyUpdateBits (
    Hdptx,
    CMN_REG005E,
    ROPLL_SDM_EN_MASK,
    ROPLL_SDM_EN (Cfg->Sdm_En)
    );
  if (!Cfg->Sdm_En) {
    PhyUpdateBits (Hdptx, CMN_REG005E, 0xf, 0);
  }

  PhyWrite (Hdptx, CMN_REG005F, 0x01);

  PhyUpdateBits (
    Hdptx,
    CMN_REG0064,
    ROPLL_SDM_NUM_SIGN_RBR_MASK,
    ROPLL_SDM_NUM_SIGN_RBR (Cfg->Sdm_Num_Sign)
    );
  PhyWrite (Hdptx, CMN_REG0065, Cfg->Sdm_Num);
  PhyWrite (Hdptx, CMN_REG0060, Cfg->Sdm_Deno);

  PhyUpdateBits (
    Hdptx,
    CMN_REG0069,
    ROPLL_SDC_N_RBR_MASK,
    ROPLL_SDC_N_RBR (Cfg->Sdc_N)
    );

  PhyWrite (Hdptx, CMN_REG006C, Cfg->Sdc_Num);
  PhyWrite (Hdptx, CMN_REG0070, Cfg->Sdc_Deno);

  PhyWrite (Hdptx, CMN_REG006B, 0x04);

  PhyWrite (Hdptx, CMN_REG0073, 0x30);
  PhyWrite (Hdptx, CMN_REG0074, 0x04);
  PhyWrite (Hdptx, CMN_REG0075, 0x20);
  PhyWrite (Hdptx, CMN_REG0076, 0x30);
  PhyWrite (Hdptx, CMN_REG0077, 0x08);
  PhyWrite (Hdptx, CMN_REG0078, 0x0c);
  PhyWrite (Hdptx, CMN_REG0079, 0x00);
  PhyWrite (Hdptx, CMN_REG007B, 0x00);
  PhyWrite (Hdptx, CMN_REG007C, 0x00);
  PhyWrite (Hdptx, CMN_REG007D, 0x00);
  PhyWrite (Hdptx, CMN_REG007E, 0x00);
  PhyWrite (Hdptx, CMN_REG007F, 0x00);
  PhyWrite (Hdptx, CMN_REG0080, 0x00);
  PhyWrite (Hdptx, CMN_REG0081, 0x01);
  PhyWrite (Hdptx, CMN_REG0082, 0x04);
  PhyWrite (Hdptx, CMN_REG0083, 0x24);
  PhyWrite (Hdptx, CMN_REG0084, 0x20);
  PhyWrite (Hdptx, CMN_REG0085, 0x03);

  PhyUpdateBits (
    Hdptx,
    CMN_REG0086,
    PLL_PCG_POSTDIV_SEL_MASK,
    PLL_PCG_POSTDIV_SEL (Cfg->Pms_Sdiv)
    );

  PhyUpdateBits (
    Hdptx,
    CMN_REG0086,
    PLL_PCG_CLK_SEL_MASK,
    PLL_PCG_CLK_SEL (8)
    );

  PhyUpdateBits (Hdptx, CMN_REG0086, PLL_PCG_CLK_EN, PLL_PCG_CLK_EN);

  PhyWrite (Hdptx, CMN_REG0087, 0x04);
  PhyWrite (Hdptx, CMN_REG0089, 0x00);
  PhyWrite (Hdptx, CMN_REG008A, 0x55);
  PhyWrite (Hdptx, CMN_REG008B, 0x25);
  PhyWrite (Hdptx, CMN_REG008C, 0x2c);
  PhyWrite (Hdptx, CMN_REG008D, 0x22);
  PhyWrite (Hdptx, CMN_REG008E, 0x14);
  PhyWrite (Hdptx, CMN_REG008F, 0x20);
  PhyWrite (Hdptx, CMN_REG0090, 0x00);
  PhyWrite (Hdptx, CMN_REG0091, 0x00);
  PhyWrite (Hdptx, CMN_REG0092, 0x00);
  PhyWrite (Hdptx, CMN_REG0093, 0x00);
  PhyWrite (Hdptx, CMN_REG0095, 0x00);
  PhyWrite (Hdptx, CMN_REG0097, 0x02);
  PhyWrite (Hdptx, CMN_REG0099, 0x04);
  PhyWrite (Hdptx, CMN_REG009A, 0x11);
  PhyWrite (Hdptx, CMN_REG009B, 0x00);
  DEBUG ((DEBUG_INIT, "%a HdptxRopllCmnConfig end\n", __func__));
  return HdptxPostEnablePll (Hdptx);
}

STATIC
EFI_STATUS
HdptxPostEnableLane (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx
  )
{
  UINT32  Val = 0;
  UINT32  i;

  /* deassert lane reset */
  if (!Hdptx->Id) {
    CruWrite (PMU1CRU_SOFTRST_CON03, BIT (13), 0);
  } else {
    CruWrite (PMU1CRU_SOFTRST_CON04, BIT (1), 0);
  }

  Val = HDPTX_I_BIAS_EN | HDPTX_I_BGR_EN;
  GrfWrite (Hdptx, GRF_HDPTX_CON0, Val, Val);

  /* 4 lanes frl mode */
  PhyWrite (Hdptx, LNTOP_REG0207, 0x0f);

  Val = 0;
  for (i = 0; i < 50; i++) {
    Val = GrfRead (Hdptx, GRF_HDPTX_STATUS);

    if (Val & HDPTX_O_PHY_RDY && Val & HDPTX_O_PLL_LOCK_DONE) {
      break;
    }

    NanoSecondDelay (100000);
  }

  if (i == 50) {
    DEBUG ((DEBUG_INIT, "%a hdptx phy lane can't ready!\n", __func__));
    return EFI_TIMEOUT;
  }

  DEBUG ((DEBUG_INIT, "%a hdptx phy lane locked!\n", __func__));

  return EFI_SUCCESS;
}

EFI_STATUS
HdptxRopllTmdsModeConfig (
  OUT struct RockchipHdptxPhyHdmi  *Hdptx,
  IN  UINT32                       BitRate
  )
{
  PhyWrite (Hdptx, SB_REG0114, 0x00);
  PhyWrite (Hdptx, SB_REG0115, 0x00);
  PhyWrite (Hdptx, SB_REG0116, 0x00);
  PhyWrite (Hdptx, SB_REG0117, 0x00);
  PhyWrite (Hdptx, LNTOP_REG0200, 0x06);

  if (BitRate >= 3400000) {
    /* For 1/40 bitrate clk */
    PhyWrite (Hdptx, LNTOP_REG0201, 0x00);
    PhyWrite (Hdptx, LNTOP_REG0202, 0x00);
    PhyWrite (Hdptx, LNTOP_REG0203, 0x0f);
    PhyWrite (Hdptx, LNTOP_REG0204, 0xff);
    PhyWrite (Hdptx, LNTOP_REG0205, 0xff);
  } else {
    /* For 1/10 bitrate clk */
    PhyWrite (Hdptx, LNTOP_REG0201, 0x07);
    PhyWrite (Hdptx, LNTOP_REG0202, 0xc1);
    PhyWrite (Hdptx, LNTOP_REG0203, 0xf0);
    PhyWrite (Hdptx, LNTOP_REG0204, 0x7c);
    PhyWrite (Hdptx, LNTOP_REG0205, 0x1f);
  }

  PhyWrite (Hdptx, LNTOP_REG0206, 0x07);
  PhyWrite (Hdptx, LANE_REG0303, 0x0c);
  PhyWrite (Hdptx, LANE_REG0307, 0x20);
  PhyWrite (Hdptx, LANE_REG030A, 0x17);
  PhyWrite (Hdptx, LANE_REG030B, 0x77);
  PhyWrite (Hdptx, LANE_REG030C, 0x77);
  PhyWrite (Hdptx, LANE_REG030D, 0x77);
  PhyWrite (Hdptx, LANE_REG030E, 0x38);
  PhyWrite (Hdptx, LANE_REG0310, 0x03);
  PhyWrite (Hdptx, LANE_REG0311, 0x0f);
  PhyWrite (Hdptx, LANE_REG0312, 0x00);
  PhyWrite (Hdptx, LANE_REG0316, 0x02);
  PhyWrite (Hdptx, LANE_REG031B, 0x01);
  PhyWrite (Hdptx, LANE_REG031E, 0x00);
  PhyWrite (Hdptx, LANE_REG031F, 0x15);
  PhyWrite (Hdptx, LANE_REG0320, 0xa0);
  PhyWrite (Hdptx, LANE_REG0403, 0x0c);
  PhyWrite (Hdptx, LANE_REG0407, 0x20);
  PhyWrite (Hdptx, LANE_REG040A, 0x17);
  PhyWrite (Hdptx, LANE_REG040B, 0x77);
  PhyWrite (Hdptx, LANE_REG040C, 0x77);
  PhyWrite (Hdptx, LANE_REG040D, 0x77);
  PhyWrite (Hdptx, LANE_REG040E, 0x38);
  PhyWrite (Hdptx, LANE_REG0410, 0x03);
  PhyWrite (Hdptx, LANE_REG0411, 0x0f);
  PhyWrite (Hdptx, LANE_REG0412, 0x00);
  PhyWrite (Hdptx, LANE_REG0416, 0x02);
  PhyWrite (Hdptx, LANE_REG041B, 0x01);
  PhyWrite (Hdptx, LANE_REG041E, 0x00);
  PhyWrite (Hdptx, LANE_REG041F, 0x15);
  PhyWrite (Hdptx, LANE_REG0420, 0xa0);
  PhyWrite (Hdptx, LANE_REG0503, 0x0c);
  PhyWrite (Hdptx, LANE_REG0507, 0x20);
  PhyWrite (Hdptx, LANE_REG050A, 0x17);
  PhyWrite (Hdptx, LANE_REG050B, 0x77);
  PhyWrite (Hdptx, LANE_REG050C, 0x77);
  PhyWrite (Hdptx, LANE_REG050D, 0x77);
  PhyWrite (Hdptx, LANE_REG050E, 0x38);
  PhyWrite (Hdptx, LANE_REG0510, 0x03);
  PhyWrite (Hdptx, LANE_REG0511, 0x0f);
  PhyWrite (Hdptx, LANE_REG0512, 0x00);
  PhyWrite (Hdptx, LANE_REG0516, 0x02);
  PhyWrite (Hdptx, LANE_REG051B, 0x01);
  PhyWrite (Hdptx, LANE_REG051E, 0x00);
  PhyWrite (Hdptx, LANE_REG051F, 0x15);
  PhyWrite (Hdptx, LANE_REG0520, 0xa0);
  PhyWrite (Hdptx, LANE_REG0603, 0x0c);
  PhyWrite (Hdptx, LANE_REG0607, 0x20);
  PhyWrite (Hdptx, LANE_REG060A, 0x17);
  PhyWrite (Hdptx, LANE_REG060B, 0x77);
  PhyWrite (Hdptx, LANE_REG060C, 0x77);
  PhyWrite (Hdptx, LANE_REG060D, 0x77);
  PhyWrite (Hdptx, LANE_REG060E, 0x38);
  PhyWrite (Hdptx, LANE_REG0610, 0x03);
  PhyWrite (Hdptx, LANE_REG0611, 0x0f);
  PhyWrite (Hdptx, LANE_REG0612, 0x00);
  PhyWrite (Hdptx, LANE_REG0616, 0x02);
  PhyWrite (Hdptx, LANE_REG061B, 0x01);
  PhyWrite (Hdptx, LANE_REG061E, 0x08);
  PhyWrite (Hdptx, LANE_REG061F, 0x15);
  PhyWrite (Hdptx, LANE_REG0620, 0xa0);

  PhyWrite (Hdptx, LANE_REG0303, 0x2f);
  PhyWrite (Hdptx, LANE_REG0403, 0x2f);
  PhyWrite (Hdptx, LANE_REG0503, 0x2f);
  PhyWrite (Hdptx, LANE_REG0603, 0x2f);
  PhyWrite (Hdptx, LANE_REG0305, 0x03);
  PhyWrite (Hdptx, LANE_REG0405, 0x03);
  PhyWrite (Hdptx, LANE_REG0505, 0x03);
  PhyWrite (Hdptx, LANE_REG0605, 0x03);
  PhyWrite (Hdptx, LANE_REG0306, 0x1c);
  PhyWrite (Hdptx, LANE_REG0406, 0x1c);
  PhyWrite (Hdptx, LANE_REG0506, 0x1c);
  PhyWrite (Hdptx, LANE_REG0606, 0x1c);

  return HdptxPostEnableLane (Hdptx);
}
