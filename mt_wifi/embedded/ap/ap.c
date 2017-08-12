 /****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************

    Module Name:
    soft_ap.c

    Abstract:
    Access Point specific routines and MAC table maintenance routines

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    John Chang  08-04-2003    created for 11g soft-AP

 */

#include "rt_config.h"

#define MCAST_WCID_TO_REMOVE 0 //Pat: TODO

char const *pEventText[EVENT_MAX_EVENT_TYPE] = {
	"restart access point",
	"successfully associated",
	"has disassociated",
	"has been aged-out and disassociated" ,
	"active countermeasures",
	"has disassociated with invalid PSK password"};


UCHAR get_apidx_by_addr(RTMP_ADAPTER *pAd, UCHAR *addr)
{
	UCHAR apidx;

	for (apidx=0; apidx<pAd->ApCfg.BssidNum; apidx++)
	{
		if (RTMPEqualMemory(addr, pAd->ApCfg.MBSSID[apidx].wdev.bssid, MAC_ADDR_LEN))
			break;
	}

	return apidx;
}

static INT ap_mlme_set_capability(RTMP_ADAPTER *pAd, BSS_STRUCT *pMbss)
{
    struct wifi_dev *wdev = &pMbss->wdev;
    BOOLEAN SpectrumMgmt = FALSE;

#ifdef A_BAND_SUPPORT
    /* Decide the Capability information field */
    /* In IEEE Std 802.1h-2003, the spectrum management bit is enabled in the 5 GHz band */
    if ((wdev->channel > 14) && pAd->CommonCfg.bIEEE80211H == TRUE)
        SpectrumMgmt = TRUE;
#endif /* A_BAND_SUPPORT */

	pMbss->CapabilityInfo = CAP_GENERATE(1,
									0,
									IS_SECURITY_Entry(wdev),
									(pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong ? 0 : 1),
									pAd->CommonCfg.bUseShortSlotTime,
									SpectrumMgmt);

#ifdef DOT11K_RRM_SUPPORT
    if (pMbss->RrmCfg.bDot11kRRMEnable == TRUE)
        pMbss->CapabilityInfo |= RRM_CAP_BIT;
#endif /* DOT11K_RRM_SUPPORT */

    if (pMbss->wdev.bWmmCapable == TRUE)
    {
        /*
            In WMM spec v1.1, A WMM-only AP or STA does not set the "QoS"
            bit in the capability field of association, beacon and probe
            management frames.
        */
/*          pMbss->CapabilityInfo |= 0x0200; */
    }

#ifdef UAPSD_SUPPORT
    if (pMbss->wdev.UapsdInfo.bAPSDCapable == TRUE)
    {
        /*
            QAPs set the APSD subfield to 1 within the Capability
            Information field when the MIB attribute
            dot11APSDOptionImplemented is true and set it to 0 otherwise.
            STAs always set this subfield to 0.
        */
        pMbss->CapabilityInfo |= 0x0800;
    }
#endif /* UAPSD_SUPPORT */

    return TRUE;
}


static VOID ApAutoChannelAtBootUp(RTMP_ADAPTER *pAd)
{

    #define SINGLE_BAND 0
    #define DUAL_BAND   1
    struct wifi_dev *pwdev;
    INT i;
    UCHAR NewChannel;
    UCHAR AutoChannel2GDone = FALSE;
    UCHAR AutoChannel5GDone = FALSE;
    BOOLEAN IsAband;
    INT32 ret;

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s----------------->\n", __FUNCTION__));

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: AutoChannelBootup = %d, AutoChannelFlag = %d\n",
    __FUNCTION__, pAd->ApCfg.bAutoChannelAtBootup, pAd->AutoChSelCtrl.AutoChannelFlag));
    
    if (!pAd->ApCfg.bAutoChannelAtBootup || (pAd->AutoChSelCtrl.AutoChannelFlag == 0))
    {
       MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s<-----------------\n", __FUNCTION__));
       return;
    }
    
    /* Now Enable RxTx*/
    RTMPEnableRxTx(pAd);


    pAd->AutoChSelCtrl.p2GScanwdev = NULL;
    pAd->AutoChSelCtrl.p5GScanwdev = NULL;    

    for (i = 0; i < pAd->ApCfg.BssidNum; i++)
    {            
        pwdev = &pAd->ApCfg.MBSSID[i].wdev;
        
        if (pwdev->channel == 0)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
            ("%s: PhyMode: %d\n", __FUNCTION__, pwdev->PhyMode));
            
            if (WMODE_CAP_2G(pwdev->PhyMode) && AutoChannel2GDone == FALSE)
            {
                IsAband = FALSE;

                pAd->AutoChSelCtrl.p2GScanwdev = pwdev;
                
                pAd->AutoChSelCtrl.PhyMode = pwdev->PhyMode; 
                
                /* Now we can receive the beacon and do the listen beacon*/
                NewChannel = AP_AUTO_CH_SEL(pAd, pAd->ApCfg.AutoChannelAlg, IsAband);
                
                MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
                ("%s : Auto channel selection for 2G channel =%d\n", __FUNCTION__, NewChannel));
                
                ret = HcUpdateChannel(pAd,NewChannel);
                
                if(ret < 0 )
                {
                    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
                    ("%s(): Update Channel %d faild, not support this RF\n", __FUNCTION__, NewChannel));		                 
                }
                
                AutoChannel2GDone = TRUE; 
            }   
            else if (WMODE_CAP_5G(pwdev->PhyMode) && AutoChannel5GDone == FALSE)
            {      
                IsAband = TRUE;

                pAd->AutoChSelCtrl.p5GScanwdev = pwdev;

                pAd->AutoChSelCtrl.PhyMode = pwdev->PhyMode; 
                
                /* Now we can receive the beacon and do the listen beacon*/
                NewChannel = AP_AUTO_CH_SEL(pAd, pAd->ApCfg.AutoChannelAlg, IsAband);
                
                MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
                ("%s : Auto channel selection for 5G channel = %d\n", __FUNCTION__, NewChannel));
                
                ret = HcUpdateChannel(pAd,NewChannel);
                
                if(ret < 0 )
                {
                    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
                    ("%s(): Update Channel %d faild, not support this RF\n", __FUNCTION__, NewChannel));		                 
                } 
                
                AutoChannel5GDone = TRUE;                        
            }
        }
          
        if((pAd->CommonCfg.dbdc_mode == DUAL_BAND) && AutoChannel2GDone && AutoChannel5GDone)
        {
            break;
        }
        else if ((pAd->CommonCfg.dbdc_mode == SINGLE_BAND) && (AutoChannel2GDone || AutoChannel5GDone))
        {
            break;
        }
    }

    pAd->ApCfg.bAutoChannelAtBootup = FALSE;
    
    pAd->AutoChSelCtrl.AutoChannelFlag = 0;
    
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s<-----------------\n", __FUNCTION__));
}

/*
	==========================================================================
	Description:
		Initialize AP specific data especially the NDIS packet pool that's
		used for wireless client bridging.
	==========================================================================
 */
static VOID ApChannelCheckForRfIC(RTMP_ADAPTER *pAd,UCHAR RfIC)
{
	UCHAR Channel = HcGetChannelByRf(pAd,RfIC);
	UCHAR PhyMode = HcGetPhyModeByRf(pAd,RfIC);
	if(!HcIsRfSupport(pAd,RfIC))
	{
		return;
	}

#ifdef DOT11_N_SUPPORT
	/* If WMODE_CAP_N(phymode) and BW=40 check extension channel, after select channel	*/
	N_ChannelCheck(pAd,PhyMode,Channel);
#endif /*DOT11_N_SUPPORT*/
}


NDIS_STATUS APOneShotSettingInitialize(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
#ifdef BACKGROUND_SCAN_SUPPORT	
	UCHAR Channel = 0;
	INT32	ret=0;
#endif /* BACKGROUND_SCAN_SUPPORT */
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;
	/*AP Open*/
	WifiSysOpen(pAd,wdev);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("---> APOneShotSettingInitialize\n"));

	RTMPInitTimer(pAd, &pAd->ApCfg.CounterMeasureTimer, GET_TIMER_FUNCTION(CMTimerExec), pAd, FALSE);
#if defined (RTMP_MAC_USB) || defined(RTMP_MAC_SDIO)
	RTMPInitTimer(pAd, &pAd->CommonCfg.BeaconUpdateTimer, GET_TIMER_FUNCTION(BeaconUpdateExec), pAd, TRUE);
#endif /* RTMP_MAC_USB */

#ifdef IDS_SUPPORT
	/* Init intrusion detection timer */
	RTMPInitTimer(pAd, &pAd->ApCfg.IDSTimer, GET_TIMER_FUNCTION(RTMPIdsPeriodicExec), pAd, FALSE);
	pAd->ApCfg.IDSTimerRunning = FALSE;
#endif /* IDS_SUPPORT */


#ifdef WAPI_SUPPORT
	/* Init WAPI rekey timer */
	RTMPInitWapiRekeyTimerAction(pAd, NULL);
#endif /* WAPI_SUPPORT */

#ifdef IGMP_SNOOP_SUPPORT
	MulticastFilterTableInit(pAd, &pAd->pMulticastFilterTable);
#endif /* IGMP_SNOOP_SUPPORT */

#ifdef DOT11V_WNM_SUPPORT
	initList(&pAd->DMSEntryList);
#endif /* DOT11V_WNM_SUPPORT */

#ifdef FTM_SUPPORT
	FtmMgmtInit(pAd);
#endif /* FTM_SUPPORT */

#ifdef DOT11K_RRM_SUPPORT
	RRM_CfgInit(pAd);
#endif /* DOT11K_RRM_SUPPORT */

	/*update ext_cha to hdev*/
	HcUpdateExtCha(pAd,wdev->channel,wdev->extcha);

	BuildChannelList(pAd);

	if (CheckNonOccupancyChannel(pAd, wdev->channel) == FALSE) {
		HcUpdateChannel(pAd, FirstChannel(pAd));
	}

	/* Init BssTab & ChannelInfo tabbles for auto channel select.*/
	AutoChBssTableInit(pAd);
	ChannelInfoInit(pAd);

        ApAutoChannelAtBootUp(pAd);

#ifdef BACKGROUND_SCAN_SUPPORT	
	if (pAd->CommonCfg.dbdc_mode == TRUE) {
       		pAd->BgndScanCtrl.BgndScanSupport = 0;
		pAd->BgndScanCtrl.DfsZeroWaitSupport = 0;
	}        
#ifdef DOT11_VHT_AC	
	else if (pAd->CommonCfg.vht_bw == VHT_BW_160 || pAd->CommonCfg.vht_bw == VHT_BW_8080 ) {
		pAd->BgndScanCtrl.BgndScanSupport = 0;
		pAd->BgndScanCtrl.DfsZeroWaitSupport = 0;
	}    
#endif /* DOT11_VHT_AC */	                
	else {
                        pAd->BgndScanCtrl.BgndScanSupport = 1;
	}
	
	Channel  = HcGetChannelByRf(pAd, RFIC_5GHZ);
	MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Current Channel is %d. DfsZeroWaitSupport=%d\n", Channel, pAd->BgndScanCtrl.DfsZeroWaitSupport));
	if (Channel !=0 && pAd->BgndScanCtrl.DfsZeroWaitSupport && pAd->BgndScanCtrl.BgndScanSupport && RadarChannelCheck(pAd, Channel)) {

		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Channel %d is a DFS channel. Trigger Auto Channel selection!\n", Channel));
		pAd->BgndScanCtrl.DfsZeroWaitChannel = Channel; /* Record original channel for DFS zero wait */
		pAd->BgndScanCtrl.SkipDfsChannel = 1; //Notify AP_AUTO_CH_SEL skip DFS channel.
		/* Re-select a non-DFS channel. */
		Channel = AP_AUTO_CH_SEL(pAd, ChannelAlgBusyTime, TRUE);
		pAd->BgndScanCtrl.SkipDfsChannel = 0;//Clear.
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Get a new Channel %d for DFS zero wait (temporary) using!\n", Channel));	
		ret = HcUpdateChannel(pAd,Channel);
                        	if(ret < 0 )
                        	{
                               	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
                               	("%s(): Update Channel %d faild, not support this RF\n", __FUNCTION__, Channel));		                 
                        	}  
	}
#endif /* BACKGROUND_SCAN_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	/*per band checking*/
	ApChannelCheckForRfIC(pAd,RFIC_24GHZ);
	ApChannelCheckForRfIC(pAd,RFIC_5GHZ);

	RTMP_11N_D3_TimerInit(pAd);

#endif /* DOT11N_DRAFT3 */
#endif /*DOT11_N_SUPPORT*/


#ifdef RTMP_MAC_SDIO
    MTSDIOBssBeaconInit(pAd);
#endif /* RTMP_MAC_SDIO */

#ifdef AP_QLOAD_SUPPORT
    QBSS_LoadInit(pAd);
#endif /* AP_QLOAD_SUPPORT */

    /*
        Some modules init must be called before APStartUp().
        Or APStartUp() will make up beacon content and call
        other modules API to get some information to fill.
    */


#ifdef MAT_SUPPORT
    MATEngineInit(pAd);
#endif /* MAT_SUPPORT */

#ifdef CLIENT_WDS
    CliWds_ProxyTabInit(pAd);
#endif /* CLIENT_WDS */

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("<--- APOneShotSettingInitialize\n"));
	return Status;
}


/*
	==========================================================================
	Description:
		Shutdown AP and free AP specific resources
	==========================================================================
 */
VOID APShutdown(RTMP_ADAPTER *pAd)
{
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("---> APShutdown\n"));

#ifdef MT_MAC
	/*	Disable RX */
	//MtAsicSetMacTxRx(pAd, ASIC_MAC_RX, FALSE,0);

#ifdef RTMP_MAC_PCI
	APStop(pAd);
#endif /* RTMP_MAC_PCI */
	//MlmeRadioOff(pAd);
#else
		MlmeRadioOff(pAd);

#ifdef RTMP_MAC_PCI
		APStop(pAd);
#endif /* RTMP_MAC_PCI */
#endif

	/*remove sw related timer and table*/
	rtmp_ap_exit(pAd);

#ifdef FTM_SUPPORT
	FtmMgmtExit(pAd);
#endif /* FTM_SUPPORT */

#ifdef DOT11V_WNM_SUPPORT
	DMSTable_Release(pAd);
#endif /* DOT11V_WNM_SUPPORT */

	NdisFreeSpinLock(&pAd->MacTabLock);

#ifdef WDS_SUPPORT
	NdisFreeSpinLock(&pAd->WdsTab.WdsTabLock);
#endif /* WDS_SUPPORT */

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("<--- APShutdown\n"));
}


