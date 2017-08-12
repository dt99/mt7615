/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2013, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All related CFG80211 function body.

	History:

***************************************************************************/
#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT
#ifdef CONFIG_AP_SUPPORT

#include "rt_config.h"

#ifdef MT_MAC
VOID write_tmac_info_beacon(RTMP_ADAPTER *pAd, INT apidx, UCHAR *tmac_buf, HTTRANSMIT_SETTING *BeaconTransmit, ULONG frmLen);
#endif /* MT_MAC */

static INT CFG80211DRV_UpdateTimIE(PRTMP_ADAPTER pAd, UINT mbss_idx, PUCHAR pBeaconFrame, UINT32 tim_ie_pos)
{
	UCHAR  ID_1B, TimFirst, TimLast, *pTim, *ptr, New_Tim_Len;
	UINT  i;
    struct wifi_dev *wdev = NULL;
    BCN_BUF_STRUC *bcn_buf = NULL;

	ptr = pBeaconFrame + tim_ie_pos; /* TIM LOCATION */
	*ptr = IE_TIM;
	*(ptr + 2) = pAd->ApCfg.DtimCount;
	*(ptr + 3) = pAd->ApCfg.DtimPeriod;

	TimFirst = 0; /* record first TIM byte != 0x00 */
	TimLast = 0;  /* record last  TIM byte != 0x00 */

    wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;
    bcn_buf = &wdev->bcn_buf;
	pTim = bcn->TimBitmaps;

	for(ID_1B=0; ID_1B < WLAN_MAX_NUM_OF_TIM; ID_1B++)
	{
		/* get the TIM indicating PS packets for 8 stations */
		UCHAR tim_1B = pTim[ID_1B];

		if (ID_1B == 0)
			tim_1B &= 0xfe; /* skip bit0 bc/mc */

		if (tim_1B == 0)
			continue; /* find next 1B */

		if (TimFirst == 0)
			TimFirst = ID_1B;

		TimLast = ID_1B;
	}

	/* fill TIM content to beacon buffer */
	if (TimFirst & 0x01)
		TimFirst --; /* find the even offset byte */

	*(ptr + 1) = 3 + (TimLast - TimFirst + 1); /* TIM IE length */
	*(ptr + 4) = TimFirst;

	for(i=TimFirst; i <= TimLast; i++)
		*(ptr + 5 + i - TimFirst) = pTim[i];

	/* bit0 means backlogged mcast/bcast */
    if (pAd->ApCfg.DtimCount == 0)
		*(ptr + 4) |= (bcn_buf->TimBitmaps[WLAN_CT_TIM_BCMC_OFFSET] & 0x01);

	/* adjust BEACON length according to the new TIM */
	New_Tim_Len = (2 + *(ptr+1));

	return New_Tim_Len;
}

static INT CFG80211DRV_UpdateApSettingFromBeacon(PRTMP_ADAPTER pAd, UINT mbss_idx, CMD_RTPRIV_IOCTL_80211_BEACON *pBeacon)
{
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;

	const UCHAR *ssid_ie = NULL, *wpa_ie = NULL, *rsn_ie = NULL;
	const UCHAR *supp_rates_ie = NULL;
	const UCHAR *ext_supp_rates_ie = NULL, *ht_cap = NULL, *ht_info = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0))
	const UCHAR CFG_HT_OP_EID = WLAN_EID_HT_OPERATION;
#else
	const UCHAR CFG_HT_OP_EID = WLAN_EID_HT_INFORMATION;
#endif /* LINUX_VERSION_CODE: 3.5.0 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
	const UCHAR CFG_WPA_EID = WLAN_EID_VENDOR_SPECIFIC;
#else
	const UCHAR CFG_WPA_EID = WLAN_EID_WPA;
#endif /* LINUX_VERSION_CODE: 3.8.0 */

	ssid_ie = cfg80211_find_ie(WLAN_EID_SSID, pBeacon->beacon_head+36, pBeacon->beacon_head_len-36);
	supp_rates_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, pBeacon->beacon_head+36, pBeacon->beacon_head_len-36);
	/* if it doesn't find WPA_IE in tail first 30 bytes. treat it as is not found */
	wpa_ie = cfg80211_find_ie(CFG_WPA_EID, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	rsn_ie = cfg80211_find_ie(WLAN_EID_RSN, pBeacon->beacon_tail, pBeacon->beacon_tail_len);//wpa2 case.
	ext_supp_rates_ie = cfg80211_find_ie(WLAN_EID_EXT_SUPP_RATES, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	ht_cap = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	ht_info = cfg80211_find_ie(CFG_HT_OP_EID, pBeacon->beacon_tail, pBeacon->beacon_tail_len);


	/* SSID */
	os_zero_mem(pMbss->Ssid, pMbss->SsidLen);
	if (ssid_ie == NULL)
	{
		os_move_mem(pMbss->Ssid, "CFG_Linux_GO", 12);
		pMbss->SsidLen = 12;
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("CFG: SSID Not Found In Packet\n"));
	}
	else
	{
		pMbss->SsidLen = ssid_ie[1];
		NdisCopyMemory(pMbss->Ssid, ssid_ie+2, pMbss->SsidLen);
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("CFG : SSID: %s, %d\n", pMbss->Ssid, pMbss->SsidLen));
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
	if (pBeacon->hidden_ssid > 0 && pBeacon->hidden_ssid < 3) {
		pMbss->bHideSsid = TRUE;
	}
	else
		pMbss->bHideSsid = FALSE;

	if (pBeacon->hidden_ssid == 1)
		pMbss->SsidLen = 0;
#endif /* LINUX_VERSION_CODE 3.4.0 */

	/* WMM EDCA Paramter */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	CFG80211_SyncPacketWmmIe(pAd, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
#endif /* LINUX_VERSION_CODE 3.2.0 */

	CFG80211_ParseBeaconIE(pAd, pMbss, wdev, (UCHAR *)wpa_ie, (UCHAR *)rsn_ie);

	pMbss->CapabilityInfo =	CAP_GENERATE(1, 0, (wdev->WepStatus != Ndis802_11EncryptionDisabled),
			 (pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong ? 0 : 1), pAd->CommonCfg.bUseShortSlotTime, /*SpectrumMgmt*/FALSE);

	/* Disable Driver-Internal Rekey */
	pMbss->WPAREKEY.ReKeyInterval = 0;
	pMbss->WPAREKEY.ReKeyMethod = DISABLE_REKEY;

	if (pBeacon->interval != 0)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("CFG_TIM New BI %d\n", pBeacon->interval));
		pAd->CommonCfg.BeaconPeriod = pBeacon->interval;
	}

	if (pBeacon->dtim_period != 0)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CFG_TIM New DP %d\n", pBeacon->dtim_period));
		pAd->ApCfg.DtimPeriod = pBeacon->dtim_period;
	}

	return TRUE;
}

