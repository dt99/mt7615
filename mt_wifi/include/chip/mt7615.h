#ifndef __MT7615_H__
#define __MT7615_H__

#include "mcu/andes_core.h"
#include "phy/mt_rf.h"


struct _RTMP_ADAPTER;
struct _MT_SWITCH_CHANNEL_CFG;

#define MAX_RF_ID	127
#define MAC_RF_BANK 7

#define MT7615_MT_WTBL_SIZE	128
#define MT7615_MT_WMM_SIZE	4
#define MT7615_PDA_PORT		0xf800

#define MT7615_BIN_FILE_PATH "/etc/Wireless/WIFI_RAM_CODE_MT7615.bin"


void mt7615_init(struct _RTMP_ADAPTER *pAd);
void mt7615_get_tx_pwr_per_rate(struct _RTMP_ADAPTER *pAd);
void mt7615_get_tx_pwr_info(struct _RTMP_ADAPTER *pAd);
void mt7615_antenna_sel_ctl(struct _RTMP_ADAPTER *pAd);
int mt7615_read_chl_pwr(struct _RTMP_ADAPTER *pAd);
void mt7615_pwrOn(struct _RTMP_ADAPTER *pAd);
void mt7615_calibration(struct _RTMP_ADAPTER *pAd, UCHAR channel);
void mt7615_tssi_compensation(struct _RTMP_ADAPTER *pAd, UCHAR channel);

#ifdef SINGLE_SKU_V2
VOID mt_FillBfBackOffParameter(struct _RTMP_ADAPTER *pAd, UINT8 Nsstream, UINT8 *BFGainTable);
VOID mt_FillBFBackoff(struct _RTMP_ADAPTER *pAd,UINT8 channel, UCHAR Band, UINT8 *BFPowerBackOff);
#endif /* SINGLE_SKU_V2 */

#ifdef MT7615_FPGA
INT mt7615_chk_top_default_cr_setting(struct _RTMP_ADAPTER *pAd);
INT mt7615_chk_hif_default_cr_setting(struct _RTMP_ADAPTER *pAd);
#endif /* MT7615_FPGA */

typedef enum _ENUM_MUBF_CAP_T {
    MUBF_OFF,
    MUBF_BFER,
    MUBF_BFEE,
    MUBF_ALL
} ENUM_MUBF_CAP_T, *P_ENUM_MUBF_CAP_T;

typedef enum _ENUM_BF_BACKOFF_TYPE_T {
    BF_BACKOFF_4T = 4,
    BF_BACKOFF_3T = 3,
    BF_BACKOFF_2T = 2
} ENUM_BF_BACKOFF_TYPE_T, *P_ENUM_BF_BACKOFF_TYPE_T;


INT Mt7615AsicArchOpsInit(struct _RTMP_ADAPTER *pAd);

#ifdef CAL_TO_FLASH_SUPPORT
enum {
	GBAND=0,
	ABAND=1,
};

/* RXDCOC */
#define DCOC_TO_FLASH_OFFSET  (1024)

extern UINT16 KtoFlashA20Freq[];
extern UINT16 KtoFlashA40Freq[];
extern UINT16 KtoFlashA80Freq[];
extern UINT16 KtoFlashG20Freq[];
extern UINT16 KtoFlashAllFreq[];

extern UINT16 K_A20_SIZE;
extern UINT16 K_A40_SIZE;
extern UINT16 K_A80_SIZE;
extern UINT16 K_G20_SIZE;
extern UINT16 K_ALL_SIZE;

/* TXDPD */
#define DPDPART1_TO_FLASH_OFFSET  (DCOC_TO_FLASH_OFFSET + K_ALL_SIZE * RXDCOC_TO_FLASH_SIZE)
#define DPDPART2_TO_FLASH_OFFSET  (DCOC_TO_FLASH_OFFSET + K_ALL_SIZE * RXDCOC_TO_FLASH_SIZE + 36 * TXDPD_TO_FLASH_SIZE)
#define TXDPD_PART1_LIMIT 36  /* 36*216 won't exceed 8K , array size exceeding 8K will cause problem */

extern UINT16 DPDtoFlashA20Freq[];
extern UINT16 DPDtoFlashG20Freq[];
extern UINT16 DPDtoFlashAllFreq[];
   
extern UINT16 DPD_A20_SIZE;
extern UINT16 DPD_G20_SIZE;
extern UINT16 DPD_ALL_SIZE;


void ShowDCOCDataFromFlash(struct _RTMP_ADAPTER *pAd, RXDCOC_RESULT_T RxDcocResult);
void ShowDPDDataFromFlash(struct _RTMP_ADAPTER *pAd, TXDPD_RESULT_T TxDPDResult);
void mt7615_apply_cal_data_from_flash(struct _RTMP_ADAPTER *pAd, struct _MT_SWITCH_CHANNEL_CFG SwChCfg);

#endif /* CAL_TO_FLASH_SUPPORT */

#endif // __MT7615_H__