static INT ap_hw_tb_init(RTMP_ADAPTER *pAd)
{
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():Reset WCID Table\n", __FUNCTION__));
		HW_SET_DEL_ASIC_WCID(pAd, WCID_ALL);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():Reset Sec Table\n", __FUNCTION__));
	return TRUE;
}





static INT ap_phy_rrm_init_byRf(RTMP_ADAPTER *pAd, UCHAR RfIC)
{
	UCHAR PhyMode = HcGetPhyModeByRf(pAd,RfIC);
	UCHAR Channel = HcGetChannelByRf(pAd,RfIC);
	UCHAR BandIdx = HcGetBandByChannel(pAd,Channel);

    if (Channel== 0 )
	{
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%sBandIdx %d Channel setting is 0\n", __FUNCTION__,BandIdx));

        return FALSE;
		}

	pAd->CommonCfg.CentralChannel = Channel;

	AsicSetTxStream(pAd, pAd->Antenna.field.TxPath, OPMODE_AP, TRUE,BandIdx);
	AsicSetRxStream(pAd, pAd->Antenna.field.RxPath, BandIdx);

		// TODO: shiang-usw, get from MT7620_MT7610 Single driver, check this!!
	N_ChannelCheck(pAd,PhyMode,Channel);//correct central channel offset
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("pAd->CommonCfg.CentralChannel=%d\n",pAd->CommonCfg.CentralChannel));
	AsicBBPAdjust(pAd,Channel);
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("pAd->CommonCfg.CentralChannel=%d\n",pAd->CommonCfg.CentralChannel));


#ifdef DOT11_VHT_AC

		if ((pAd->CommonCfg.BBPCurrentBW == BW_80) || (pAd->CommonCfg.BBPCurrentBW == BW_160) || (pAd->CommonCfg.BBPCurrentBW == BW_8080))
			pAd->hw_cfg.cent_ch = pAd->CommonCfg.vht_cent_ch;
		else
#endif /* DOT11_VHT_AC */
			pAd->hw_cfg.cent_ch = pAd->CommonCfg.CentralChannel;


		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("pAd->CommonCfg.CentralChannel=%d\n",pAd->CommonCfg.CentralChannel));
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("pAd->hw_cfg.cent_ch=%d,BW=%d\n",pAd->hw_cfg.cent_ch,pAd->CommonCfg.BBPCurrentBW));

		AsicSwitchChannel(pAd, pAd->hw_cfg.cent_ch, FALSE);
		AsicLockChannel(pAd, pAd->hw_cfg.cent_ch);

#ifdef DOT11_VHT_AC
	//+++Add by shiang for debug
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): AP Set CentralFreq at %d(Prim=%d, HT-CentCh=%d, VHT-CentCh=%d, BBP_BW=%d)\n",
						__FUNCTION__, pAd->hw_cfg.cent_ch, Channel,
							pAd->CommonCfg.CentralChannel, pAd->CommonCfg.vht_cent_ch,
							pAd->CommonCfg.BBPCurrentBW));
	//---Add by shiang for debug
#endif /* DOT11_VHT_AC */
	return TRUE;
}

/*
	==========================================================================
	Description:
		Start AP service. If any vital AP parameter is changed, a STOP-START
		sequence is required to disassociate all STAs.

	IRQL = DISPATCH_LEVEL.(from SetInformationHandler)
	IRQL = PASSIVE_LEVEL. (from InitializeHandler)

	Note:
		Can't call NdisMIndicateStatus on this routine.

		RT61 is a serialized driver on Win2KXP and is a deserialized on Win9X
		Serialized callers of NdisMIndicateStatus must run at IRQL = DISPATCH_LEVEL.

	==========================================================================
 */

VOID APStartUpForMbss(RTMP_ADAPTER *pAd,BSS_STRUCT *pMbss)
{
	struct wifi_dev *wdev = &pMbss->wdev;
	BOOLEAN bWmmCapable = FALSE;
	EDCA_PARM *pEdca = NULL;
	UCHAR phy_mode = pAd->CommonCfg.cfg_wmode;

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("===> %s(), CfgMode:%d\n", __FUNCTION__, phy_mode));

	/*Ssid length sanity check.*/
	if ((pMbss->SsidLen <= 0) || (pMbss->SsidLen > MAX_LEN_OF_SSID))
	{
	    NdisMoveMemory(pMbss->Ssid, "HT_AP", 5);
	    pMbss->Ssid[5] = '0' + pMbss->mbss_idx;
	    pMbss->SsidLen = 6;
	}

	if (wdev->func_idx == 0)
	{
	   MgmtTableSetMcastEntry(pAd, MCAST_WCID_TO_REMOVE);
	}

	APSecInit(pAd, wdev);
	
        ap_mlme_set_capability(pAd, pMbss);

#ifdef WSC_V2_SUPPORT
        if (pMbss->WscControl.WscV2Info.bEnableWpsV2)
        {
            /*
                WPS V2 doesn't support Chiper WEP and TKIP.
            */
		struct _SECURITY_CONFIG *pSecConfig = &wdev->SecConfig;
		if (IS_CIPHER_WEP_TKIP_ONLY(pSecConfig->PairwiseCipher)
			|| (pMbss->bHideSsid))
			WscOnOff(pAd, wdev->func_idx, TRUE);
		else
			WscOnOff(pAd, wdev->func_idx, FALSE);
        }
#endif /* WSC_V2_SUPPORT */

	/* If any BSS is WMM Capable, we need to config HW CRs */
	if (wdev->bWmmCapable)
	{
   	 	bWmmCapable = TRUE;
	}

	if (WMODE_CAP_N(wdev->PhyMode) || bWmmCapable)
	{
		pEdca = &pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx];
		/* EDCA parameters used for AP's own transmission */
		if (pEdca->bValid == FALSE)
			set_default_ap_edca_param(pEdca);

		/* EDCA parameters to be annouced in outgoing BEACON, used by WMM STA */
		if (pAd->ApCfg.BssEdcaParm.bValid == FALSE)
			set_default_sta_edca_param(&pAd->ApCfg.BssEdcaParm);
	}

#ifdef DOT11_N_SUPPORT
	RTMPSetPhyMode(pAd, wdev->PhyMode);
	/*update rate info for wdev*/
	RTMPUpdateRateInfo(wdev->PhyMode,&wdev->rate);

	if (!WMODE_CAP_N(wdev->PhyMode))
	{
		pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth = BW_20; /* Patch UI */
	}

#ifdef DOT11N_DRAFT3
    /*
        We only do this Overlapping BSS Scan when system up, for the
        other situation of channel changing, we depends on station's
        report to adjust ourself.
    */

    if (pAd->CommonCfg.bForty_Mhz_Intolerant == TRUE)
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Disable 20/40 BSSCoex Channel Scan(BssCoex=%d, 40MHzIntolerant=%d)\n",
                                    pAd->CommonCfg.bBssCoexEnable,
                                    pAd->CommonCfg.bForty_Mhz_Intolerant));
    }
    else if(pAd->CommonCfg.bBssCoexEnable == TRUE)
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Enable 20/40 BSSCoex Channel Scan(BssCoex=%d)\n",
                    pAd->CommonCfg.bBssCoexEnable));
        APOverlappingBSSScan(pAd,wdev);
    }
#endif /* DOT11N_DRAFT3 */

    if (pAd->CommonCfg.bRdg) {
        AsicSetRDG(pAd, WCID_ALL, 0, 0, 0);
        if (pAd->CommonCfg.dbdc_mode) {
            AsicSetRDG(pAd, WCID_ALL, 1, 0, 0);
        }
    }

#ifdef GREENAP_SUPPORT
	if (pAd->ApCfg.bGreenAPEnable == TRUE)
	{
		RTMP_CHIP_ENABLE_AP_MIMOPS(pAd,TRUE,wdev);
		pAd->ApCfg.GreenAPLevel=GREENAP_WITHOUT_ANY_STAS_CONNECT;
	}
#endif /* GREENAP_SUPPORT */
#endif /*DOT11_N_SUPPORT*/

	MlmeUpdateTxRates(pAd, FALSE, wdev->func_idx);
#ifdef DOT11_N_SUPPORT
	if (WMODE_CAP_N(wdev->PhyMode))
	{
		MlmeUpdateHtTxRates(pAd, wdev->func_idx);
	}
#endif /* DOT11_N_SUPPORT */

	//update per wdev bw
	wdev->bw = wdev->MaxHTPhyMode.field.BW;

	WifiSysApLinkUp(pAd,wdev);

	APKeyTableInit(pAd, wdev);

    RadarStateCheck(pAd);

#if defined(MT7615)
    /*==============================================================================*/
    /* enable/disable SKU by profile  */
    TxPowerSKUCtrl(pAd, pAd->CommonCfg.SKUenable);

    /* enable/disable Power Percentage by profile */
    TxPowerPercentCtrl(pAd, pAd->CommonCfg.PERCENTAGEenable);
    /*==============================================================================*/
#endif /* defined(MT7615) */    
	
	if(pMbss->wdev.channel > 14)
		ap_phy_rrm_init_byRf(pAd,RFIC_5GHZ);
	else
		ap_phy_rrm_init_byRf(pAd,RFIC_24GHZ);

#ifdef MT_DFS_SUPPORT	
		DfsCacNormalStart(pAd); 		
		WrapDfsRadarDetectStart(pAd);
#endif

}

// TODO: for run time usage should remove it
VOID APStartUp(RTMP_ADAPTER *pAd)
{
	UINT32 idx;
	BSS_STRUCT *pMbss = NULL;
#ifdef DFS_SUPPORT
	UCHAR Channel5G = HcGetChannelByRf(pAd,RFIC_5GHZ);
#endif /*DFS_SUPPORT*/

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("===> %s()\n", __FUNCTION__));

#ifdef INF_AMAZON_SE
	for (idx=0;idx<NUM_OF_TX_RING;idx++)
		pAd->BulkOutDataSizeLimit[idx]=24576;
#endif /* INF_AMAZON_SE */

	AsicDisableSync(pAd, HW_BSSID_0);//don't gen beacon, reset tsf timer, don't gen interrupt.

	/*reset wtbl*/
	ap_hw_tb_init(pAd);
	
	for (idx = 0; idx < pAd->ApCfg.BssidNum; idx++)
	{
		pMbss = &pAd->ApCfg.MBSSID[idx];
		pMbss->mbss_idx = idx;
		APStartUpForMbss(pAd, pMbss);
	}

#ifdef DOT11_N_SUPPORT
	AsicSetRalinkBurstMode(pAd, pAd->CommonCfg.bRalinkBurstMode);

#ifdef PIGGYBACK_SUPPORT
	AsicSetPiggyBack(pAd, pAd->CommonCfg.bPiggyBackCapable);
#endif /* PIGGYBACK_SUPPORT */
#endif /* DOT11_N_SUPPORT */

#if defined(RTMP_MAC) || defined(RLT_MAC)
#ifdef FIFO_EXT_SUPPORT
	if ((pAd->chipCap.hif_type == HIF_RTMP)
            || (pAd->chipCap.hif_type == HIF_RLT))
		RtAsicFifoExtSet(pAd);
#endif /* FIFO_EXT_SUPPORT */
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */

	/* Workaround start: Let Rx packet can be dequeued from PSE or Tx CMD will fail */
	/* Workaround end */

    /* setup tx preamble */
	MlmeSetTxPreamble(pAd, (USHORT)pAd->CommonCfg.TxPreamble);

	/* Clear BG-Protection flag */
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);

    /* default NO_PROTECTION */
    AsicUpdateProtect(pAd, 0, (ALLN_SETPROTECT|CCKSETPROTECT|OFDMSETPROTECT), TRUE, FALSE);

    /* Update ERP */
    APUpdateCapabilityAndErpIe(pAd);

#ifdef DOT11_N_SUPPORT
    /* Update HT & GF Protect */
    APUpdateOperationMode(pAd, &pMbss->wdev);
#endif /* DOT11_N_SUPPORT */

	/* Set the RadarDetect Mode as Normal, bc the APUpdateAllBeaconFram() will refer this parameter. */
	//pAd->Dot11_H.RDMode = RD_NORMAL_MODE;

#ifdef LED_CONTROL_SUPPORT
	RTMPSetLED(pAd, LED_LINK_UP);
#endif /* LED_CONTROL_SUPPORT */

#if defined (CONFIG_WIFI_PKT_FWD)
	WifiFwdSet(pAd->CommonCfg.WfFwdDisabled);
#endif /* CONFIG_WIFI_PKT_FWD */

	ApLogEvent(pAd, pAd->CurrentAddress, EVENT_RESET_ACCESS_POINT);
	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;
	pAd->MacTab.MsduLifeTime = 5; /* default 5 seconds */

	OPSTATUS_SET_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

	RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);


	/*
		NOTE!!!:
			All timer setting shall be set after following flag be cleared
				fRTMP_ADAPTER_HALT_IN_PROGRESS
	*/
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);

    for (idx = 0; idx < pAd->ApCfg.BssidNum; idx++)
    {
        pMbss = &pAd->ApCfg.MBSSID[idx];
        APKeyTableInit(pAd,  &pMbss->wdev);
    }

	//RadarStateCheck(pAd);



#ifdef RTMP_MAC_SDIO
	MTSDIOBssBeaconInit(pAd);
#endif /* RTMP_MAC_SDIO */

	/* start sending BEACON out */
    	UpdateBeaconHandler(
        pAd,
        NULL,
        AP_RENEW);

#ifdef DFS_SUPPORT
	if (IS_DOT11_H_RADAR_STATE(pAd, RD_SILENCE_MODE,Channel5G))
		NewRadarDetectionStart(pAd);
#endif /* DFS_SUPPORT */

#ifdef CARRIER_DETECTION_SUPPORT
	if (pAd->CommonCfg.CarrierDetect.Enable == TRUE)
		CarrierDetectionStart(pAd);
#endif /* CARRIER_DETECTION_SUPPORT */

	if (pAd->Dot11_H.RDMode == RD_NORMAL_MODE)
	{
        AsicSetSyncModeAndEnable(pAd, pAd->CommonCfg.BeaconPeriod, HW_BSSID_0, OPMODE_AP);
	}

	/* Pre-tbtt interrupt setting. */
	AsicSetPreTbtt(pAd, TRUE, HW_BSSID_0);

#ifdef WAPI_SUPPORT
	RTMPStartWapiRekeyTimerAction(pAd, NULL);
#endif /* WAPI_SUPPORT */

	/*
		Set group re-key timer if necessary.
		It must be processed after clear flag "fRTMP_ADAPTER_HALT_IN_PROGRESS"
	*/
	WPAGroupRekeyAction(pAd);