VOID CFG80211DRV_DisableApInterface(PRTMP_ADAPTER pAd)
{
#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else
	UINT apidx = MAIN_MBSSID;
#endif /*RT_CFG80211_P2P_SUPPORT*/
	/*CFG_TODO: IT Should be set fRTMP_ADAPTER_HALT_IN_PROGRESS */
	struct wifi_dev *pWdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.bBcnSntReq = FALSE;
  	/* For AP - STA switch */
	if (pAd->CommonCfg.BBPCurrentBW != BW_40)
	{
		CFG80211DBG(DBG_LVL_TRACE, ("80211> %s, switch to BW_20\n", __FUNCTION__));
		HcBbpSetBwByChannel(pAd,BW_20,pWdev->channel)
   	}

    /* Disable pre-TBTT interrupt */
    AsicSetPreTbtt(pAd, FALSE, HW_BSSID_0);

    if (!INFRA_ON(pAd))
    {
		/* Disable piggyback */
		AsicSetPiggyBack(pAd, FALSE);
		AsicUpdateProtect(pAd, 0,  (ALLN_SETPROTECT|CCKSETPROTECT|OFDMSETPROTECT), TRUE, FALSE);
    }

    if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
    {
        AsicDisableSync(pAd, HW_BSSID_0);
    }

#ifdef RTMP_MAC_SDIO
    /* For RT2870, we need to clear the beacon sync buffer. */
    MTSDIOBssBeaconExit(pAd);
#endif /* RTMP_MAC_SDIO */
	OPSTATUS_CLEAR_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);
	RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);
}

#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
PCHAR rtstrstr2(PCHAR s1,const PCHAR s2, INT s1_len, INT s2_len)
{
	INT offset=0;
	while (s1_len >= s2_len)
	{
		s1_len--;
		if (!memcmp(s1,s2,s2_len))
		{
			return offset;
		}
		s1++;
		offset++;
	}
	return NULL;
}
#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */

VOID CFG80211_UpdateBeacon(
	VOID                                            *pAdOrg,
	UCHAR 										    *beacon_head_buf,
	UINT32											beacon_head_len,
	UCHAR 										    *beacon_tail_buf,
	UINT32											beacon_tail_len,
	BOOLEAN											isAllUpdate)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	HTTRANSMIT_SETTING BeaconTransmit;   /* MGMT frame PHY rate setting when operatin at Ht rate. */
	PUCHAR pBeaconFrame;
	UCHAR *tmac_info, New_Tim_Len = 0;
	UINT32 beacon_len = 0;
	BSS_STRUCT *pMbss;
	struct wifi_dev *wdev;
	COMMON_CONFIG *pComCfg;
#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else /* RT_CFG80211_P2P_SUPPORT */
	UINT apidx = MAIN_MBSSID;
#endif /* !RT_CFG80211_P2P_SUPPORT */
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
	ULONG	Value;
	ULONG	TimeTillTbtt;
	ULONG	temp;
	INT		bufferoffset =0;
	USHORT		bufferoffset2 =0;
	CHAR 	temp_buf[512]={0};
	CHAR	P2POUIBYTE[4] = {0x50, 0x6f, 0x9a, 0x9};
	INT	temp_len;
	INT P2P_IE=4;
	USHORT p2p_ie_len;
	UCHAR Count;
	ULONG StartTime;
#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */
	UCHAR tx_hw_hdr_len = pAd->chipCap.tx_hw_hdr_len;
    UINT8 TXWISize = pAd->chipCap.TXWISize;

	pComCfg = &pAd->CommonCfg;
    pMbss = &pAd->ApCfg.MBSSID[apidx];
    wdev = &pMbss->wdev;

	if (!pMbss || !pMbss->wdev.bcn_buf.BeaconPkt)
                return;

	tmac_info = (UCHAR *)GET_OS_PKT_DATAPTR(pMbss->wdev.bcn_buf.BeaconPkt);

#ifdef MT_MAC
        if (pAd->chipCap.hif_type == HIF_MT)
        {
                pBeaconFrame = (UCHAR *)(tmac_info + tx_hw_hdr_len);
        }
        else
