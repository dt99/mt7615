/*
 ***************************************************************************
 * MediaTek Inc. 
 *
 * All rights reserved. source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek, Inc. is obtained.
 ***************************************************************************

    Module Name:
    mt_rdm.h//Jelly20150123
*/


#ifndef _MT_RDM_H_
#define _MT_RDM_H_

#ifdef MT_DFS_SUPPORT

#include "rt_config.h"

//Remember add a RDM compile flag -- Shihwei 20141104
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define RDD_STOP     0
#define RDD_START    1
#define RDD_NODETSTOP    2	
#define RDD_DETSTOP   3
#define RDD_CACSTART    4
#define RDD_CACEND    5
#define RDD_NORMALSTART 6
#define RDD_DFSCAL 7
#define RDD_PULSEDBG 8
#define RDD_READPULSE 9

#define MAC_DFS_TXSTART 2

#define HW_RDD0      0
#define HW_RDD1      1
#define HW_RDD_RX0      0
#define HW_RDD_RX1      1
#define BAND0        0
#define BAND1        1

#define RESTRICTION_BAND_LOW	116
#define RESTRICTION_BAND_HIGH	128
#define CHAN_SWITCH_PERIOD 10
#define CHAN_NON_OCCUPANCY 1800
#define GROUP1_LOWER 36
#define GROUP1_UPPER 48
#define GROUP2_LOWER 52
#define GROUP2_UPPER 64

#define GROUP3_LOWER 100
#define GROUP3_UPPER 112


#define DFS_MACHINE_BASE	0
#define DFS_BEFORE_SWITCH    0
#define DFS_MAX_STATE      	1
#define DFS_CAC_END 0
#define DFS_CHAN_SWITCH_TIMEOUT 1
#define DFS_MAX_MSG			2
#define DFS_FUNC_SIZE (DFS_MAX_STATE * DFS_MAX_MSG)

#define UPPER_BW_160(_pAd)										\
	(_pAd->CommonCfg.RDDurRegion == FCC ? 64:128)                              

#define CH_NOSUPPORT_BW40(_ch)                                 \
    (_ch == 140 || _ch == 165) 

enum {  
    BW80Group1 = 1, //CH36~48
    BW80Group2,     //CH52~64
    BW80Group3,     //CH100~112 
    BW80Group4,     //CH116~128
    BW80Group5,     //CH132~144
    BW80Group6,     //CH149~161
};


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _DFS_CHANNEL_LIST {
    UCHAR Channel;
	USHORT NonOccupancy;
	UCHAR DfsReq;
	UCHAR Flags;
}DFS_CHANNEL_LIST, *PDFS_CHANNEL_LIST;

typedef struct _DFS_PARAM {
	UCHAR Band0Ch;//smaller channel number
	UCHAR Band1Ch;//larger channel number		
	UCHAR PrimCh;
	UCHAR PrimBand;
	UCHAR Bw;
	UCHAR RDDurRegion;
	DFS_CHANNEL_LIST DfsChannelList[MAX_NUM_OF_CHANNELS];
	UCHAR ChannelListNum;
	BOOLEAN bIEEE80211H;
	BOOLEAN DfsChBand[2];	
	BOOLEAN RadarDetected[2];
	DOT11_H Dot11_H;
	UCHAR RegTxSettingBW;
	BOOLEAN bDfsCheck;
	BOOLEAN RadarDetectState;
	BOOLEAN IsSetCountryRegion;
	BOOLEAN DisableDfsCal;
	BOOLEAN bNoSwitchCh;
	BOOLEAN bShowPulseInfo;
	BOOLEAN bDBDCMode;
	BOOLEAN bDfsEnable;
	STATE_MACHINE_FUNC 		DfsStateFunc[DFS_FUNC_SIZE]; 
	STATE_MACHINE 			DfsStatMachine;
} DFS_PARAM, *PDFS_PARAM;

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/


/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S 
********************************************************************************
*/


INT Set_RadarDetectStart_Proc(
    RTMP_ADAPTER *pAd, RTMP_STRING *arg);

INT Set_RadarDetectStop_Proc(
    RTMP_ADAPTER *pAd, RTMP_STRING *arg);

INT Set_ByPassCac_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg);

INT Set_RDDReport_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg);

VOID DfsGetSysParameters(
		IN PRTMP_ADAPTER pAd);