#ifdef WDS_SUPPORT
	/* Add wds key infomation to ASIC */
	AsicUpdateWdsRxWCIDTable(pAd);
#endif /* WDS_SUPPORT */

#ifdef IDS_SUPPORT
	/* Start IDS timer */
	if (pAd->ApCfg.IdsEnable)
	{
#ifdef SYSTEM_LOG_SUPPORT
		if (pAd->CommonCfg.bWirelessEvent == FALSE)
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("!!! WARNING !!! The WirelessEvent parameter doesn't be enabled \n"));
#endif /* SYSTEM_LOG_SUPPORT */

		RTMPIdsStart(pAd);
	}
#endif /* IDS_SUPPORT */



#ifdef DOT11R_FT_SUPPORT
	FT_Init(pAd);
#endif /* DOT11R_FT_SUPPORT */



	RTMP_ASIC_INTERRUPT_ENABLE(pAd);

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_SYSEM_READY);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Main bssid = %02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(pAd->ApCfg.MBSSID[BSS0].wdev.bssid)));

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("<=== %s()\n", __FUNCTION__));

}


/*Only first time will run it*/
VOID APStartUpForMain(RTMP_ADAPTER *pAd)
{
	BSS_STRUCT *pMbss = NULL;

#if defined(INF_AMAZON_SE) || defined(RTMP_MAC_USB)
	UINT32 idx;
#endif /*INF_AMAZON_SE|| RTMP_MAC_USB*/

#ifdef DFS_SUPPORT
	UCHAR Channel5G = HcGetChannelByRf(pAd,RFIC_5GHZ);
#endif /*DFS_SUPPORT*/

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("===> %s()\n", __FUNCTION__));

#ifdef INF_AMAZON_SE
	for (idx=0;idx<NUM_OF_TX_RING;idx++)
	{
		pAd->BulkOutDataSizeLimit[idx]=24576;
	}
#endif /* INF_AMAZON_SE */

	AsicDisableSync(pAd, HW_BSSID_0);//don't gen beacon, reset tsf timer, don't gen interrupt.

	/*reset hw wtbl*/
	ap_hw_tb_init(pAd);

	pMbss = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
	pMbss->mbss_idx = MAIN_MBSSID;

	/*update main runtime attribute*/
	APStartUpForMbss(pAd, pMbss);

#ifdef DOT11_N_SUPPORT
	AsicSetRalinkBurstMode(pAd, pAd->CommonCfg.bRalinkBurstMode);

#ifdef PIGGYBACK_SUPPORT
	AsicSetPiggyBack(pAd, pAd->CommonCfg.bPiggyBackCapable);
#endif /* PIGGYBACK_SUPPORT */
#endif /* DOT11_N_SUPPORT */

#if defined(RTMP_MAC) || defined(RLT_MAC)
#ifdef FIFO_EXT_SUPPORT
	if ((pAd->chipCap.hif_type == HIF_RTMP) || (pAd->chipCap.hif_type == HIF_RLT))
		RtAsicFifoExtSet(pAd);
#endif /* FIFO_EXT_SUPPORT */
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */

	/* Workaround start: Let Rx packet can be dequeued from PSE or Tx CMD will fail */
	/* Workaround end */

	/* Clear BG-Protection flag */
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);
	MlmeSetTxPreamble(pAd, (USHORT)pAd->CommonCfg.TxPreamble);


	/* Set the RadarDetect Mode as Normal, bc the APUpdateAllBeaconFram() will refer this parameter. */
	//pAd->Dot11_H.RDMode = RD_NORMAL_MODE;

	/* Disable Protection first. */
	AsicUpdateProtect(pAd, 0, (ALLN_SETPROTECT|CCKSETPROTECT|OFDMSETPROTECT), TRUE, FALSE);

	APUpdateCapabilityAndErpIe(pAd);
#ifdef DOT11_N_SUPPORT
	APUpdateOperationMode(pAd, &pMbss->wdev);
#endif /* DOT11_N_SUPPORT */

#ifdef LED_CONTROL_SUPPORT
	RTMPSetLED(pAd, LED_LINK_UP);
#endif /* LED_CONTROL_SUPPORT */

#if defined (CONFIG_WIFI_PKT_FWD)
	WifiFwdSet(pAd->CommonCfg.WfFwdDisabled);
#endif /* CONFIG_WIFI_PKT_FWD */

	ApLogEvent(pAd, pAd->CurrentAddress, EVENT_RESET_ACCESS_POINT);
	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;
	pAd->MacTab.MsduLifeTime = 5; /* default 5 seconds */

	OPSTATUS_SET_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

	RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);


	/*
		NOTE!!!:
			All timer setting shall be set after following flag be cleared
				fRTMP_ADAPTER_HALT_IN_PROGRESS
	*/
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);

	APKeyTableInit(pAd, &pMbss->wdev);

	//RadarStateCheck(pAd);



#ifdef RTMP_MAC_SDIO
	MTSDIOBssBeaconInit(pAd);
#endif /* RTMP_MAC_SDIO */

	/* start sending BEACON out */
    	UpdateBeaconHandler(
        pAd,
        NULL,
        AP_RENEW);

#ifdef DFS_SUPPORT
	if (Channel5G > 0 && IS_DOT11_H_RADAR_STATE(pAd, RD_SILENCE_MODE,Channel5G))
	{
		NewRadarDetectionStart(pAd);
	}
#endif /* DFS_SUPPORT */

#ifdef CARRIER_DETECTION_SUPPORT
	if (pAd->CommonCfg.CarrierDetect.Enable == TRUE)
		CarrierDetectionStart(pAd);
#endif /* CARRIER_DETECTION_SUPPORT */

	if (pAd->Dot11_H.RDMode == RD_NORMAL_MODE)
	{
        	AsicSetSyncModeAndEnable(pAd, pAd->CommonCfg.BeaconPeriod, HW_BSSID_0, OPMODE_AP);
	}

	/* Pre-tbtt interrupt setting. */
	AsicSetPreTbtt(pAd, TRUE, HW_BSSID_0);

#ifdef WAPI_SUPPORT
	RTMPStartWapiRekeyTimerAction(pAd, NULL);
#endif /* WAPI_SUPPORT */

	/*
		Set group re-key timer if necessary.
		It must be processed after clear flag "fRTMP_ADAPTER_HALT_IN_PROGRESS"
	*/
	WPAGroupRekeyAction(pAd);

#ifdef WDS_SUPPORT
	if (pAd->WdsTab.flg_wds_init)
	{
		/* Add wds key infomation to ASIC */
		AsicUpdateWdsRxWCIDTable(pAd);
	}
#endif /* WDS_SUPPORT */

#ifdef IDS_SUPPORT
	/* Start IDS timer */
	if (pAd->ApCfg.IdsEnable)
	{
#ifdef SYSTEM_LOG_SUPPORT
		if (pAd->CommonCfg.bWirelessEvent == FALSE)
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("!!! WARNING !!! The WirelessEvent parameter doesn't be enabled \n"));
#endif /* SYSTEM_LOG_SUPPORT */

		RTMPIdsStart(pAd);
	}
#endif /* IDS_SUPPORT */



#ifdef DOT11R_FT_SUPPORT
	FT_Init(pAd);
#endif /* DOT11R_FT_SUPPORT */


#ifdef BAND_STEERING
	if (pAd->ApCfg.BandSteering)
		BndStrg_Init(pAd);
#endif /* BAND_STEERING */


	RTMP_ASIC_INTERRUPT_ENABLE(pAd);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Main bssid = %02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(pAd->ApCfg.MBSSID[BSS0].wdev.bssid)));

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("<=== %s()\n", __FUNCTION__));

}


/*
	==========================================================================
	Description:
		disassociate all STAs and stop AP service.
	Note:
	==========================================================================
 */
VOID APStop(RTMP_ADAPTER *pAd)
{
	BOOLEAN Cancelled;
	INT idx;
	BSS_STRUCT *pMbss;

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("!!! APStop !!!\n"));

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_SYSEM_READY);

#ifdef DFS_SUPPORT
		NewRadarDetectionStop(pAd);
#endif /* DFS_SUPPORT */
#ifdef MT_DFS_SUPPORT //Jelly20150217
		WrapDfsRadarDetectStop(pAd);
#endif

#ifdef CONFIG_AP_SUPPORT
#ifdef CARRIER_DETECTION_SUPPORT
		if (pAd->CommonCfg.CarrierDetect.Enable == TRUE)
		{
			/* make sure CarrierDetect wont send CTS */
			CarrierDetectionStop(pAd);
		}
#endif /* CARRIER_DETECTION_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */


#ifdef WDS_SUPPORT
	WdsDown(pAd);
#endif /* WDS_SUPPORT */

#ifdef APCLI_SUPPORT
	ApCliIfDown(pAd);
#endif /* APCLI_SUPPORT */

	MacTableReset(pAd);
	CMDHandler(pAd);

	/* Disable pre-tbtt interrupt */
	AsicSetPreTbtt(pAd, FALSE, HW_BSSID_0);

	/* Disable piggyback */
	AsicSetPiggyBack(pAd, FALSE);

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		AsicDisableSync(pAd, HW_BSSID_0);

#ifdef LED_CONTROL_SUPPORT
		/* Set LED */
		RTMPSetLED(pAd, LED_LINK_DOWN);
#endif /* LED_CONTROL_SUPPORT */
	}


#ifdef RTMP_MAC_SDIO
	MTSDIOBssBeaconExit(pAd);
#endif /* RTMP_MAC_SDIO */


	for (idx = 0; idx < pAd->ApCfg.BssidNum; idx++)
	{
		pMbss = &pAd->ApCfg.MBSSID[idx];
		RTMPCancelTimer(&pAd->ApCfg.MBSSID[idx].wdev.SecConfig.GroupRekeyTimer, &Cancelled);
		RTMPReleaseTimer(&pAd->ApCfg.MBSSID[idx].wdev.SecConfig.GroupRekeyTimer, &Cancelled);

	        /* clear protection to default */
	        pMbss->wdev.protection = 0;
	        WifiSysApLinkDown(pAd, &pMbss->wdev);
	}

	if (pAd->ApCfg.CMTimerRunning == TRUE)
	{
		RTMPCancelTimer(&pAd->ApCfg.CounterMeasureTimer, &Cancelled);
		pAd->ApCfg.CMTimerRunning = FALSE;
		pAd->ApCfg.BANClass3Data = FALSE;
	}

#ifdef WAPI_SUPPORT
	RTMPCancelWapiRekeyTimerAction(pAd, NULL);
#endif /* WAPI_SUPPORT */

	/* */
	/* Cancel the Timer, to make sure the timer was not queued. */
	/* */
	OPSTATUS_CLEAR_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);
	RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);

#ifdef IDS_SUPPORT
	/* if necessary, cancel IDS timer */
	RTMPIdsStop(pAd);
#endif /* IDS_SUPPORT */

#ifdef DOT11R_FT_SUPPORT
	FT_Release(pAd);
#endif /* DOT11R_FT_SUPPORT */

#ifdef DOT11V_WNM_SUPPORT
	DMSTable_Release(pAd);
#endif /* DOT11V_WNM_SUPPORT */

}

/*
	==========================================================================
	Description:
		This routine is used to clean up a specified power-saving queue. It's
		used whenever a wireless client is deleted.
	==========================================================================
 */
VOID APCleanupPsQueue(RTMP_ADAPTER *pAd, QUEUE_HEADER *pQueue)
{
	PQUEUE_ENTRY pEntry;
	PNDIS_PACKET pPacket;

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s(): (0x%08lx)...\n", __FUNCTION__, (ULONG)pQueue));

	while (pQueue->Head)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():%u...\n", __FUNCTION__, pQueue->Number));

		pEntry = RemoveHeadQueue(pQueue);
		/*pPacket = CONTAINING_RECORD(pEntry, NDIS_PACKET, MiniportReservedEx); */
		pPacket = QUEUE_ENTRY_TO_PACKET(pEntry);
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
	}
}