#endif /* MT_MAC */
        {
                pBeaconFrame = (UCHAR *)(tmac_info + TXWISize);
        }

	if (isAllUpdate) /* Invoke From CFG80211 OPS For setting Beacon buffer */
	{
		/* 1. Update the Buf before TIM IE */
		NdisCopyMemory(pBeaconFrame, beacon_head_buf, beacon_head_len);

		/* 2. Update the Location of TIM IE */
		pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.TimIELocationInBeacon = beacon_head_len;

		/* 3. Store the Tail Part For appending later */
		if (pCfg80211_ctrl->beacon_tail_buf != NULL)
			 os_free_mem(pCfg80211_ctrl->beacon_tail_buf);

		os_alloc_mem(NULL, (UCHAR **)&pCfg80211_ctrl->beacon_tail_buf, beacon_tail_len);
		if (pCfg80211_ctrl->beacon_tail_buf != NULL)
		{
			NdisCopyMemory(pCfg80211_ctrl->beacon_tail_buf, beacon_tail_buf, beacon_tail_len);
			pCfg80211_ctrl->beacon_tail_len = beacon_tail_len;
		}
		else
		{
			pCfg80211_ctrl->beacon_tail_len = 0;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("CFG80211 Beacon: MEM ALLOC ERROR\n"));
		}

		return;
	}
	else /* Invoke From Beacon Timer */
	{
		if (pAd->ApCfg.DtimCount == 0)
			pAd->ApCfg.DtimCount = pAd->ApCfg.DtimPeriod - 1;
		else
			pAd->ApCfg.DtimCount -= 1;
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
/*
	3 mode:
		1. infra scan  7 channel  ( Duration(30+3) *7   interval (+120)  *   count  1 ),
		2. p2p find    3 channel   (Duration (65 ) *3     interval (+130))  * count 2   > 120 sec
		3. mcc  tw channel switch (Duration )  (Infra time )  interval (+ GO time )  count 3  mcc enabel always;
*/

			if (pAd->cfg80211_ctrl.GONoASchedule.Count > 0)
			{
				if (pAd->cfg80211_ctrl.GONoASchedule.Count != 200 )
					pAd->cfg80211_ctrl.GONoASchedule.Count  --;
				os_move_mem(temp_buf, pCfg80211_ctrl->beacon_tail_buf, pCfg80211_ctrl->beacon_tail_len);
				bufferoffset = rtstrstr2(temp_buf, P2POUIBYTE,pCfg80211_ctrl->beacon_tail_len,P2P_IE);
				while (bufferoffset2 <= (pCfg80211_ctrl->beacon_tail_len -bufferoffset -4 -bufferoffset2 -3))
				{
					if ( (pCfg80211_ctrl->beacon_tail_buf)[bufferoffset+4+bufferoffset2] == 12)
					{
						break;
					}
					else
					{
						bufferoffset2 = pCfg80211_ctrl->beacon_tail_buf[bufferoffset + 4 +1+bufferoffset2]+bufferoffset2;
						bufferoffset2 = bufferoffset2+3;
					}
				}

				NdisCopyMemory(&pCfg80211_ctrl->beacon_tail_buf[bufferoffset+4+bufferoffset2+5] , &pAd->cfg80211_ctrl.GONoASchedule.Count, 1);
				NdisCopyMemory(&pCfg80211_ctrl->beacon_tail_buf[bufferoffset+4+bufferoffset2+6], &pAd->cfg80211_ctrl.GONoASchedule.Duration, 4);
				NdisCopyMemory(&pCfg80211_ctrl->beacon_tail_buf[bufferoffset+4+bufferoffset2+10], &pAd->cfg80211_ctrl.GONoASchedule.Interval, 4);
				NdisCopyMemory(&pCfg80211_ctrl->beacon_tail_buf[bufferoffset+4+bufferoffset2+14], &pAd->cfg80211_ctrl.GONoASchedule.StartTime, 4);

			}

#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */



	}

#ifdef MT_MAC
	if (pAd->chipCap.hif_type == HIF_MT)
	{
#ifdef RTMP_PCI_SUPPORT
		BOOLEAN is_pretbtt_int = FALSE;
		UCHAR RingIdx=0;
        USHORT FreeNum;
		
#if defined(MT7615) || defined(MT7622)
		RingIdx = HcGetTxRingIdx(pAd,wdev);
#endif
		FreeNum = GET_BCNRING_FREENO(pAd,RingIdx);
		if (FreeNum < 0) {
	    		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s()=>BSS0:BcnRing FreeNum is not enough!\n",
	                                        __FUNCTION__));
	    		return;
		}

        if (pMbss->wdev.bcn_buf.bcn_state != BCN_TX_IDLE) {
            MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s()=>BSS0:BcnPkt not idle(%d)!\n",
                                    __FUNCTION__, pMbss->wdev.bcn_buf.bcn_state));
        	APCheckBcnQHandler(pAd, apidx, &is_pretbtt_int);
            if (is_pretbtt_int == FALSE)
			{
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("==============> pretbtt_int not init \n"));
                return;
			}
		}
#endif /* RTMP_PCI_SUPPORT */
}
#endif /* MT_MAC */

	/* 4. Update the TIM IE */
	New_Tim_Len = CFG80211DRV_UpdateTimIE(pAd, apidx, pBeaconFrame,
				pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.TimIELocationInBeacon);

	/* 5. Update the Buffer AFTER TIM IE */
	if (pCfg80211_ctrl->beacon_tail_buf != NULL)
	{
		NdisCopyMemory(pBeaconFrame + pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.TimIELocationInBeacon + New_Tim_Len,
			       pCfg80211_ctrl->beacon_tail_buf, pCfg80211_ctrl->beacon_tail_len);

		beacon_len = pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.TimIELocationInBeacon + pCfg80211_ctrl->beacon_tail_len
			     + New_Tim_Len;
	}
	else
	{
		 MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("BEACON ====> CFG80211_UpdateBeacon OOPS\n"));
		 return;
	}


    BeaconTransmit.word = 0;
	/* Should be Find the P2P IE Then Set Basic Rate to 6M */
#ifdef RT_CFG80211_P2P_SUPPORT
	if (RTMP_CFG80211_VIF_P2P_GO_ON(pAd))
	BeaconTransmit.field.MODE = MODE_OFDM; /* Use 6Mbps */
	else
#endif /*RT_CFG80211_P2P_SUPPORT*/
		BeaconTransmit.field.MODE = MODE_CCK;

	BeaconTransmit.field.MCS = MCS_RATE_6;

	write_tmac_info_beacon(pAd, &pAd->ApCfg.MBSSID[apidx].wdev, tmac_info, &BeaconTransmit, beacon_len);

	/* CFG_TODO */
	RT28xx_UpdateBeaconToAsic(pAd, apidx, beacon_len,
			pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.TimIELocationInBeacon);

}