VOID DfsParamUpdateChannelList(//finish
	IN PRTMP_ADAPTER	pAd);

VOID DfsParamInit( // finish
	IN PRTMP_ADAPTER	pAd);

VOID DfsStateMachineInit(
	IN RTMP_ADAPTER *pAd,
	IN STATE_MACHINE *Sm,
	OUT STATE_MACHINE_FUNC Trans[]);

INT Set_DfsChannelShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsBwShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsRDModeShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsRDDRegionShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsNonOccupancyShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsPulseInfoMode_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

INT Set_DfsPulseInfoRead_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg);

BOOLEAN DfsEnterSilence(
    IN PRTMP_ADAPTER pAd);

VOID DfsSetCalibration(
	    IN PRTMP_ADAPTER pAd, UINT_32 DisableDfsCal);

BOOLEAN DfsRadarChannelCheck(
    IN PRTMP_ADAPTER pAd);

VOID DfsSetCountryRegion(
    IN PRTMP_ADAPTER pAd);

VOID DfsCacEndUpdate(
	RTMP_ADAPTER *pAd,
	MLME_QUEUE_ELEM *Elem);

VOID DfsChannelSwitchTimeoutAction(
	RTMP_ADAPTER *pAd,
	MLME_QUEUE_ELEM *Elem);

VOID DfsCacNormalStart(
    IN PRTMP_ADAPTER pAd);

BOOLEAN DfsCacRestrictBand(
	IN PRTMP_ADAPTER pAd, IN PDFS_PARAM pDfsParam);

VOID DfsSaveNonOccupancy(
    IN PRTMP_ADAPTER pAd);

VOID DfsRecoverNonOccupancy(
    IN PRTMP_ADAPTER pAd);

BOOLEAN DfsSwitchCheck(//finish
		IN PRTMP_ADAPTER	pAd);

#ifdef CONFIG_AP_SUPPORT
VOID DfsChannelSwitchParam( /*channel switch count down, finish*/
	IN PRTMP_ADAPTER	pAd);
#endif /* CONFIG_AP_SUPPORT */

VOID DfsNonOccupancyCountDown( /*RemainingTimeForUse --, finish*/
	IN PRTMP_ADAPTER pAd);

VOID WrapDfsRddReportHandle( /*handle the event of EXT_EVENT_ID_RDD_REPORT*/
	IN PRTMP_ADAPTER pAd, UCHAR ucRddIdx);

BOOLEAN DfsRddReportHandle( /*handle the event of EXT_EVENT_ID_RDD_REPORT*/
	IN PDFS_PARAM pDfsParam, UCHAR ucRddIdx);

VOID DfsCacEndHandle( /*handle the event of EXT_EVENT_ID_CAC_END*/
	IN PRTMP_ADAPTER pAd, UCHAR ucRddIdx);

VOID WrapDfsSetNonOccupancy( /*Set Channel non-occupancy time, finish */
	IN PRTMP_ADAPTER pAd);

VOID DfsSetNonOccupancy( /*Set Channel non-occupancy time, finish*/
	IN PRTMP_ADAPTER pAd, IN PDFS_PARAM pDfsParam);

VOID WrapDfsSelectChannel( /*Select new channel, finish*/
	IN PRTMP_ADAPTER pAd);

VOID DfsSelectChannel( /*Select new channel, finish*/
	IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam);

UCHAR WrapDfsRandomSelectChannel( /*Select new channel using random selection, finish*/
	IN PRTMP_ADAPTER pAd);

UCHAR DfsRandomSelectChannel( /*Select new channel using random selection, finish*/
	IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam);

VOID WrapDfsRadarDetectStart( /*Start Radar Detection or not, finish*/
   IN PRTMP_ADAPTER pAd);

VOID DfsRadarDetectStart( /*Start Radar Detection or not, finish*/
   IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam);

VOID WrapDfsRadarDetectStop( /*Start Radar Detection or not*/
	IN PRTMP_ADAPTER pAd);

VOID DfsRadarDetectStop( /*Start Radar Detection or not, finish*/
   IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam);

INT mtRddControl(
    IN struct _RTMP_ADAPTER *pAd,
    IN UCHAR ucRddCtrl,
    IN UCHAR ucRddIdex,
    IN UCHAR ucRddInSel);

#endif /*MT_DFS_SUPPORT*/
#endif /*_MT_RDM_H_ */