/*
	==========================================================================
	Description:
		This routine is called by APMlmePeriodicExec() every second to check if
		1. any associated client in PSM. If yes, then TX MCAST/BCAST should be
		   out in DTIM only
		2. any client being idle for too long and should be aged-out from MAC table
		3. garbage collect PSQ
	==========================================================================
*/
VOID MacTableMaintenance(RTMP_ADAPTER *pAd)
{
	int wcid, startWcid;
#ifdef DOT11_N_SUPPORT
	ULONG MinimumAMPDUSize = pAd->CommonCfg.DesiredHtPhy.MaxRAmpduFactor; /*Default set minimum AMPDU Size to 2, i.e. 32K */
	BOOLEAN	bRdgActive = FALSE;
	BOOLEAN bRalinkBurstMode;
#endif /* DOT11_N_SUPPORT */
#ifdef RTMP_MAC_PCI
	unsigned long	IrqFlags;
#endif /* RTMP_MAC_PCI */
	MAC_TABLE *pMacTable;
#if defined(PRE_ANT_SWITCH) || defined(CFO_TRACK)
	int lastClient=0;
#endif /* defined(PRE_ANT_SWITCH) || defined(CFO_TRACK) */
	CHAR avgRssi;
	BSS_STRUCT *pMbss;
#ifdef WFA_VHT_PF
	RSSI_SAMPLE *worst_rssi = NULL;
	int worst_rssi_sta_idx = 0;
#endif /* WFA_VHT_PF */
#ifdef MT_MAC
	BOOLEAN bPreAnyStationInPsm = FALSE;
#endif /* MT_MAC */
#ifdef SMART_CARRIER_SENSE_SUPPORT
        UINT    BandIdx=0;
	CHAR	tmpRssi=0;
#endif /* SMART_CARRIER_SENSE_SUPPORT */
#ifdef APCLI_SUPPORT
	ULONG	apcli_avg_tx = 0;
	ULONG	apcli_avg_rx = 0;
	struct wifi_dev * apcli_wdev = NULL;
#endif /* APCLI_SUPPORT */
	struct wifi_dev * sta_wdev = NULL;
	struct wifi_dev * txop_wdev = NULL; 
	UCHAR    sta_hit_2g_infra_case_number =0;

	pMacTable = &pAd->MacTab;

#ifdef MT_MAC
	bPreAnyStationInPsm = pMacTable->fAnyStationInPsm;
#endif /* MT_MAC */

	pMacTable->fAnyStationInPsm = FALSE;
	pMacTable->fAnyStationBadAtheros = FALSE;
	pMacTable->fAnyTxOPForceDisable = FALSE;
	pMacTable->fAllStationAsRalink[0] = TRUE;
	pMacTable->fAllStationAsRalink[1] = TRUE;
	pMacTable->fCurrentStaBw40 = FALSE;
#ifdef DOT11_N_SUPPORT
	pMacTable->fAnyStationNonGF = FALSE;
	pMacTable->fAnyStation20Only = FALSE;
	pMacTable->fAnyStationIsLegacy = FALSE;
	pMacTable->fAnyStationMIMOPSDynamic = FALSE;
#ifdef GREENAP_SUPPORT
	/*Support Green AP */
	pMacTable->fAnyStationIsHT=FALSE;
#endif /* GREENAP_SUPPORT */

#ifdef DOT11N_DRAFT3
	pMacTable->fAnyStaFortyIntolerant = FALSE;
#endif /* DOT11N_DRAFT3 */
	pMacTable->fAllStationGainGoodMCS = TRUE;
#endif /* DOT11_N_SUPPORT */

#ifdef WAPI_SUPPORT
	pMacTable->fAnyWapiStation = FALSE;
#endif /* WAPI_SUPPORT */

	startWcid = 1;

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	/* Skip the Infra Side */
	startWcid = 2;
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

#ifdef SMART_CARRIER_SENSE_SUPPORT
        for (BandIdx=0; BandIdx< DBDC_BAND_NUM; BandIdx++)
        {
	    pAd->SCSCtrl.SCSMinRssi[BandIdx] =  0; /* (Reset)The minimum RSSI of STA */
	    pAd->SCSCtrl.OneSecRxByteCount[BandIdx] = 0;
	    pAd->SCSCtrl.OneSecTxByteCount[BandIdx] = 0;
   
        }
#endif /* SMART_CARRIER_SENSE_SUPPORT */

    /*TODO: Carter, modification start Wcid, Aid shall not simply equal to WCID.*/
	for (wcid = startWcid; VALID_UCAST_ENTRY_WCID(pAd, wcid); wcid++)
	{
		MAC_TABLE_ENTRY *pEntry = &pMacTable->Content[wcid];
		STA_TR_ENTRY *tr_entry = &pMacTable->tr_entry[wcid];
		BOOLEAN bDisconnectSta = FALSE;
		
#ifdef SMART_CARRIER_SENSE_SUPPORT
		if (IS_ENTRY_CLIENT(pEntry) || IS_ENTRY_APCLI(pEntry) || IS_ENTRY_REPEATER(pEntry)) {
			BandIdx = HcGetBandByWdev(pEntry->wdev);

			pAd->SCSCtrl.OneSecRxByteCount[BandIdx] += pEntry->OneSecRxBytes;
			pAd->SCSCtrl.OneSecTxByteCount[BandIdx] += pEntry->OneSecTxBytes;
			if (pAd->SCSCtrl.SCSEnable[BandIdx] == SCS_ENABLE) {
				tmpRssi = RTMPMinRssi(pAd, pEntry->RssiSample.AvgRssi[0], pEntry->RssiSample.AvgRssi[1], 
	                                                pEntry->RssiSample.AvgRssi[2], pEntry->RssiSample.AvgRssi[3]);
				if (tmpRssi <pAd->SCSCtrl.SCSMinRssi[BandIdx] )
					pAd->SCSCtrl.SCSMinRssi[BandIdx] = tmpRssi;
			}
		}	
#endif /* SMART_CARRIER_SENSE_SUPPORT */	

		pEntry->AvgTxBytes = (pEntry->AvgTxBytes == 0) ? \
							pEntry->OneSecTxBytes : \
							((pEntry->AvgTxBytes + pEntry->OneSecTxBytes) >> 1);
		pEntry->OneSecTxBytes = 0;

		pEntry->AvgRxBytes = (pEntry->AvgRxBytes == 0) ? \
							pEntry->OneSecRxBytes : \
							((pEntry->AvgRxBytes + pEntry->OneSecRxBytes) >> 1);
		pEntry->OneSecRxBytes = 0;
		
#ifdef APCLI_SUPPORT
	if((IS_ENTRY_APCLI(pEntry) || IS_ENTRY_REPEATER(pEntry))
            && (tr_entry->PortSecured == WPA_802_1X_PORT_SECURED)
        )
		{
#ifdef MAC_REPEATER_SUPPORT
			if (pEntry->bReptCli)
			{
				pEntry->ReptCliIdleCount++;

				if ((pEntry->bReptEthCli) 
					&& (pEntry->ReptCliIdleCount >= MAC_TABLE_AGEOUT_TIME)
					&& (pEntry->bReptEthBridgeCli == FALSE)) /* Do NOT ageout br0 link. @2016/1/27 */
				{
					REPEATER_CLIENT_ENTRY *pReptCliEntry = NULL;
					pReptCliEntry = &pAd->ApCfg.pRepeaterCliPool[pEntry->MatchReptCliIdx];
					if (pReptCliEntry) {
						pReptCliEntry->Disconnect_Sub_Reason = APCLI_DISCONNECT_SUB_REASON_MTM_REMOVE_STA;
					}
					MlmeEnqueue(pAd,
								APCLI_CTRL_STATE_MACHINE,
 								APCLI_CTRL_DISCONNECT_REQ,
								0,
								NULL,
								(REPT_MLME_START_IDX + pEntry->MatchReptCliIdx));
                    RTMP_MLME_HANDLER(pAd);
					continue;
				}
			}
#endif /* MAC_REPEATER_SUPPORT */

			if (IS_ENTRY_APCLI(pEntry))
				apcli_wdev = pEntry->wdev;

			apcli_avg_tx += pEntry->AvgTxBytes;
			apcli_avg_rx += pEntry->AvgRxBytes;
			

			if (((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
#ifdef CONFIG_MULTI_CHANNEL
				&& (pAd->Mlme.bStartMcc == FALSE)
#endif /* CONFIG_MULTI_CHANNEL */
			)
			{
				/* use Null or QoS Null to detect the ACTIVE station*/
				BOOLEAN ApclibQosNull = FALSE;

				if (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_WMM_CAPABLE))
					ApclibQosNull = TRUE;

			       ApCliRTMPSendNullFrame(pAd,pEntry->CurrTxRate, ApclibQosNull, pEntry, PWR_ACTIVE);

				continue;
			}
		}
#endif /* APCLI_SUPPORT */

		if (!IS_ENTRY_CLIENT(pEntry))
			continue;
                                if (pEntry ->fgGband256QAMSupport && sta_hit_2g_infra_case_number <= STA_NUMBER_FOR_TRIGGER) {
                                        sta_wdev = pEntry->wdev;
                                        if (WMODE_CAP_2G(sta_wdev->PhyMode)) {
                                               UINT tx_tp = (pEntry->AvgTxBytes >> BYTES_PER_SEC_TO_MBPS);
                                               UINT rx_tp = (pEntry->AvgRxBytes >> BYTES_PER_SEC_TO_MBPS); 
				
                                                if (tx_tp > INFRA_TP_PEEK_BOUND_THRESHOLD && 
                                                        (tx_tp *100)/(tx_tp+rx_tp) >TX_MODE_RATIO_THRESHOLD){
                                                        if (sta_hit_2g_infra_case_number < STA_NUMBER_FOR_TRIGGER) {
                                                                txop_wdev = sta_wdev;
                                                                sta_hit_2g_infra_case_number ++;
                                                        } else 
                                                                sta_hit_2g_infra_case_number ++;
                                                }

                                        }
                                                
                                }                


#ifdef MT_PS
		CheckSkipTX(pAd, pEntry);
#endif /* MT_PS */

		if (pEntry->NoDataIdleCount == 0)
			pEntry->StationKeepAliveCount = 0;

		pEntry->NoDataIdleCount ++;
		// TODO: shiang-usw,  remove upper setting becasue we need to migrate to tr_entry!
		pAd->MacTab.tr_entry[pEntry->wcid].NoDataIdleCount = 0;

		pEntry->StaConnectTime ++;

		pMbss = &pAd->ApCfg.MBSSID[pEntry->func_tb_idx];

		/* 0. STA failed to complete association should be removed to save MAC table space. */
		if ((pEntry->Sst != SST_ASSOC) && (pEntry->NoDataIdleCount >= pEntry->AssocDeadLine))
		{
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
					("%02x:%02x:%02x:%02x:%02x:%02x fail to complete ASSOC in %d sec\n",
					PRINT_MAC(pEntry->Addr), MAC_TABLE_ASSOC_TIMEOUT));
#ifdef WSC_AP_SUPPORT
			if (NdisEqualMemory(pEntry->Addr, pMbss->WscControl.EntryAddr, MAC_ADDR_LEN))
				NdisZeroMemory(pMbss->WscControl.EntryAddr, MAC_ADDR_LEN);
#endif /* WSC_AP_SUPPORT */
			MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
			continue;
		}

		/*
			1. check if there's any associated STA in power-save mode. this affects outgoing
				MCAST/BCAST frames should be stored in PSQ till DtimCount=0
		*/
		if (pEntry->PsMode == PWR_SAVE) {
			pMacTable->fAnyStationInPsm = TRUE;
			if (pEntry->wdev &&
					(pEntry->wdev->wdev_type == WDEV_TYPE_AP || pEntry->wdev->wdev_type == WDEV_TYPE_GO)) {
				// TODO: it looks like pEntry->wdev->tr_tb_idx is not assigned?
				pAd->MacTab.tr_entry[pEntry->wdev->tr_tb_idx].PsMode = PWR_SAVE;
				if (tr_entry->PsDeQWaitCnt)
				{
					tr_entry->PsDeQWaitCnt++;
					if (tr_entry->PsDeQWaitCnt > 2)
						tr_entry->PsDeQWaitCnt = 0;
				}
			}
		}

#ifdef DOT11_N_SUPPORT
		if (pEntry->MmpsMode == MMPS_DYNAMIC)
			pMacTable->fAnyStationMIMOPSDynamic = TRUE;

		if (pEntry->MaxHTPhyMode.field.BW == BW_20)
			pMacTable->fAnyStation20Only = TRUE;

		if (pEntry->MaxHTPhyMode.field.MODE != MODE_HTGREENFIELD)
			pMacTable->fAnyStationNonGF = TRUE;

		if ((pEntry->MaxHTPhyMode.field.MODE == MODE_OFDM) || (pEntry->MaxHTPhyMode.field.MODE == MODE_CCK))
			pMacTable->fAnyStationIsLegacy = TRUE;
#ifdef GREENAP_SUPPORT
		else
			pMacTable->fAnyStationIsHT=TRUE;
#endif /* GREENAP_SUPPORT */

#ifdef DOT11N_DRAFT3
		if (pEntry->bForty_Mhz_Intolerant)
			pMacTable->fAnyStaFortyIntolerant = TRUE;
#endif /* DOT11N_DRAFT3 */

		/* Get minimum AMPDU size from STA */
		if (MinimumAMPDUSize > pEntry->MaxRAmpduFactor)
			MinimumAMPDUSize = pEntry->MaxRAmpduFactor;
#endif /* DOT11_N_SUPPORT */

#if defined(RTMP_MAC) || defined(RLT_MAC)
        if (pAd->chipCap.hif_type == HIF_RTMP
                || pAd->chipCap.hif_type == HIF_RLT)
        {
            if (pEntry->bIAmBadAtheros)
            {
                pMacTable->fAnyStationBadAtheros = TRUE;
#ifdef DOT11_N_SUPPORT
                if (pAd->CommonCfg.IOTestParm.bRTSLongProtOn == FALSE)
                    AsicUpdateProtect(pAd, 8, ALLN_SETPROTECT, FALSE, pMacTable->fAnyStationNonGF);
#endif /* DOT11_N_SUPPORT */
            }
        }
#endif

        /* detect the station alive status */
		/* detect the station alive status */
		if ((pMbss->StationKeepAliveTime > 0) &&
			(pEntry->NoDataIdleCount >= pMbss->StationKeepAliveTime))
		{
			/*
				If no any data success between ap and the station for
				StationKeepAliveTime, try to detect whether the station is
				still alive.

				Note: Just only keepalive station function, no disassociation
				function if too many no response.
			*/

			/*
				For example as below:

				1. Station in ACTIVE mode,

		        ......
		        sam> tx ok!
		        sam> count = 1!	 ==> 1 second after the Null Frame is acked
		        sam> count = 2!	 ==> 2 second after the Null Frame is acked
		        sam> count = 3!
		        sam> count = 4!
		        sam> count = 5!
		        sam> count = 6!
		        sam> count = 7!
		        sam> count = 8!
		        sam> count = 9!
		        sam> count = 10!
		        sam> count = 11!
		        sam> count = 12!
		        sam> count = 13!
		        sam> count = 14!
		        sam> count = 15! ==> 15 second after the Null Frame is acked
		        sam> tx ok!      ==> (KeepAlive Mechanism) send a Null Frame to
										detect the STA life status
		        sam> count = 1!  ==> 1 second after the Null Frame is acked
		        sam> count = 2!
		        sam> count = 3!
		        sam> count = 4!
		        ......

				If the station acknowledges the QoS Null Frame,
				the NoDataIdleCount will be reset to 0.


				2. Station in legacy PS mode,

				We will set TIM bit after 15 seconds, the station will send a
				PS-Poll frame and we will send a QoS Null frame to it.
				If the station acknowledges the QoS Null Frame, the
				NoDataIdleCount will be reset to 0.


				3. Station in legacy UAPSD mode,

				Currently we do not support the keep alive mechanism.
				So if your station is in UAPSD mode, the station will be
				kicked out after 300 seconds.

				Note: the rate of QoS Null frame can not be 1M of 2.4GHz or
				6M of 5GHz, or no any statistics count will occur.
			*/

			if (pEntry->StationKeepAliveCount++ == 0)
			{
					if (pEntry->PsMode == PWR_SAVE)
					{
						/* use TIM bit to detect the PS station */
						WLAN_MR_TIM_BIT_SET(pAd, pEntry->func_tb_idx, pEntry->Aid);
					}
					else
					{
						/* use Null or QoS Null to detect the ACTIVE station */
						BOOLEAN bQosNull = FALSE;

						if (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_WMM_CAPABLE))
							bQosNull = TRUE;

						RtmpEnqueueNullFrame(pAd, pEntry->Addr, pEntry->CurrTxRate,
	    	                           						pEntry->Aid, pEntry->func_tb_idx, bQosNull, TRUE, 0);
					}
			}
			else
			{
				if (pEntry->StationKeepAliveCount >= pMbss->StationKeepAliveTime)
					pEntry->StationKeepAliveCount = 0;
			}
		}

		/* 2. delete those MAC entry that has been idle for a long time */
		if (pEntry->NoDataIdleCount >= pEntry->StaIdleTimeout)
		{
			bDisconnectSta = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("ageout %02x:%02x:%02x:%02x:%02x:%02x after %d-sec silence\n",
					PRINT_MAC(pEntry->Addr), pEntry->StaIdleTimeout));
			ApLogEvent(pAd, pEntry->Addr, EVENT_AGED_OUT);
		}
		else if (pEntry->ContinueTxFailCnt >= pAd->ApCfg.EntryLifeCheck)
		{
			/*
				AP have no way to know that the PwrSaving STA is leaving or not.
				So do not disconnect for PwrSaving STA.
			*/
			if (pEntry->PsMode != PWR_SAVE)
			{
				bDisconnectSta = TRUE;
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("STA-%02x:%02x:%02x:%02x:%02x:%02x had left (%d %lu)\n",
					PRINT_MAC(pEntry->Addr),
					pEntry->ContinueTxFailCnt, pAd->ApCfg.EntryLifeCheck));
			}
		}