BOOLEAN CFG80211DRV_OpsBeaconSet(VOID *pAdOrg, VOID *pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_BEACON *pBeacon;
#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else
	UINT apidx = MAIN_MBSSID;
#endif /*RT_CFG80211_P2P_SUPPORT*/

	pBeacon = (CMD_RTPRIV_IOCTL_80211_BEACON *)pData;
	CFG80211DRV_UpdateApSettingFromBeacon(pAd, apidx, pBeacon);
	CFG80211_UpdateBeacon(pAd, pBeacon->beacon_head, pBeacon->beacon_head_len,
			  				   pBeacon->beacon_tail, pBeacon->beacon_tail_len,
							   TRUE);

	return TRUE;
}

BOOLEAN CFG80211DRV_OpsBeaconAdd(VOID *pAdOrg, VOID *pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_BEACON *pBeacon;
	BOOLEAN Cancelled;
	UINT i = 0;
	INT32 Ret = 0;
	EDCA_PARM *pEdca;
#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else
	UINT apidx = MAIN_MBSSID;
#endif /*RT_CFG80211_P2P_SUPPORT*/
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apidx];
	struct wifi_dev *wdev = &pMbss->wdev;
    BCN_BUF_STRUC *bcn_buf = &wdev->bcn_buf;
	CHAR tr_tb_idx = MAX_LEN_OF_MAC_TABLE + apidx;
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	PNET_DEV pNetDev = NULL;
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
#ifdef RT_CFG80211_SUPPORT
#ifdef RT_CFG80211_P2P_SUPPORT
	if (!RTMP_CFG80211_VIF_P2P_GO_ON(pAd))
#endif
		wdev->Hostapd=Hostapd_CFG;
#endif
	CFG80211DBG(DBG_LVL_TRACE, ("80211> %s ==>\n", __FUNCTION__));
#if defined(RTMP_MAC_USB) || defined(RTMP_MAC_SDIO)
#ifdef CONFIG_AP_SUPPORT
	RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);
#endif /*CONFIG_AP_SUPPORT*/
#endif /*RTMP_MAC_USB*/

	pBeacon = (CMD_RTPRIV_IOCTL_80211_BEACON *)pData;

#ifdef UAPSD_SUPPORT
        wdev->UapsdInfo.bAPSDCapable = TRUE;
        pMbss->CapabilityInfo |= 0x0800;
#endif /* UAPSD_SUPPORT */

	CFG80211DRV_UpdateApSettingFromBeacon(pAd, apidx, pBeacon);

	AsicSetRxFilter(pAd);

	/* Start from 0 & MT_MAC using HW_BSSID 1, TODO */
#ifdef RT_CFG80211_P2P_SUPPORT
	pAd->ApCfg.BssidNum = (CFG_GO_BSSID_IDX + 1);
#else
	pAd->ApCfg.BssidNum = (MAIN_MBSSID + 1);
#endif /*RT_CFG80211_P2P_SUPPORT*/

	pAd->MacTab.MsduLifeTime = 20; /* pEntry's UAPSD Q Idle Threshold */
	/* CFG_TODO */
    bcn_buf->BcnBufIdx = 0 ;
	for(i = 0; i < WLAN_MAX_NUM_OF_TIM; i++)
                bcn_buf->TimBitmaps[i] = 0;

	bcn_buf->bBcnSntReq = TRUE;

	/* For GO Timeout */
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
	pAd->ApCfg.StaIdleTimeout = 300;
	pMbss->StationKeepAliveTime = 60;
#else
	pAd->ApCfg.StaIdleTimeout = 300;
	pMbss->StationKeepAliveTime = 0;
#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */

	AsicDisableSync(pAd, HW_BSSID_0);

	if (pAd->CommonCfg.Channel > 14)
		pAd->CommonCfg.PhyMode = (WMODE_A | WMODE_AN);
	else
		pAd->CommonCfg.PhyMode = (WMODE_B | WMODE_G |WMODE_GN);

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	/* Using netDev ptr from VifList if VifDevList Exist */
	if ((pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList.size > 0) &&
	   ((pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, RT_CMD_80211_IFTYPE_P2P_GO)) != NULL))
	{
		Ret = wdev_init(pAd, wdev, WDEV_TYPE_GO, pNetDev, apidx, (VOID *)&pAd->ApCfg.MBSSID[apidx], (VOID *)pAd);
		wdev_attr_update(pAd,&pMbss->wdev);

		if (Ret == FALSE)
		{
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s(): register wdev fail\n", __FUNCTION__));
		}

		COPY_MAC_ADDR(wdev->bssid, pNetDev->dev_addr);
		COPY_MAC_ADDR(wdev->if_addr, pNetDev->dev_addr);
        os_move_mem(wdev->bss_info_argument.Bssid,wdev->bssid,MAC_ADDR_LEN);

		RTMP_OS_NETDEV_SET_WDEV(pNetDev, wdev);
		RTMP_OS_NETDEV_SET_PRIV(pNetDev, pAd);
	}
	else
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
	{
		Ret = wdev_init(pAd, wdev, WDEV_TYPE_GO, pAd->net_dev, apidx, (VOID *)&pAd->ApCfg.MBSSID[apidx], (VOID *)pAd);
		wdev_attr_update(pAd,wdev);

		if (Ret == FALSE)
		{
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s(): register wdev fail\n", __FUNCTION__));
		}

		COPY_MAC_ADDR(wdev->bssid, pAd->CurrentAddress);
		COPY_MAC_ADDR(wdev->if_addr, pAd->CurrentAddress);
        os_move_mem(wdev->bss_info_argument.Bssid,wdev->bssid,MAC_ADDR_LEN);

		/* assoc to MBSSID's wdev */
		RTMP_OS_NETDEV_SET_WDEV(pAd->net_dev, wdev);
		RTMP_OS_NETDEV_SET_PRIV(pAd->net_dev, pAd);
	}

	/* cfg_todo */
	wdev->bWmmCapable = TRUE;
	os_move_mem(wdev->bss_info_argument.Bssid,wdev->bssid,MAC_ADDR_LEN);

	/* BC/MC Handling */
    TRTableInsertMcastEntry(pAd, tr_tb_idx, wdev);