#ifdef BAND_STEERING
		else if (pAd->ApCfg.BndStrgTable.bEnabled == TRUE)
		{
			if (BndStrg_IsClientStay(pAd, pEntry) == FALSE)
			{
				bDisconnectSta = TRUE;
			}
		}
#endif /* BAND_STEERING */

		if ((pMbss->RssiLowForStaKickOut != 0) &&
			  ( (avgRssi=RTMPAvgRssi(pAd, &pEntry->RssiSample)) < pMbss->RssiLowForStaKickOut))
		{
			bDisconnectSta = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("Disassoc STA %02x:%02x:%02x:%02x:%02x:%02x , RSSI Kickout Thres[%d]-[%d]\n",
					PRINT_MAC(pEntry->Addr), pMbss->RssiLowForStaKickOut,
					avgRssi));

		}


		if (bDisconnectSta)
		{
			/* send wireless event - for ageout */
			RTMPSendWirelessEvent(pAd, IW_AGEOUT_EVENT_FLAG, pEntry->Addr, 0, 0);

			if (pEntry->Sst == SST_ASSOC)
			{
				PUCHAR pOutBuffer = NULL;
				NDIS_STATUS NStatus;
				ULONG FrameLen = 0;
				HEADER_802_11 DeAuthHdr;
				USHORT Reason;

				/*  send out a DISASSOC request frame */
				NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
				if (NStatus != NDIS_STATUS_SUCCESS)
				{
					MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, (" MlmeAllocateMemory fail  ..\n"));
					/*NdisReleaseSpinLock(&pAd->MacTabLock); */
					continue;
				}
				Reason = REASON_DEAUTH_STA_LEAVING;
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("Send DEAUTH - Reason = %d frame  TO %x %x %x %x %x %x \n",
										Reason, PRINT_MAC(pEntry->Addr)));
				MgtMacHeaderInit(pAd, &DeAuthHdr, SUBTYPE_DEAUTH, 0, pEntry->Addr,
								pMbss->wdev.if_addr,
								pMbss->wdev.bssid);
				MakeOutgoingFrame(pOutBuffer, &FrameLen,
								sizeof(HEADER_802_11), &DeAuthHdr,
								2, &Reason,
								END_OF_ARGS);
				MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
				MlmeFreeMemory( pOutBuffer);

#ifdef MAC_REPEATER_SUPPORT
				if ((pAd->ApCfg.bMACRepeaterEn == TRUE) &&
                    IS_ENTRY_CLIENT(pEntry))
				{
					UCHAR apCliIdx, CliIdx;
					REPEATER_CLIENT_ENTRY *pReptEntry = NULL;

					pReptEntry = RTMPLookupRepeaterCliEntry(
                                                pAd,
                                                TRUE,
                                                pEntry->Addr,
                                                TRUE);
					if (pReptEntry &&
                        (pReptEntry->CliConnectState != REPT_ENTRY_DISCONNT))
					{
						pReptEntry->Disconnect_Sub_Reason = APCLI_DISCONNECT_SUB_REASON_MTM_REMOVE_STA;
						apCliIdx = pReptEntry->MatchApCliIdx;
						CliIdx = pReptEntry->MatchLinkIdx;
						MlmeEnqueue(pAd,
									APCLI_CTRL_STATE_MACHINE,
									APCLI_CTRL_DISCONNECT_REQ,
									0,
									NULL,
									(REPT_MLME_START_IDX + CliIdx));
						RTMP_MLME_HANDLER(pAd);
					}
				}
#endif /* MAC_REPEATER_SUPPORT */
			}

			MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
			continue;
		}

#ifdef CONFIG_HOTSPOT_R2
		if (pEntry->BTMDisassocCount == 1)
		{
			PUCHAR      pOutBuffer = NULL;
			NDIS_STATUS NStatus;
			ULONG       FrameLen = 0;
			HEADER_802_11 DisassocHdr;
			USHORT      Reason;

			/*  send out a DISASSOC request frame */
			NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
			if (NStatus != NDIS_STATUS_SUCCESS)
			{
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, (" MlmeAllocateMemory fail  ..\n"));
				/*NdisReleaseSpinLock(&pAd->MacTabLock); */
				continue;
			}

			Reason = REASON_DISASSOC_INACTIVE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("BTM ASSOC - Send DISASSOC  Reason = %d frame  TO %x %x %x %x %x %x \n",Reason,pEntry->Addr[0],
				pEntry->Addr[1],pEntry->Addr[2],pEntry->Addr[3],pEntry->Addr[4],pEntry->Addr[5]));
			MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0, pEntry->Addr, pMbss->wdev.if_addr, pMbss->wdev.bssid);
			MakeOutgoingFrame(pOutBuffer, &FrameLen, sizeof(HEADER_802_11), &DisassocHdr, 2, &Reason, END_OF_ARGS);
			MiniportMMRequest(pAd, MGMT_USE_PS_FLAG, pOutBuffer, FrameLen);
			MlmeFreeMemory( pOutBuffer);
			//JERRY
			if (!pEntry->IsKeep)
				MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
			continue;
		}
		if (pEntry->BTMDisassocCount != 0)
			pEntry->BTMDisassocCount--;
#endif /* CONFIG_HOTSPOT_R2 */

		/* 3. garbage collect the ps_queue if the STA has being idle for a while */
		if ((pEntry->PsMode == PWR_SAVE) && (tr_entry->ps_state == APPS_RETRIEVE_DONE || tr_entry->ps_state == APPS_RETRIEVE_IDLE))
		{
			 if (tr_entry->enqCount > 0)
			{
				tr_entry->PsQIdleCount++;
				if (tr_entry->PsQIdleCount > 5)
				{
					rtmp_tx_swq_exit(pAd, pEntry->wcid);
					tr_entry->PsQIdleCount = 0;
					WLAN_MR_TIM_BIT_CLEAR(pAd, pEntry->func_tb_idx, pEntry->Aid);
					MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():Clear WCID[%d] packets\n",__FUNCTION__, pEntry->wcid));
				}
			}
		}
		else
		{
			tr_entry->PsQIdleCount = 0;
		}

#ifdef UAPSD_SUPPORT
		UAPSD_QueueMaintenance(pAd, pEntry);
#endif /* UAPSD_SUPPORT */

		/* check if this STA is Ralink-chipset */
		if (!CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_RALINK_CHIPSET)) {
            UCHAR band_idx;

            band_idx = HcGetBandByWdev(pEntry->wdev);
			pMacTable->fAllStationAsRalink[band_idx] = FALSE;
        }

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
		if ((pEntry->BSS2040CoexistenceMgmtSupport)
			&& (pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_INFO_NOTIFY)
			&& (pAd->CommonCfg.bBssCoexEnable == TRUE)
		)
		{
			SendNotifyBWActionFrame(pAd, pEntry->wcid, pEntry->func_tb_idx);
		}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */
#ifdef WAPI_SUPPORT
		if (IS_CIPHER_WPI_SMS4(pEntry->SecConfig.PairwiseCipher))
			pMacTable->fAnyWapiStation = TRUE;
#endif /* WAPI_SUPPORT */

#if defined(PRE_ANT_SWITCH) || defined(CFO_TRACK)
		lastClient = wcid;
#endif /* defined(PRE_ANT_SWITCH) || defined(CFO_TRACK) */

		/* only apply burst when run in MCS0,1,8,9,16,17, not care about phymode */
		if ((pEntry->HTPhyMode.field.MCS != 32) &&
			((pEntry->HTPhyMode.field.MCS % 8 == 0) || (pEntry->HTPhyMode.field.MCS % 8 == 1)))
		{
			pMacTable->fAllStationGainGoodMCS = FALSE;
		}

		/* Check Current STA's Operation Mode is BW20 or BW40 */
		pMacTable->fCurrentStaBw40 = (pEntry->HTPhyMode.field.BW == BW_40) ? TRUE : FALSE;

#ifdef WFA_VHT_PF
		if (worst_rssi == NULL) {
			worst_rssi = &pEntry->RssiSample;
			worst_rssi_sta_idx = wcid;
		} else {
			if (worst_rssi->AvgRssi[0] > pEntry->RssiSample.AvgRssi[0]) {
				worst_rssi = &pEntry->RssiSample;
				worst_rssi_sta_idx = wcid;
			}
		}
#endif /* WFA_VHT_PF */

	}


#ifdef MT_MAC
	/* If we check that any preview stations are in Psm and no stations are in Psm now. */
	/* AP will dequeue all buffer broadcast packets */

	if ((pAd->chipCap.hif_type == HIF_MT) && (pMacTable->fAnyStationInPsm == FALSE)) {
		UINT apidx = 0;
        for (apidx = 0; apidx<pAd->ApCfg.BssidNum; apidx++)
        {
            BSS_STRUCT *pMbss;
            UINT wcid = 0;
            STA_TR_ENTRY *tr_entry = NULL;

            pMbss = &pAd->ApCfg.MBSSID[apidx];

            wcid = pMbss->wdev.tr_tb_idx;
            tr_entry = &pAd->MacTab.tr_entry[wcid];

			if ((bPreAnyStationInPsm == TRUE) &&  (tr_entry->tx_queue[QID_AC_BE].Head != NULL)) {
					if (tr_entry->tx_queue[QID_AC_BE].Number > MAX_PACKETS_IN_MCAST_PS_QUEUE)
						RTMPDeQueuePacket(pAd, FALSE, WMM_NUM_OF_AC, wcid, MAX_PACKETS_IN_MCAST_PS_QUEUE);
					else
						RTMPDeQueuePacket(pAd, FALSE, WMM_NUM_OF_AC, wcid, tr_entry->tx_queue[QID_AC_BE].Number);
			}
		}
	}
#endif

#ifdef WFA_VHT_PF
	if (worst_rssi != NULL &&
		((pAd->Mlme.OneSecPeriodicRound % 10) == 5) &&
		(worst_rssi_sta_idx >= 1))
	{
		CHAR gain = 2;
		if (worst_rssi->AvgRssi[0] >= -40)
			gain = 1;
		else if (worst_rssi->AvgRssi[0] <= -50)
			gain = 2;
		rt85592_lna_gain_adjust(pAd, gain);
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s():WorstRSSI for STA(%02x:%02x:%02x:%02x:%02x:%02x):%d,%d,%d, Set Gain as %s\n",
					__FUNCTION__,
					PRINT_MAC(pMacTable->Content[worst_rssi_sta_idx].Addr),
					worst_rssi->AvgRssi[0], worst_rssi->AvgRssi[1], worst_rssi->AvgRssi[2],
					(gain == 2 ? "Mid" : "Low")));
	}
#endif /* WFA_VHT_PF */

#ifdef PRE_ANT_SWITCH
#endif /* PRE_ANT_SWITCH */

#ifdef CFO_TRACK
#endif /* CFO_TRACK */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	if (pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_INFO_NOTIFY)
		pAd->CommonCfg.Bss2040CoexistFlag &= (~BSS_2040_COEXIST_INFO_NOTIFY);
#endif /* DOT11N_DRAFT3 */

	/* If all associated STAs are Ralink-chipset, AP shall enable RDG. */
	if (pAd->CommonCfg.bRdg)
    {
        if (pMacTable->fAllStationAsRalink[0])
            bRdgActive = TRUE;
        else
            bRdgActive = FALSE;

        if (pAd->CommonCfg.dbdc_mode)
        {
            if (pMacTable->fAllStationAsRalink[1])
                ; // Not support yet...
        }
    }

#ifdef APCLI_SUPPORT
	if (apcli_wdev) {
		UINT tx_tp = (apcli_avg_tx >> BYTES_PER_SEC_TO_MBPS);
		UINT rx_tp = (apcli_avg_rx >> BYTES_PER_SEC_TO_MBPS);
		apcli_dync_txop_alg(pAd, apcli_wdev, tx_tp, rx_tp);
	}
#endif /* APCLI_SUPPORT */

                if (sta_hit_2g_infra_case_number == STA_NUMBER_FOR_TRIGGER ) {
                        if (pAd->G_MODE_INFRA_TXOP_RUNNING == FALSE) {
                                pAd->g_mode_txop_wdev = txop_wdev;
                                pAd->G_MODE_INFRA_TXOP_RUNNING = TRUE;
                                enable_tx_burst(pAd, txop_wdev, AC_BE, PRIO_2G_INFRA, TXOP_FE);
                                
                        } else if (pAd->g_mode_txop_wdev != txop_wdev) {
                                disable_tx_burst(pAd, pAd->g_mode_txop_wdev, AC_BE, PRIO_2G_INFRA, TXOP_0);
                                enable_tx_burst(pAd, txop_wdev, AC_BE, PRIO_2G_INFRA, TXOP_FE);
                                pAd->g_mode_txop_wdev = txop_wdev;
                                
                        }
                        
                } else {
                        if (pAd->G_MODE_INFRA_TXOP_RUNNING == TRUE) {
                                disable_tx_burst(pAd, pAd->g_mode_txop_wdev, AC_BE, PRIO_2G_INFRA, TXOP_0);
                                pAd->G_MODE_INFRA_TXOP_RUNNING = FALSE; 
                                pAd->g_mode_txop_wdev = NULL;
                        }
                }
                        

	if (pAd->CommonCfg.bRalinkBurstMode && pMacTable->fAllStationGainGoodMCS)
		bRalinkBurstMode = TRUE;
	else
		bRalinkBurstMode = FALSE;

#ifdef GREENAP_SUPPORT
	if (WMODE_CAP_N(pMbss->wdev.PhyMode))
	{
		if(pAd->MacTab.fAnyStationIsHT == FALSE
			&& pAd->ApCfg.bGreenAPEnable == TRUE)
		{
			if (pAd->ApCfg.GreenAPLevel!=GREENAP_ONLY_11BG_STAS)
			{
				RTMP_CHIP_ENABLE_AP_MIMOPS(pAd,FALSE);
				pAd->ApCfg.GreenAPLevel=GREENAP_ONLY_11BG_STAS;
			}
		}
		else
		{
			if (pAd->ApCfg.GreenAPLevel!=GREENAP_11BGN_STAS)
			{
				RTMP_CHIP_DISABLE_AP_MIMOPS(pAd);
				pAd->ApCfg.GreenAPLevel=GREENAP_11BGN_STAS;
			}
		}
	}