#ifdef RT_CFG80211_P2P_SUPPORT
	bcn_buf_init(pAd, &pAd->ApCfg.MBSSID[apidx].wdev);
#endif /*RT_CFG80211_P2P_SUPPORT*/

	wdev->allow_data_tx = TRUE;

    wdev->bss_info_argument.Active = TRUE;
    wdev->bss_info_argument.u4BssInfoFeature = (BSS_INFO_OWN_MAC_FEATURE |
                                        BSS_INFO_BASIC_FEATURE |
                                        BSS_INFO_RF_CH_FEATURE |
                                        BSS_INFO_SYNC_MODE_FEATURE);

    AsicBssInfoUpdate(pAd, wdev->bss_info_argument);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("New AP BSSID %02x:%02x:%02x:%02x:%02x:%02x (%d)\n",
		PRINT_MAC(wdev->bssid), pAd->CommonCfg.PhyMode));

	RTMPSetPhyMode(pAd, pAd->CommonCfg.PhyMode);

#ifdef DOT11_N_SUPPORT
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode) && (pAd->Antenna.field.TxPath == 2))
		bbp_set_txdac(pAd, 2);
	else
#endif /* DOT11_N_SUPPORT */
		bbp_set_txdac(pAd, 0);

	/* Receiver Antenna selection */
	bbp_set_rxpath(pAd, pAd->Antenna.field.RxPath);

	if(!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		if (WMODE_CAP_N(pAd->CommonCfg.PhyMode) || wdev->bWmmCapable)
		{
			pEdca = &pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx];
			/* EDCA parameters used for AP's own transmission */
			if (pEdca->bValid == FALSE)
				set_default_ap_edca_param(pEdca);

			/* EDCA parameters to be annouced in outgoing BEACON, used by WMM STA */
			if (pAd->ApCfg.BssEdcaParm.bValid == FALSE)
				set_default_sta_edca_param(&pAd->ApCfg.BssEdcaParm);

			HcAcquiredEdca(pAd,wdev,pEdca);
            HcSetEdca(wdev);
		}
		else
		{
			HcReleaseEdca(pAd,wdev);
		}

	}

#ifdef DOT11_N_SUPPORT
#ifndef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode))
		pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth = BW_20; /* Patch UI */
#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */

	AsicSetRDG(pAd, pAd->CommonCfg.bRdg);

	AsicSetRalinkBurstMode(pAd, pAd->CommonCfg.bRalinkBurstMode);
#endif /* DOT11_N_SUPPORT */

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():Reset WCID Table\n", __FUNCTION__));
	HW_SET_DEL_ASIC_WCID(pAd, WCID_ALL);

	pAd->MacTab.Content[0].Addr[0] = 0x01;
	pAd->MacTab.Content[0].HTPhyMode.field.MODE = MODE_OFDM;
	pAd->MacTab.Content[0].HTPhyMode.field.MCS = 3;
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
#ifdef DOT11_N_SUPPORT
	SetCommonHtVht(pAd,NULL);
#endif /* DOT11_N_SUPPORT */

	/*In MCC  & p2p GO not support VHT now, */
	/*change here for support P2P GO 40 BW*/
	/*	pAd->CommonCfg.vht_bw = 0;*/
	if(wdev->extcha== EXTCHA_BELOW)
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 2;
	else if (wdev->extcha == EXTCHA_ABOVE)
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel + 2;
	else
	pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;

	AsicSwitchChannel(pAd, pAd->CommonCfg.CentralChannel,FALSE);
       AsicLockChannel(pAd, pAd->CommonCfg.CentralChannel);
	HcBbpSetBwByChannel(pAd,wdev->bw,pAd->CommonCfg.CentralChannel)
#else
	pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */


	AsicBBPAdjust(pAd,wdev->band_idx);
	//MlmeSetTxPreamble(pAd, (USHORT)pAd->CommonCfg.TxPreamble);
	//MlmeUpdateTxRates(pAd, FALSE, MIN_NET_DEVICE_FOR_CFG80211_VIF_P2P_GO + apidx);
#ifdef RT_CFG80211_P2P_SUPPORT
	MlmeUpdateTxRates(pAd, FALSE, MIN_NET_DEVICE_FOR_CFG80211_VIF_P2P_GO + apidx);
#else
	MlmeUpdateTxRates(pAd, FALSE, apidx);
#endif /*RT_CFG80211_P2P_SUPPORT*/

#ifdef DOT11_N_SUPPORT
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode))
		MlmeUpdateHtTxRates(pAd, MIN_NET_DEVICE_FOR_MBSSID);
#endif /* DOT11_N_SUPPORT */

	/* Disable Protection first. */
	if (!INFRA_ON(pAd))
		AsicUpdateProtect(pAd, 0, (ALLN_SETPROTECT|CCKSETPROTECT|OFDMSETPROTECT), TRUE, FALSE);

	APUpdateCapabilityAndErpIe(pAd);
#ifdef DOT11_N_SUPPORT
	APUpdateOperationMode(pAd, wdev);
#endif /* DOT11_N_SUPPORT */
	CFG80211_UpdateBeacon(pAd, pBeacon->beacon_head, pBeacon->beacon_head_len,
	                       pBeacon->beacon_tail, pBeacon->beacon_tail_len, TRUE);