#endif /* GREENAP_SUPPORT */

	if (bRdgActive != RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RDG_ACTIVE))
	{
        // DBDC not support yet, only using BAND_0
        if (bRdgActive) {
            AsicSetRDG(pAd, WCID_ALL, 0, 1, 1);
        }
        else {
            AsicSetRDG(pAd, WCID_ALL, 0, 0, 0);
        }
        //AsicWtblSetRDG(pAd, bRdgActive);
	}

	if (bRalinkBurstMode != RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RALINK_BURST_MODE))
		AsicSetRalinkBurstMode(pAd, bRalinkBurstMode);

#if defined(RTMP_MAC) || defined(RLT_MAC)
    if (pAd->chipCap.hif_type == HIF_RTMP
            || pAd->chipCap.hif_type == HIF_RLT)
    {
        if ((pMacTable->fAnyStationBadAtheros == FALSE)
                && (pAd->CommonCfg.IOTestParm.bRTSLongProtOn == TRUE))
        {
            AsicUpdateProtect(pAd, pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode,
                    ALLN_SETPROTECT, FALSE, pMacTable->fAnyStationNonGF);
        }
    }
#endif

#endif /* DOT11_N_SUPPORT */

#ifdef RTMP_MAC_PCI
    RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
#endif /* RTMP_MAC_PCI */
    /*
       4.
       garbage collect pAd->MacTab.McastPsQueue if backlogged MCAST/BCAST frames
       stale in queue. Since MCAST/BCAST frames always been sent out whenever
		DtimCount==0, the only case to let them stale is surprise removal of the NIC,
		so that ASIC-based Tbcn interrupt stops and DtimCount dead.
	*/
	// TODO: shiang-usw. revise this becasue now we have per-BSS McastPsQueue!
	if (pMacTable->McastPsQueue.Head)
	{
		UINT bss_index;

		pMacTable->PsQIdleCount ++;
		if (pMacTable->PsQIdleCount > 1)
		{

			APCleanupPsQueue(pAd, &pMacTable->McastPsQueue);
			pMacTable->PsQIdleCount = 0;

			if (pAd->ApCfg.BssidNum > MAX_MBSSID_NUM(pAd))
				pAd->ApCfg.BssidNum = MAX_MBSSID_NUM(pAd);

			/* clear MCAST/BCAST backlog bit for all BSS */
			for(bss_index=BSS0; bss_index<pAd->ApCfg.BssidNum; bss_index++)
				WLAN_MR_TIM_BCMC_CLEAR(bss_index);
		}
	}
	else
		pMacTable->PsQIdleCount = 0;
#ifdef RTMP_MAC_PCI
	RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);
#endif /* RTMP_MAC_PCI */
}


UINT32 MacTableAssocStaNumGet(RTMP_ADAPTER *pAd)
{
	UINT32 num = 0;
	UINT32 i;

	for (i = 1; VALID_UCAST_ENTRY_WCID(pAd, i); i++)
	{
		MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

		if (!IS_ENTRY_CLIENT(pEntry))
			continue;

		if (pEntry->Sst == SST_ASSOC)
			num ++;
	}

	return num;
}


/*
	==========================================================================
	Description:
		Look up a STA MAC table. Return its Sst to decide if an incoming
		frame from this STA or an outgoing frame to this STA is permitted.
	Return:
	==========================================================================
*/
MAC_TABLE_ENTRY *APSsPsInquiry(
	IN RTMP_ADAPTER *pAd,
	IN UCHAR *pAddr,
	OUT SST *Sst,
	OUT USHORT *Aid,
	OUT UCHAR *PsMode,
	OUT UCHAR *Rate)
{
	MAC_TABLE_ENTRY *pEntry = NULL;

	if (MAC_ADDR_IS_GROUP(pAddr)) /* mcast & broadcast address */
	{
		*Sst = SST_ASSOC;
		*Aid = MCAST_WCID_TO_REMOVE;	/* Softap supports 1 BSSID and use WCID=0 as multicast Wcid index */
		*PsMode = PWR_ACTIVE;
		*Rate = pAd->CommonCfg.MlmeRate;
	}
	else /* unicast address */
	{
		pEntry = MacTableLookup(pAd, pAddr);
		if (pEntry)
		{
			*Sst = pEntry->Sst;
			*Aid = pEntry->Aid;
			*PsMode = pEntry->PsMode;
			if (IS_AKM_WPA_CAPABILITY_Entry(pEntry)
				&& (pEntry->SecConfig.Handshake.GTKState != REKEY_ESTABLISHED))
			{
				*Rate = pAd->CommonCfg.MlmeRate;
			} else
				*Rate = pEntry->CurrTxRate;
		}
		else
		{
			*Sst = SST_NOT_AUTH;
			*Aid = MCAST_WCID_TO_REMOVE;
			*PsMode = PWR_ACTIVE;
			*Rate = pAd->CommonCfg.MlmeRate;
		}
	}

	return pEntry;
}


#ifdef SYSTEM_LOG_SUPPORT
/*
	==========================================================================
	Description:
		This routine is called to log a specific event into the event table.
		The table is a QUERY-n-CLEAR array that stop at full.
	==========================================================================
 */
VOID ApLogEvent(RTMP_ADAPTER *pAd, UCHAR *pAddr, USHORT Event)
{
	if (pAd->EventTab.Num < MAX_NUM_OF_EVENT)
	{
		RT_802_11_EVENT_LOG *pLog = &pAd->EventTab.Log[pAd->EventTab.Num];
		RTMP_GetCurrentSystemTime(&pLog->SystemTime);
		COPY_MAC_ADDR(pLog->Addr, pAddr);
		pLog->Event = Event;
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("LOG#%ld %02x:%02x:%02x:%02x:%02x:%02x %s\n",
			pAd->EventTab.Num, pAddr[0], pAddr[1], pAddr[2],
			pAddr[3], pAddr[4], pAddr[5], pEventText[Event]));
		pAd->EventTab.Num += 1;
	}
}
#endif /* SYSTEM_LOG_SUPPORT */


#ifdef DOT11_N_SUPPORT
/*
	==========================================================================
	Description:
		Operationg mode is as defined at 802.11n for how proteciton in this BSS operates.
		Ap broadcast the operation mode at Additional HT Infroamtion Element Operating Mode fields.
		802.11n D1.0 might has bugs so this operating mode use  EWC MAC 1.24 definition first.

		Called when receiving my bssid beacon or beaconAtJoin to update protection mode.
		40MHz or 20MHz protection mode in HT 40/20 capabale BSS.
		As STA, this obeys the operation mode in ADDHT IE.
		As AP, update protection when setting ADDHT IE and after new STA joined.
	==========================================================================
*/
VOID APUpdateOperationMode(RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
	BOOLEAN bDisableBGProtect = FALSE, bNonGFExist = FALSE;

	pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode = 0;
	if ((pAd->ApCfg.LastNoneHTOLBCDetectTime + (5 * OS_HZ)) > pAd->Mlme.Now32 && pAd->Mlme.Now32!=0) /* non HT BSS exist within 5 sec */
	{
		pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode = 1;
		bDisableBGProtect = FALSE;
		bNonGFExist = TRUE;
	}

   	/* If I am 40MHz BSS, and there exist HT-20MHz station. */
	/* Update to 2 when it's zero.  Because OperaionMode = 1 or 3 has more protection. */
	if ((pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode == 0) &&
		(pAd->MacTab.fAnyStation20Only) &&
		(pAd->CommonCfg.DesiredHtPhy.ChannelWidth == 1))
	{
		pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode = 2;
		bDisableBGProtect = TRUE;
	}

	if (pAd->MacTab.fAnyStationIsLegacy)
	{
		pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode = 3;
		bDisableBGProtect = TRUE;
	}

	if (bNonGFExist == FALSE)
		bNonGFExist = pAd->MacTab.fAnyStationNonGF;

	pAd->CommonCfg.AddHTInfo.AddHtInfo2.NonGfPresent = pAd->MacTab.fAnyStationNonGF;

    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            (" --%s:\n OperationMode: %d, bDisableBGProtect: %d, bNonGFExist: %d\n",
             __FUNCTION__, pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode,
             bDisableBGProtect, bNonGFExist));

	/*if no protect should enable for CTS-2-Self, WHQA_00025629*/
	if(MTK_REV_GTE(pAd, MT7615, MT7615E1) && MTK_REV_LT(pAd, MT7615, MT7615E3))
	{
		if(pAd->CommonCfg.dbdc_mode && pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode==0)
		{
			pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode=2;
		}
	}

    UpdateBeaconHandler(pAd, wdev, IE_CHANGE);

#if defined(MT_MAC)
    if (pAd->chipCap.hif_type == HIF_MT) {
        wdev->protection = SET_PROTECT(pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode);
        if (bDisableBGProtect == FALSE) {
            //wdev->protection |= SET_PROTECT(ERP);
        }
        if (bNonGFExist == FALSE) {
            wdev->protection |= SET_PROTECT(GREEN_FIELD_PROTECT);
        }
    }
#endif

    AsicUpdateProtect(pAd,
            pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode,
            (ALLN_SETPROTECT),
            bDisableBGProtect,
            bNonGFExist);
}
#endif /* DOT11_N_SUPPORT */


/*
	==========================================================================
	Description:
        Check to see the exist of long preamble STA in associated list
    ==========================================================================
 */
BOOLEAN ApCheckLongPreambleSTA(RTMP_ADAPTER *pAd)
{
    UCHAR   i;

    for (i=0; VALID_UCAST_ENTRY_WCID(pAd, i); i++)
    {
		PMAC_TABLE_ENTRY pEntry = &pAd->MacTab.Content[i];
		if (!IS_ENTRY_CLIENT(pEntry) || (pEntry->Sst != SST_ASSOC))
			continue;

        if (!CAP_IS_SHORT_PREAMBLE_ON(pEntry->CapabilityInfo))
        {
            return TRUE;
        }
    }

    return FALSE;
}


/*
	==========================================================================
	Description:
		Update ERP IE and CapabilityInfo based on STA association status.
		The result will be auto updated into the next outgoing BEACON in next
		TBTT interrupt service routine
	==========================================================================
 */

static VOID ApUpdateCapabilityAndErpleByRf(RTMP_ADAPTER *pAd, UCHAR RfIC)
{
	UCHAR  i, ErpIeContent = 0;
	BOOLEAN ShortSlotCapable = pAd->CommonCfg.bUseShortSlotTime;
	UCHAR	apidx;
	BOOLEAN bUseBGProtection;
	BOOLEAN LegacyBssExist;
	MAC_TABLE_ENTRY *pEntry = NULL;
	USHORT *pCapInfo = NULL;
	UCHAR Channel, PhyMode;

	if(!HcIsRfSupport(pAd,RfIC))
	{
		return ;
	}

	Channel = HcGetChannelByRf(pAd,RfIC);
	PhyMode = HcGetPhyModeByRf(pAd,RfIC);

	if (WMODE_EQUAL(PhyMode, WMODE_B))
	{
		return;
	}

    for (i=1; VALID_UCAST_ENTRY_WCID(pAd, i); i++)
    {
        pEntry = &pAd->MacTab.Content[i];

        if (!IS_ENTRY_CLIENT(pEntry) || (pEntry->Sst != SST_ASSOC))
        {
            continue;
        }

        if(!wmode_band_equal(PhyMode,pEntry->wdev->PhyMode))
        {
            continue;
        }

        /* at least one 11b client associated, turn on ERP.NonERPPresent bit */
        /* almost all 11b client won't support "Short Slot" time, turn off for maximum compatibility */
        if (pEntry->MaxSupportedRate < RATE_FIRST_OFDM_RATE)
        {
            ShortSlotCapable = FALSE;
            ErpIeContent |= 0x01;
        }

        /* at least one client can't support short slot */
        if ((pEntry->CapabilityInfo & 0x0400) == 0)
            ShortSlotCapable = FALSE;
    }

    /* legacy BSS exist within 5 sec */
    if ((pAd->ApCfg.LastOLBCDetectTime + (5 * OS_HZ)) > pAd->Mlme.Now32)
        LegacyBssExist = TRUE;
    else
        LegacyBssExist = FALSE;

    /* decide ErpIR.UseProtection bit, depending on pAd->CommonCfg.UseBGProtection
       AUTO (0): UseProtection = 1 if any 11b STA associated
       ON (1): always USE protection
       OFF (2): always NOT USE protection
       */
    if (pAd->CommonCfg.UseBGProtection == 0)
    {
        ErpIeContent = (ErpIeContent)? 0x03 : 0x00;
        /*if ((pAd->ApCfg.LastOLBCDetectTime + (5 * OS_HZ)) > pAd->Mlme.Now32) // legacy BSS exist within 5 sec */
        if (LegacyBssExist)
        {
            ErpIeContent |= 0x02;                                     /* set Use_Protection bit */
        }
    }
    else if (pAd->CommonCfg.UseBGProtection == 1)
    {
        ErpIeContent |= 0x02;
    }

    bUseBGProtection = (pAd->CommonCfg.UseBGProtection == 1) ||    /* always use */
        ((pAd->CommonCfg.UseBGProtection == 0) && ERP_IS_USE_PROTECTION(ErpIeContent));

#ifdef A_BAND_SUPPORT
    /* always no BG protection in A-band. falsely happened when switching A/G band to a dual-band AP */
    if (Channel > 14)
        bUseBGProtection = FALSE;
#endif /* A_BAND_SUPPORT */

    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("-- bUseBGProtection: %s, BG_PROTECT_INUSED: %s, ERP IE Content: 0x%x\n",
             (bUseBGProtection)?"Yes":"No",
             (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))?"Yes":"No",
             ErpIeContent));

    if (bUseBGProtection != OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))
    {
        USHORT OperationMode = 0;
        BOOLEAN	bNonGFExist = 0;

#ifdef DOT11_N_SUPPORT
        OperationMode = pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode;
        bNonGFExist = pAd->MacTab.fAnyStationNonGF;
#endif /* DOT11_N_SUPPORT */

        if (bUseBGProtection)
        {
            OPSTATUS_SET_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);
#if defined(RTMP_MAC) || defined(RLT_MAC)
            if (pAd->chipCap.hif_type == HIF_RTMP
                    || pAd->chipCap.hif_type == HIF_RLT)
            {
                AsicUpdateProtect(pAd, OperationMode,
                        (OFDMSETPROTECT), FALSE, bNonGFExist);
            }
#endif
        }
        else
        {
            OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);
#if defined(RTMP_MAC) || defined(RLT_MAC)
            if (pAd->chipCap.hif_type == HIF_RTMP
                    || pAd->chipCap.hif_type == HIF_RLT)
            {
                AsicUpdateProtect(pAd, OperationMode,
                        (OFDMSETPROTECT), TRUE, bNonGFExist);
            }
#endif
        }

    }

    /* Decide Barker Preamble bit of ERP IE */
    if ((pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong) || (ApCheckLongPreambleSTA(pAd) == TRUE))
        pAd->ApCfg.ErpIeContent = (ErpIeContent | 0x04);
    else
        pAd->ApCfg.ErpIeContent = ErpIeContent;

#ifdef A_BAND_SUPPORT
    /* Force to use ShortSlotTime at A-band */
    if(Channel> 14)
    {
        ShortSlotCapable = TRUE;
    }
#endif /* A_BAND_SUPPORT */

    /* deicide CapabilityInfo.ShortSlotTime bit */
    for (apidx=0; apidx<pAd->ApCfg.BssidNum; apidx++)
    {
        /*update and check when the band is the same as wdev*/
        if(!wmode_band_equal(PhyMode,pAd->ApCfg.MBSSID[apidx].wdev.PhyMode))
        {
            continue;
        }

        pCapInfo = &(pAd->ApCfg.MBSSID[apidx].CapabilityInfo);

        /* In A-band, the ShortSlotTime bit should be ignored. */
        if (ShortSlotCapable
#ifdef A_BAND_SUPPORT
                && (Channel <= 14)
#endif /* A_BAND_SUPPORT */
           )
            (*pCapInfo) |= 0x0400;
        else
            (*pCapInfo) &= 0xfbff;


        if (pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong)
            (*pCapInfo) &= (~0x020);
        else
            (*pCapInfo) |= 0x020;

    }

	/*update slot time only when value is difference*/
	if(pAd->CommonCfg.bUseShortSlotTime!=ShortSlotCapable)
	{
    	HW_SET_SLOTTIME(pAd, ShortSlotCapable, Channel, NULL);
		pAd->CommonCfg.bUseShortSlotTime = ShortSlotCapable;
	}
}



VOID APUpdateCapabilityAndErpIe(RTMP_ADAPTER *pAd)
{
	ApUpdateCapabilityAndErpleByRf(pAd,RFIC_24GHZ);
	ApUpdateCapabilityAndErpleByRf(pAd,RFIC_5GHZ);
}


/*
	==========================================================================
	Description:
		Check if the specified STA pass the Access Control List checking.
		If fails to pass the checking, then no authentication nor association
		is allowed
	Return:
		MLME_SUCCESS - this STA passes ACL checking

	==========================================================================
*/
BOOLEAN ApCheckAccessControlList(RTMP_ADAPTER *pAd, UCHAR *pAddr, UCHAR Apidx)
{
	BOOLEAN Result = TRUE;

    if (Apidx >= HW_BEACON_MAX_NUM)
        return FALSE;

    if (pAd->ApCfg.MBSSID[Apidx].AccessControlList.Policy == 0)       /* ACL is disabled */
        Result = TRUE;
    else
    {
        ULONG i;
        if (pAd->ApCfg.MBSSID[Apidx].AccessControlList.Policy == 1)   /* ACL is a positive list */
            Result = FALSE;
        else                                              /* ACL is a negative list */
            Result = TRUE;
        for (i=0; i<pAd->ApCfg.MBSSID[Apidx].AccessControlList.Num; i++)
        {
            if (MAC_ADDR_EQUAL(pAddr, pAd->ApCfg.MBSSID[Apidx].AccessControlList.Entry[i].Addr))
            {
                Result = !Result;
                break;
            }
        }
    }

    if (Result == FALSE)
    {
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%02x:%02x:%02x:%02x:%02x:%02x failed in ACL checking\n",
        			PRINT_MAC(pAddr)));
    }

    return Result;
}


/*
	==========================================================================
	Description:
		This routine update the current MAC table based on the current ACL.
		If ACL change causing an associated STA become un-authorized. This STA
		will be kicked out immediately.
	==========================================================================
*/
VOID ApUpdateAccessControlList(RTMP_ADAPTER *pAd, UCHAR Apidx)
{
	USHORT   AclIdx, MacIdx;
	BOOLEAN  Matched;
	PUCHAR      pOutBuffer = NULL;
	NDIS_STATUS NStatus;
	ULONG       FrameLen = 0;
	HEADER_802_11 DisassocHdr;
	USHORT      Reason;
	MAC_TABLE_ENTRY *pEntry;
	BSS_STRUCT *pMbss;
	BOOLEAN drop;

	ASSERT(Apidx < MAX_MBSSID_NUM(pAd));
	if (Apidx >= MAX_MBSSID_NUM(pAd))
		return;
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("ApUpdateAccessControlList : Apidx = %d\n", Apidx));

	/* ACL is disabled. Do nothing about the MAC table. */
	pMbss = &pAd->ApCfg.MBSSID[Apidx];
	if (pMbss->AccessControlList.Policy == 0)
		return;

	for (MacIdx=0; VALID_UCAST_ENTRY_WCID(pAd, MacIdx); MacIdx++)
	{
		pEntry = &pAd->MacTab.Content[MacIdx];
		if (!IS_ENTRY_CLIENT(pEntry))
			continue;

		/* We only need to update associations related to ACL of MBSSID[Apidx]. */
		if (pEntry->func_tb_idx != Apidx)
			continue;

		drop = FALSE;
		Matched = FALSE;
		 for (AclIdx = 0; AclIdx < pMbss->AccessControlList.Num; AclIdx++)
		{
			if (MAC_ADDR_EQUAL(&pEntry->Addr[0], pMbss->AccessControlList.Entry[AclIdx].Addr))
			{
				Matched = TRUE;
				break;
			}
		}

		if ((Matched == FALSE) && (pMbss->AccessControlList.Policy == 1))
		{
			drop = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("STA not on positive ACL. remove it...\n"));
		}
	       else if ((Matched == TRUE) && (pMbss->AccessControlList.Policy == 2))
		{
			drop = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("STA on negative ACL. remove it...\n"));
		}

		if (drop == TRUE) {
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Apidx = %d\n", Apidx));
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("pAd->ApCfg.MBSSID[%d].AccessControlList.Policy = %ld\n", Apidx,
				pMbss->AccessControlList.Policy));

			/* Before delete the entry from MacTable, send disassociation packet to client. */
			if (pEntry->Sst == SST_ASSOC)
			{
				/* send out a DISASSOC frame */
				NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
				if (NStatus != NDIS_STATUS_SUCCESS)
				{
					MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, (" MlmeAllocateMemory fail  ..\n"));
					return;
				}

				Reason = REASON_DECLINED;
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("ASSOC - Send DISASSOC  Reason = %d frame  TO %x %x %x %x %x %x \n",
							Reason, PRINT_MAC(pEntry->Addr)));
				MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0,
									pEntry->Addr,
									pMbss->wdev.if_addr,
									pMbss->wdev.bssid);
				MakeOutgoingFrame(pOutBuffer, &FrameLen, sizeof(HEADER_802_11), &DisassocHdr, 2, &Reason, END_OF_ARGS);
				MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
				MlmeFreeMemory( pOutBuffer);

				RtmpusecDelay(5000);
			}
			MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
		}
	}
}


#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
/*
	Depends on the 802.11n Draft 4.0, Before the HT AP start a BSS, it should scan some specific channels to
collect information of existing BSSs, then depens on the collected channel information, adjust the primary channel
and secondary channel setting.

	For 5GHz,
		Rule 1: If the AP chooses to start a 20/40 MHz BSS in 5GHz and that occupies the same two channels
				as any existing 20/40 MHz BSSs, then the AP shall ensure that the primary channel of the
				new BSS is identical to the primary channel of the existing 20/40 MHz BSSs and that the
				secondary channel of the new 20/40 MHz BSS is identical to the secondary channel of the
				existing 20/40 MHz BSSs, unless the AP discoverr that on those two channels are existing
				20/40 MHz BSSs with different primary and secondary channels.
		Rule 2: If the AP chooses to start a 20/40MHz BSS in 5GHz, the selected secondary channel should
				correspond to a channel on which no beacons are detected during the overlapping BSS
				scan time performed by the AP, unless there are beacons detected on both the selected
				primary and secondary channels.
		Rule 3: An HT AP should not start a 20 MHz BSS in 5GHz on a channel that is the secondary channel
				of a 20/40 MHz BSS.
	For 2.4GHz,
		Rule 1: The AP shall not start a 20/40 MHz BSS in 2.4GHz if the value of the local variable "20/40
				Operation Permitted" is FALSE.

		20/40OperationPermitted =  (P == OPi for all values of i) AND
								(P == OTi for all values of i) AND
								(S == OSi for all values if i)
		where
			P 	is the operating or intended primary channel of the 20/40 MHz BSS
			S	is the operating or intended secondary channel of the 20/40 MHz BSS
			OPi  is member i of the set of channels that are members of the channel set C and that are the
				primary operating channel of at least one 20/40 MHz BSS that is detected within the AP's
				BSA during the previous X seconds
			OSi  is member i of the set of channels that are members of the channel set C and that are the
				secondary operating channel of at least one 20/40 MHz BSS that is detected within AP's
				BSA during the previous X seconds
			OTi  is member i of the set of channels that comparises all channels that are members of the
				channel set C that were listed once in the Channel List fields of 20/40 BSS Intolerant Channel
				Report elements receved during the previous X seconds and all channels that are members
				of the channel set C and that are the primary operating channel of at least one 20/40 MHz
				BSS that were detected within the AP's BSA during the previous X seconds.
			C	is the set of all channels that are allowed operating channels within the current operational
				regulatory domain and whose center frequency falls within the 40 MHz affected channel
				range given by following equation:
					                                                 Fp + Fs                  Fp + Fs
					40MHz affected channel range = [ ------  - 25MHz,  ------- + 25MHz ]
					                                                      2                          2
					Where
						Fp = the center frequency of channel P
						Fs = the center frequency of channel S

			"==" means that the values on either side of the "==" are to be tested for equaliy with a resulting
				 Boolean value.
			        =>When the value of OPi is the empty set, then the expression (P == OPi for all values of i)
			        	is defined to be TRUE
			        =>When the value of OTi is the empty set, then the expression (P == OTi for all values of i)
			        	is defined to be TRUE
			        =>When the value of OSi is the empty set, then the expression (S == OSi for all values of i)
			        	is defined to be TRUE
*/
INT GetBssCoexEffectedChRange(
	IN RTMP_ADAPTER *pAd,
	IN struct wifi_dev *wdev,
	IN BSS_COEX_CH_RANGE *pCoexChRange,
	IN UCHAR Channel)
{
	INT index, cntrCh = 0;

	memset(pCoexChRange, 0, sizeof(BSS_COEX_CH_RANGE));

	/* Build the effected channel list, if something wrong, return directly. */
#ifdef A_BAND_SUPPORT
	if (Channel > 14)
	{	/* For 5GHz band */
		for (index = 0; index < pAd->ChannelListNum; index++)
		{
			if(pAd->ChannelList[index].Channel == Channel)
				break;
		}

		if (index < pAd->ChannelListNum)
		{
			/* First get the primary channel */
			pCoexChRange->primaryCh = pAd->ChannelList[index].Channel;

			/* Now check about the secondary and central channel */
			if(wdev->extcha == EXTCHA_ABOVE)
			{
				pCoexChRange->effectChStart = pCoexChRange->primaryCh;
				pCoexChRange->effectChEnd = pCoexChRange->primaryCh + 4;
				pCoexChRange->secondaryCh = pCoexChRange->effectChEnd;
			}
			else
			{
				pCoexChRange->effectChStart = pCoexChRange->primaryCh -4;
				pCoexChRange->effectChEnd = pCoexChRange->primaryCh;
				pCoexChRange->secondaryCh = pCoexChRange->effectChStart;
			}

			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("5.0GHz: Found CtrlCh idx(%d) from the ChList, ExtCh=%s, PriCh=[Idx:%d, CH:%d], SecCh=[Idx:%d, CH:%d], effected Ch=[CH:%d~CH:%d]!\n",
										index,
										((wdev->extcha == EXTCHA_ABOVE) ? "ABOVE" : "BELOW"),
										pCoexChRange->primaryCh, pAd->ChannelList[pCoexChRange->primaryCh].Channel,
										pCoexChRange->secondaryCh, pAd->ChannelList[pCoexChRange->secondaryCh].Channel,
										pAd->ChannelList[pCoexChRange->effectChStart].Channel,
										pAd->ChannelList[pCoexChRange->effectChEnd].Channel));
			return TRUE;
		}
		else
		{
			/* It should not happened! */
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("5GHz: Cannot found the CtrlCh(%d) in ChList, something wrong?\n",
						Channel));
		}
	}
	else
#endif /* A_BAND_SUPPORT */
	{	/* For 2.4GHz band */
		for (index = 0; index < pAd->ChannelListNum; index++)
		{
			if(pAd->ChannelList[index].Channel == Channel)
				break;
		}

		if (index < pAd->ChannelListNum)
		{
			/* First get the primary channel */
			pCoexChRange->primaryCh = index;

			/* Now check about the secondary and central channel */
			if(wdev->extcha == EXTCHA_ABOVE)
			{
				if ((index + 4) < pAd->ChannelListNum)
				{
					cntrCh = index + 2;
					pCoexChRange->secondaryCh = index + 4;
				}
			}
			else
			{
				if ((index - 4) >=0)
				{
					cntrCh = index - 2;
					pCoexChRange->secondaryCh = index - 4;
				}
			}

			if (cntrCh)
			{
				pCoexChRange->effectChStart = (cntrCh - 5) > 0 ? (cntrCh - 5) : 0;
				pCoexChRange->effectChEnd= (cntrCh + 5) > 0 ? (cntrCh + 5) : 0;
				MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("2.4GHz: Found CtrlCh idx(%d) from the ChList, ExtCh=%s, PriCh=[Idx:%d, CH:%d], SecCh=[Idx:%d, CH:%d], effected Ch=[CH:%d~CH:%d]!\n",
										index,
										((wdev->extcha == EXTCHA_ABOVE) ? "ABOVE" : "BELOW"),
										pCoexChRange->primaryCh, pAd->ChannelList[pCoexChRange->primaryCh].Channel,
										pCoexChRange->secondaryCh, pAd->ChannelList[pCoexChRange->secondaryCh].Channel,
										pAd->ChannelList[pCoexChRange->effectChStart].Channel,
										pAd->ChannelList[pCoexChRange->effectChEnd].Channel));
			}
			return TRUE;
		}

		/* It should not happened! */
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("2.4GHz: Didn't found valid channel range, Ch index=%d, ChListNum=%d, CtrlCh=%d\n",
									index, pAd->ChannelListNum, Channel));
	}

	return FALSE;
}