#ifdef RTMP_MAC_SDIO
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s @@ ---> %d\n", __FUNCTION__, __LINE__));
	MTSDIOBssBeaconInit(pAd);
	MTSDIOBssBeaconStart(pAd);
#endif


#if defined(RTMP_MAC_USB) || defined(RTMP_MAC_SDIO)
        RTMPInitTimer(pAd, &pAd->CommonCfg.BeaconUpdateTimer, GET_TIMER_FUNCTION(BeaconUpdateExec), pAd, TRUE);
#endif /* RTMP_MAC_USB || RTMP_MAC_SDIO */
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
	if (INFRA_ON(pAd))
	{
		ULONG BPtoJiffies;
		LONG timeDiff;
		INT starttime= pAd->Mlme.channel_1st_staytime;
		NdisGetSystemUpTime(&pAd->Mlme.BeaconNow32);

		timeDiff = (pAd->Mlme.BeaconNow32 - pAd->StaCfg[0].LastBeaconRxTime) % (pAd->CommonCfg.BeaconPeriod);
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("#####pAd->Mlme.Now32 %d pAd->StaCfg[0].LastBeaconRxTime %d \n",pAd->Mlme.BeaconNow32,pAd->StaCfg[0].LastBeaconRxTime));
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("####    timeDiff %d \n",timeDiff));
		if (starttime > timeDiff)
		{
			OS_WAIT((starttime - timeDiff));
		}
		else{
			OS_WAIT((starttime + (pAd->CommonCfg.BeaconPeriod - timeDiff)));
		}
	}

#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */


	/* Enable BSS Sync*/
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT
	if (INFRA_ON(pAd))
	{
		ULONG BPtoJiffies;
		LONG timeDiff;
		INT starttime= pAd->Mlme.channel_1st_staytime;
		NdisGetSystemUpTime(&pAd->Mlme.BeaconNow32);
		timeDiff = (pAd->Mlme.BeaconNow32 - pAd->StaCfg[0].LastBeaconRxTime) % (pAd->CommonCfg.BeaconPeriod);
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("#####pAd->Mlme.Now32 %d pAd->StaCfg[0].LastBeaconRxTime %d \n",pAd->Mlme.BeaconNow32,pAd->StaCfg[0].LastBeaconRxTime));
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("####    timeDiff %d \n",timeDiff));
		if (starttime > timeDiff)
		{
			OS_WAIT((starttime - timeDiff));
		}
		else{
			OS_WAIT((starttime + (pAd->CommonCfg.BeaconPeriod - timeDiff)));
		}
	}

#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */


	/* Enable AP BSS Sync */
	AsicEnableApBssSync(pAd, pAd->CommonCfg.BeaconPeriod);
	//pAd->P2pCfg.bSentProbeRSP = TRUE;

	AsicSetPreTbtt(pAd, TRUE, HW_BSSID_0);


	OPSTATUS_SET_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);
	RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);

	return TRUE;
}

BOOLEAN CFG80211DRV_ApKeyDel(
	VOID                                            *pAdOrg,
	VOID                                            *pData)
{
    PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
    CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;
	MAC_TABLE_ENTRY *pEntry;
	ASIC_SEC_INFO Info = {0};	

#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else
	UINT apidx = MAIN_MBSSID;
#endif /*RT_CFG80211_P2P_SUPPORT*/


	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        if (pKeyInfo->bPairwise == FALSE )
#else
        if (pKeyInfo->KeyId > 0)
#endif
	{
		/* Set key material to Asic */
		os_zero_mem(&Info, sizeof(ASIC_SEC_INFO));
		Info.Operation = SEC_ASIC_REMOVE_GROUP_KEY;
		Info.Wcid = BSS0;
		
		/* Set key material to Asic */
		HW_ADDREMOVE_KEYTABLE(pAd, &Info);
	}
	else
	{
		pEntry = MacTableLookup(pAd, pKeyInfo->MAC);

		if (pEntry && (pEntry->Aid != 0))
		{
			/* Set key material to Asic */
			os_zero_mem(&Info, sizeof(ASIC_SEC_INFO));
			Info.Operation = SEC_ASIC_REMOVE_PAIRWISE_KEY;
			Info.Wcid = pEntry->wcid;
			
			/* Set key material to Asic */
			HW_ADDREMOVE_KEYTABLE(pAd, &Info);
		}
	}

	return TRUE;
}

VOID CFG80211DRV_RtsThresholdAdd(
	VOID                                            *pAdOrg,
	UINT                                            threshold)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;

		if((threshold > 0) && (threshold <= MAX_RTS_THRESHOLD))
			pAd->CommonCfg.RtsThreshold  = (USHORT)threshold;
}


VOID CFG80211DRV_FragThresholdAdd(
	VOID                                            *pAdOrg,
	UINT                                            threshold)
{
		PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
		if (threshold > MAX_FRAG_THRESHOLD || threshold < MIN_FRAG_THRESHOLD)
		{
			/*Illegal FragThresh so we set it to default*/
			pAd->CommonCfg.FragmentThreshold = MAX_FRAG_THRESHOLD;
		}
		else if (threshold % 2 == 1)
		{
			/*
				The length of each fragment shall always be an even number of octets,
				except for the last fragment of an MSDU or MMPDU, which may be either
				an even or an odd number of octets.
			*/
			pAd->CommonCfg.FragmentThreshold = (USHORT)(threshold - 1);
		}
		else
		{
			pAd->CommonCfg.FragmentThreshold = (USHORT)threshold;
		}

}

BOOLEAN CFG80211DRV_ApKeyAdd(
	VOID                                            *pAdOrg,
	VOID                                            *pData)
{
#ifdef CONFIG_AP_SUPPORT
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;
	MAC_TABLE_ENTRY *pEntry = NULL;
	UINT Wcid = 0;
#ifdef RT_CFG80211_P2P_SUPPORT
	UINT apidx = CFG_GO_BSSID_IDX;
#else
	UINT apidx = MAIN_MBSSID;
#endif /*RT_CFG80211_P2P_SUPPORT*/

	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apidx];
    struct wifi_dev *pWdev = &pMbss->wdev;

	MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("%s =====> \n", __FUNCTION__));
	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;

	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40 || pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104)
	{
		pWdev->WepStatus = Ndis802_11WEPEnabled;
		{
				CIPHER_KEY	*pSharedKey;
				POS_COOKIE pObj;

				pObj = (POS_COOKIE) pAd->OS_Cookie;

				pSharedKey = &pAd->SharedKey[apidx][pKeyInfo->KeyId];
				os_move_mem(pSharedKey->Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);


				if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40)
					pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_WEP64;
	else
					pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_WEP128;

				AsicAddSharedKeyEntry(pAd, apidx, pKeyInfo->KeyId, pSharedKey);
	}
	}
	else if(pKeyInfo->KeyType == RT_CMD_80211_KEY_WPA)
	{

	if (pKeyInfo->cipher == Ndis802_11AESEnable)
	{
		/* AES */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        if (pKeyInfo->bPairwise == FALSE )
#else
        if (pKeyInfo->KeyId > 0)
#endif	/* LINUX_VERSION_CODE 2.6.37 */
		{
			if (pWdev->GroupKeyWepStatus == Ndis802_11Encryption3Enabled)
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CFG: Set AES Security Set. (GROUP) %d\n", pKeyInfo->KeyLen));
				pAd->SharedKey[apidx][pKeyInfo->KeyId].KeyLen= LEN_TK;
				os_move_mem(pAd->SharedKey[apidx][pKeyInfo->KeyId].Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);

				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_AES;

				AsicAddSharedKeyEntry(pAd, apidx, pKeyInfo->KeyId,
						&pAd->SharedKey[apidx][pKeyInfo->KeyId]);

				GET_GroupKey_WCID(pWdev, Wcid);
				RTMPSetWcidSecurityInfo(pAd, apidx, (UINT8)(pKeyInfo->KeyId),
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg, Wcid, SHAREDKEYTABLE);

#ifdef MT_MAC
                if (pAd->chipCap.hif_type == HIF_MT)
                        RTMP_ADDREMOVE_KEY(pAd, 0, apidx, 0, Wcid, SHAREDKEYTABLE,
                                        	&pAd->SharedKey[apidx][pKeyInfo->KeyId], BROADCAST_ADDR);
#endif /* MT_MAC */

			}
		}
		else
		{
			if (pKeyInfo->MAC)
				pEntry = MacTableLookup(pAd, pKeyInfo->MAC);

			if(pEntry)
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CFG: Set AES Security Set. (PAIRWISE) %d\n", pKeyInfo->KeyLen));
				pEntry->PairwiseKey.KeyLen = LEN_TK;
				NdisCopyMemory(&pEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyBuf, OFFSET_OF_PTK_TK);
				os_move_mem(pEntry->PairwiseKey.Key, &pEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyLen);
				pEntry->PairwiseKey.CipherAlg = CIPHER_AES;

				AsicAddPairwiseKeyEntry(pAd, (UCHAR)pEntry->Aid, &pEntry->PairwiseKey);
				RTMPSetWcidSecurityInfo(pAd, pEntry->apidx, (UINT8)(pKeyInfo->KeyId & 0x0fff),
				pEntry->PairwiseKey.CipherAlg, pEntry->Aid, PAIRWISEKEYTABLE);

#ifdef MT_MAC
                if (pAd->chipCap.hif_type == HIF_MT)
                    	RTMP_ADDREMOVE_KEY(pAd, 0, apidx, pKeyInfo->KeyId, pEntry->wcid, PAIRWISEKEYTABLE,
                                            &pEntry->PairwiseKey, pEntry->Addr);
#endif /* MT_MAC */
#ifdef RT_CFG80211_P2P_MULTI_CHAN_SUPPORT

				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: InfraCh=%d, pWdev->channel=%d\n", __FUNCTION__, pAd->MlmeAux.InfraChannel, pWdev->channel));
				if (INFRA_ON(pAd) &&
					( ((pAd->StaCfg[0].wdev.bw == pWdev->bw) && (pAd->StaCfg[0].wdev.channel != pWdev->channel ))
					||!((pAd->StaCfg[0].wdev.bw == pWdev->bw) && ((pAd->StaCfg[0].wdev.channel == pWdev->channel)))))
					{
					/*wait 1 s  DHCP  for P2P CLI */
					OS_WAIT(1000);
					MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("OS WAIT 1000 FOR DHCP\n"));
//					pAd->MCC_GOConnect_Protect = FALSE;
//					pAd->MCC_GOConnect_Count = 0;
					Start_MCC(pAd);
					MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("infra => GO test\n"));
				}
				else if((pAd->StaCfg[0].wdev.bw != pWdev->bw) && ((pAd->StaCfg[0].wdev.channel == pWdev->channel)))
				{
					MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("start bw !=  && SCC\n"));
					pAd->Mlme.bStartScc = TRUE;
				}
/*after p2p cli connect , neet to change to default configure*/
				//MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("iversontest pWdev->bw %d  \n",pWdev->bw));
				if (pWdev->bw == 0)
				{
					wdev->extcha = EXTCHA_BELOW;					
					HcUpdateExtCha(pAd,wdev->channel,wdev->extcha);
					pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
					pAd->CommonCfg.HT_Disable = 0;
					SetCommonHtVht(pAd,NULL);
				}