VOID APOverlappingBSSScan(RTMP_ADAPTER *pAd,struct wifi_dev *wdev)
{
	BOOLEAN needFallBack = FALSE;
	INT chStartIdx, chEndIdx, index,curPriChIdx, curSecChIdx;
	BSS_COEX_CH_RANGE  coexChRange;
	UCHAR PhyMode = wdev->PhyMode;
	UCHAR Channel = wdev->channel;

	if(!HcIsRfSupport(pAd,RFIC_5GHZ))
	{
		return;
	}

	/* We just care BSS who operating in 40MHz N Mode. */
	if ((!WMODE_CAP_N(PhyMode)) ||
		(pAd->CommonCfg.RegTransmitSetting.field.BW  == BW_20)
		)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("The wdev->PhyMode=%d, BW=%d, didn't need channel adjustment!\n",
				PhyMode, pAd->CommonCfg.RegTransmitSetting.field.BW));
		return;
	}

	/* Build the effected channel list, if something wrong, return directly. */
	/* For 2.4GHz band */
	for (index = 0; index < pAd->ChannelListNum; index++)
	{
		if(pAd->ChannelList[index].Channel == Channel)
			break;
	}

	if (index < pAd->ChannelListNum)
	{

		if(wdev->extcha== EXTCHA_ABOVE)
		{
			curPriChIdx = index;
			curSecChIdx = ((index + 4) < pAd->ChannelListNum) ? (index + 4) : (pAd->ChannelListNum - 1);

			chStartIdx = (curPriChIdx >= 3) ? (curPriChIdx - 3) : 0;
			chEndIdx = ((curSecChIdx + 3) < pAd->ChannelListNum) ? (curSecChIdx + 3) : (pAd->ChannelListNum - 1);
		}
		else
		{
			curPriChIdx = index;
			curSecChIdx = ((index - 4) >=0 ) ? (index - 4) : 0;
			chStartIdx =(curSecChIdx >= 3) ? (curSecChIdx - 3) : 0;
			chEndIdx =  ((curPriChIdx + 3) < pAd->ChannelListNum) ? (curPriChIdx + 3) : (pAd->ChannelListNum - 1);;
		}
	}
	else
	{
		/* It should not happened! */
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("2.4GHz: Cannot found the Control Channel(%d) in ChannelList, something wrong?\n",
					Channel));
		return;
	}

	GetBssCoexEffectedChRange(pAd,wdev,&coexChRange, Channel);

	/* Before we do the scanning, clear the bEffectedChannel as zero for latter use. */
	for (index = 0; index < pAd->ChannelListNum; index++)
		pAd->ChannelList[index].bEffectedChannel = 0;

	pAd->CommonCfg.BssCoexApCnt = 0;

	/* If we are not ready for Tx/Rx Pakcet, enable it now for receiving Beacons. */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP) == 0)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Card still not enable Tx/Rx, enable it now!\n"));

		RTMP_IRQ_ENABLE(pAd);

		/* rtmp_rx_done_handle() API will check this flag to decide accept incoming packet or not. */
		/* Set the flag be ready to receive Beacon frame for autochannel select. */
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);
	}

	RTMPEnableRxTx(pAd);

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Ready to do passive scanning for Channel[%d] to Channel[%d]!\n",
			pAd->ChannelList[chStartIdx].Channel, pAd->ChannelList[chEndIdx].Channel));

	/* Now start to do the passive scanning. */
	pAd->CommonCfg.bOverlapScanning = TRUE;
	for (index = chStartIdx; index<=chEndIdx; index++)
	{
		Channel = pAd->ChannelList[index].Channel;
		AsicSetChannel(pAd, Channel, BW_20,  EXTCHA_NONE, TRUE);

		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("SYNC - BBP R4 to 20MHz.l\n"));
		/*MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Passive scanning for Channel %d.....\n", Channel)); */
		OS_WAIT(300); /* wait for 200 ms at each channel. */
	}
	pAd->CommonCfg.bOverlapScanning = FALSE;

	/* After scan all relate channels, now check the scan result to find out if we need fallback to 20MHz. */
	for (index = chStartIdx; index <= chEndIdx; index++)
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Channel[Idx=%d, Ch=%d].bEffectedChannel=0x%x!\n",
					index, pAd->ChannelList[index].Channel, pAd->ChannelList[index].bEffectedChannel));
		if ((pAd->ChannelList[index].bEffectedChannel & (EFFECTED_CH_PRIMARY | EFFECTED_CH_LEGACY))  && (index != curPriChIdx) )
		{
			needFallBack = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("needFallBack=TRUE due to OP/OT!\n"));
		}
		if ((pAd->ChannelList[index].bEffectedChannel & EFFECTED_CH_SECONDARY)  && (index != curSecChIdx))
		{
			needFallBack = TRUE;
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("needFallBack=TRUE due to OS!\n"));
		}
	}

	/* If need fallback, now do it. */
	if ((needFallBack == TRUE)
		&& (pAd->CommonCfg.BssCoexApCnt > pAd->CommonCfg.BssCoexApCntThr)
	)
	{
		pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = BW_20;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = EXTCHA_NONE;
		HcUpdateExtCha(pAd,wdev->channel,EXTCHA_NONE);
		pAd->CommonCfg.LastBSSCoexist2040.field.BSS20WidthReq = 1;
		pAd->CommonCfg.Bss2040CoexistFlag |= BSS_2040_COEXIST_INFO_SYNC;

        /*update hw setting.*/
        HcBbpSetBwByChannel(pAd,BW_20,Channel);
	}

	return;
}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */


#ifdef DOT1X_SUPPORT
/*
 ========================================================================
 Routine Description:
    Send Leyer 2 Frame to notify 802.1x daemon. This is a internal command

 Arguments:

 Return Value:
    TRUE - send successfully
    FAIL - send fail

 Note:
 ========================================================================
*/
BOOLEAN DOT1X_InternalCmdAction(
    IN  PRTMP_ADAPTER	pAd,
    IN  MAC_TABLE_ENTRY *pEntry,
    IN UINT8 cmd)
{
	INT				apidx = MAIN_MBSSID;
	UCHAR 			RalinkIe[9] = {221, 7, 0x00, 0x0c, 0x43, 0x00, 0x00, 0x00, 0x00};
	UCHAR			s_addr[MAC_ADDR_LEN];
	UCHAR			EAPOL_IE[] = {0x88, 0x8e};
	UINT8			frame_len = LENGTH_802_3 + sizeof(RalinkIe);
	UCHAR			FrameBuf[frame_len];
	UINT8			offset = 0;

	/* Init the frame buffer */
	NdisZeroMemory(FrameBuf, frame_len);

	if (pEntry)
	{
		apidx = pEntry->func_tb_idx;
		NdisMoveMemory(s_addr, pEntry->Addr, MAC_ADDR_LEN);
	}
	else
	{
		/* Fake a Source Address for transmission */
		NdisMoveMemory(s_addr, pAd->ApCfg.MBSSID[apidx].wdev.bssid, MAC_ADDR_LEN);
		s_addr[0] |= 0x80;
	}

	/* Assign internal command for Ralink dot1x daemon */
	RalinkIe[5] = cmd;

	/* Prepare the 802.3 header */
	MAKE_802_3_HEADER(FrameBuf,
					  pAd->ApCfg.MBSSID[apidx].wdev.bssid,
					  s_addr,
					  EAPOL_IE);
	offset += LENGTH_802_3;

	/* Prepare the specific header of internal command */
	NdisMoveMemory(&FrameBuf[offset], RalinkIe, sizeof(RalinkIe));

	/* Report to upper layer */
	if (RTMP_L2_FRAME_TX_ACTION(pAd, apidx, FrameBuf, frame_len) == FALSE)
		return FALSE;

	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s done. (cmd=%d)\n", __FUNCTION__, cmd));

	return TRUE;
}


/*
 ========================================================================
 Routine Description:
    Send Leyer 2 Frame to trigger 802.1x EAP state machine.

 Arguments:

 Return Value:
    TRUE - send successfully
    FAIL - send fail

 Note:
 ========================================================================
*/
BOOLEAN DOT1X_EapTriggerAction(RTMP_ADAPTER *pAd, MAC_TABLE_ENTRY *pEntry)
{
	// TODO: shiang-usw, fix me for pEntry->apidx to func_tb_idx
	INT				apidx = MAIN_MBSSID;
	UCHAR 			eapol_start_1x_hdr[4] = {0x01, 0x01, 0x00, 0x00};
	UINT8			frame_len = LENGTH_802_3 + sizeof(eapol_start_1x_hdr);
	UCHAR			FrameBuf[frame_len+32];
	UINT8			offset = 0;

	if(IS_AKM_1X_Entry(pEntry) || IS_IEEE8021X_Entry(&pAd->ApCfg.MBSSID[apidx].wdev))
	{
		/* Init the frame buffer */
		NdisZeroMemory(FrameBuf, frame_len);

		/* Assign apidx */
		apidx = pEntry->func_tb_idx;

		/* Prepare the 802.3 header */
		MAKE_802_3_HEADER(FrameBuf, pAd->ApCfg.MBSSID[apidx].wdev.bssid, pEntry->Addr, EAPOL);
		offset += LENGTH_802_3;

		/* Prepare a fake eapol-start body */
		NdisMoveMemory(&FrameBuf[offset], eapol_start_1x_hdr, sizeof(eapol_start_1x_hdr));

#ifdef CONFIG_HOTSPOT_R2
		if (pEntry)
		{
        		BSS_STRUCT *pMbss = pEntry->pMbss;
			if ((pMbss->HotSpotCtrl.HotSpotEnable == 1) && (IS_AKM_WPA2_Entry(&pMbss->wdev)) && (pEntry->hs_info.ppsmo_exist == 1))
			{
                		UCHAR HS2_Header[4] = {0x50,0x6f,0x9a,0x12};
				memcpy(&FrameBuf[offset+sizeof(eapol_start_1x_hdr)], HS2_Header, 4);
				memcpy(&FrameBuf[offset+sizeof(eapol_start_1x_hdr)+4], &pEntry->hs_info, sizeof(struct _sta_hs_info));
				frame_len += 4+sizeof(struct _sta_hs_info);
				printk("event eapol start, %x:%x:%x:%x\n",
						FrameBuf[offset+sizeof(eapol_start_1x_hdr)+4],FrameBuf[offset+sizeof(eapol_start_1x_hdr)+5],
						FrameBuf[offset+sizeof(eapol_start_1x_hdr)+6],FrameBuf[offset+sizeof(eapol_start_1x_hdr)+7]);
            		}
		}
#endif
		/* Report to upper layer */
		if (RTMP_L2_FRAME_TX_ACTION(pAd, apidx, FrameBuf, frame_len) == FALSE)
			return FALSE;

		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Notify 8021.x daemon to trigger EAP-SM for this sta(%02x:%02x:%02x:%02x:%02x:%02x)\n", PRINT_MAC(pEntry->Addr)));

	}

	return TRUE;
}

#endif /* DOT1X_SUPPORT */


INT rtmp_ap_init(RTMP_ADAPTER *pAd)
{
#ifdef WSC_AP_SUPPORT
    UCHAR j;
    BSS_STRUCT *mbss = NULL;
    struct wifi_dev *wdev = NULL;
    PWSC_CTRL pWscControl;

    for(j = BSS0; j < pAd->ApCfg.BssidNum; j++)
    {
        mbss = &pAd->ApCfg.MBSSID[j];
        wdev = &pAd->ApCfg.MBSSID[j].wdev;
        {
            pWscControl = &mbss->WscControl;

            pWscControl->WscRxBufLen = 0;
            pWscControl->pWscRxBuf = NULL;
            os_alloc_mem(pAd, &pWscControl->pWscRxBuf, MGMT_DMA_BUFFER_SIZE);
            if (pWscControl->pWscRxBuf)
                NdisZeroMemory(pWscControl->pWscRxBuf, MGMT_DMA_BUFFER_SIZE);
            pWscControl->WscTxBufLen = 0;
            pWscControl->pWscTxBuf = NULL;
            os_alloc_mem(pAd, &pWscControl->pWscTxBuf, MGMT_DMA_BUFFER_SIZE);
            if (pWscControl->pWscTxBuf)
                NdisZeroMemory(pWscControl->pWscTxBuf, MGMT_DMA_BUFFER_SIZE);
        }
    }
#endif /* WSC_AP_SUPPORT */

	APOneShotSettingInitialize(pAd);

    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("apstart up %02x:%02x:%02x:%02x:%02x:%02x\n",
                PRINT_MAC(pAd->CurrentAddress)));

	APStartUpForMain(pAd);

	MlmeRadioOn(pAd, &pAd->ApCfg.MBSSID[BSS0].wdev);


	/* Set up the Mac address*/
	RtmpOSNetDevAddrSet(pAd->OpMode, pAd->net_dev, &pAd->CurrentAddress[0], NULL);
//	ap_func_init(pAd);

	if (pAd->chipCap.hif_type == HIF_MT)
	{
		/* Now Enable MacRxTx*/
		RTMP_IRQ_ENABLE(pAd);
		RTMPEnableRxTx(pAd);
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);
	}
	return NDIS_STATUS_SUCCESS;
}


 VOID rtmp_ap_exit(RTMP_ADAPTER *pAd)
{
	
	BOOLEAN Cancelled;
	
	RTMPReleaseTimer(&pAd->ApCfg.CounterMeasureTimer,&Cancelled);
#if defined (RTMP_MAC_USB) || defined(RTMP_MAC_SDIO)
	RTMPReleaseTimer(&pAd->CommonCfg.BeaconUpdateTimer,&Cancelled);
#endif /* RTMP_MAC_USB */
	
#ifdef IDS_SUPPORT
	/* Init intrusion detection timer */
	RTMPReleaseTimer(&pAd->ApCfg.IDSTimer,&Cancelled);
#endif /* IDS_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3

	RTMP_11N_D3_TimerRelease(pAd);

#endif /*DOT11N_DRAFT3*/
#endif /*DOT11_N_SUPPORT*/

	/* Free BssTab & ChannelInfo tabbles.*/
	AutoChBssTableDestroy(pAd);
	ChannelInfoDestroy(pAd);

#ifdef IGMP_SNOOP_SUPPORT
	MultiCastFilterTableReset(pAd, &pAd->pMulticastFilterTable);
#endif /* IGMP_SNOOP_SUPPORT */
}