#endif /* RT_CFG80211_P2P_MULTI_CHAN_SUPPORT */


			}
			else
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("CFG: Set AES Security Set. (PAIRWISE) But pEntry NULL\n"));
			}
		}
		}
		else if (pKeyInfo->cipher == Ndis802_11TKIPEnable){
		/* TKIP */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        if (pKeyInfo->bPairwise == FALSE )
#else
        if (pKeyInfo->KeyId > 0)
#endif	/* LINUX_VERSION_CODE 2.6.37 */
		{
			if (pWdev->GroupKeyWepStatus == Ndis802_11Encryption2Enabled)
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CFG: Set TKIP Security Set. (GROUP) %d\n", pKeyInfo->KeyLen));
				pAd->SharedKey[apidx][pKeyInfo->KeyId].KeyLen= LEN_TK;
				os_move_mem(pAd->SharedKey[apidx][pKeyInfo->KeyId].Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);

				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_TKIP;

				AsicAddSharedKeyEntry(pAd, apidx, pKeyInfo->KeyId,
						&pAd->SharedKey[apidx][pKeyInfo->KeyId]);

				GET_GroupKey_WCID(pWdev, Wcid);
				RTMPSetWcidSecurityInfo(pAd, apidx, (UINT8)(pKeyInfo->KeyId),
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg, Wcid, SHAREDKEYTABLE);

#ifdef MT_MAC
                if (pAd->chipCap.hif_type == HIF_MT)
                        RTMP_ADDREMOVE_KEY(pAd, 0, apidx, pKeyInfo->KeyId, Wcid, SHAREDKEYTABLE,
                                        	&pAd->SharedKey[apidx][pKeyInfo->KeyId], BROADCAST_ADDR);
#endif /* MT_MAC */

		}
	}
		else
		{
			if (pKeyInfo->MAC)
				pEntry = MacTableLookup(pAd, pKeyInfo->MAC);

			if(pEntry)
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CFG: Set TKIP Security Set. (PAIRWISE) %d\n", pKeyInfo->KeyLen));
				pEntry->PairwiseKey.KeyLen = LEN_TK;
				NdisCopyMemory(&pEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyBuf, OFFSET_OF_PTK_TK);
				os_move_mem(pEntry->PairwiseKey.Key, &pEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyLen);
				pEntry->PairwiseKey.CipherAlg = CIPHER_TKIP;

				AsicAddPairwiseKeyEntry(pAd, (UCHAR)pEntry->Aid, &pEntry->PairwiseKey);
				RTMPSetWcidSecurityInfo(pAd, pEntry->apidx, (UINT8)(pKeyInfo->KeyId & 0x0fff), pEntry->PairwiseKey.CipherAlg, pEntry->Aid, PAIRWISEKEYTABLE);

#ifdef MT_MAC
                if (pAd->chipCap.hif_type == HIF_MT)
                    	RTMP_ADDREMOVE_KEY(pAd, 0, apidx, pKeyInfo->KeyId, pEntry->wcid, PAIRWISEKEYTABLE,
                                            &pEntry->PairwiseKey, pEntry->Addr);
#endif /* MT_MAC */

			}
			else
			{
				MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("CFG: Set TKIP Security Set. (PAIRWISE) But pEntry NULL\n"));
			}

		}
	}
	}
#endif /* CONFIG_AP_SUPPORT */
	return TRUE;

}

INT CFG80211_StaPortSecured(
	IN VOID                                         *pAdCB,
	IN UCHAR 					*pMac,
	IN UINT						flag)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	MAC_TABLE_ENTRY *pEntry;
	STA_TR_ENTRY *tr_entry;

	pEntry = MacTableLookup(pAd, pMac);
	if (!pEntry)
	{
		MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Can't find pEntry in CFG80211_StaPortSecured\n"));
	}
	else
	{
		tr_entry = &pAd->MacTab.tr_entry[pEntry->wcid];
		if (flag)
		{
			pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
			pEntry->WpaState = AS_PTKINITDONE;
			tr_entry->PortSecured = WPA_802_1X_PORT_SECURED;
			MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("AID:%d, PortSecured\n", pEntry->Aid));
		}
		else
		{
			pEntry->PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
			tr_entry->PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("AID:%d, PortNotSecured\n", pEntry->Aid));
		}
	}

	return 0;
}

INT CFG80211_ApStaDel(
	IN VOID                                         *pAdCB,
	IN UCHAR                                        *pMac)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	MAC_TABLE_ENTRY *pEntry;

	if (pMac == NULL)
	{
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
		/* From WCID=2 */
		if (INFRA_ON(pAd))
			;//P2PMacTableReset(pAd);
		else
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
			MacTableReset(pAd);
	}
	else
	{
		pEntry = MacTableLookup(pAd, pMac);
		if (pEntry)
		{
			MlmeDeAuthAction(pAd, pEntry, REASON_NO_LONGER_VALID, FALSE);
		}
		else
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Can't find pEntry in ApStaDel\n"));
	}
	return 0;
}

INT CFG80211_setApDefaultKey(
	IN VOID                    *pAdCB,
	IN UINT 					Data
)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	MTWF_LOG(DBG_CAT_SEC, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Set Ap Default Key: %d\n", Data));
#ifdef RT_CFG80211_P2P_SUPPORT
    	pAd->ApCfg.MBSSID[CFG_GO_BSSID_IDX].wdev.DefaultKeyId = Data;
#else
	pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev.DefaultKeyId = Data;
#endif /*RT_CFG80211_P2P_SUPPORT*/

	return 0;
}

INT CFG80211_ApStaDelSendEvent(PRTMP_ADAPTER pAd, const PUCHAR mac_addr)
{
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	PNET_DEV pNetDev = NULL;
	if ((pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList.size > 0) &&
		((pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, RT_CMD_80211_IFTYPE_P2P_GO)) != NULL))
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("CONCURRENT_DEVICE CFG : GO NOITFY THE CLIENT Disconnected\n"));
		CFG80211OS_DelSta(pNetDev, mac_addr);
	}
	else
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("SINGLE_DEVICE CFG : GO NOITFY THE CLIENT Disconnected\n"));
		CFG80211OS_DelSta(pAd->net_dev, mac_addr);
	}

	return 0;
}

#endif /* CONFIG_AP_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */
