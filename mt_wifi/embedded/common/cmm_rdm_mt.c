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
    cmm_rdm_mt.c//Jelly20140123
*/

#ifdef MT_DFS_SUPPORT
//Remember add RDM compiler flag - Shihwei20141104
#include "rt_config.h"
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/



/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S 
********************************************************************************
*/

/*#ifdef MT_DFS_SUPPORT
	BUILD_TIMER_FUNCTION(DfsChannelSwitchTimeOut);
#endif */
static inline BOOLEAN AutoChannelSkipListCheck(
	IN PRTMP_ADAPTER	pAd,
	IN UCHAR			Ch)
{
	UCHAR i;
	
	for (i=0; i < pAd->ApCfg.AutoChannelSkipListNum ; i++)
	{
		if (Ch == pAd->ApCfg.AutoChannelSkipList[i])
		{
			return TRUE;
		}
	}
	return FALSE;
}

static inline UCHAR Get5GPrimChannel(
    IN PRTMP_ADAPTER	pAd)
{
    
	return HcGetChannelByRf(pAd,RFIC_5GHZ);
	
}

static inline VOID Set5GPrimChannel(
    IN PRTMP_ADAPTER	pAd, UCHAR Channel)
{
	if(HcUpdateChannel(pAd,Channel) !=0)
	{
		;
	}
}

static inline UCHAR CentToPrim(
	UCHAR Channel)
{	
	return Channel-2;
}

VOID DfsGetSysParameters(
    IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	UINT_8 i;
	
#ifdef DOT11_VHT_AC		
    
	if(pAd->CommonCfg.vht_bw == VHT_BW_8080)
    {
        pDfsParam->PrimCh = Get5GPrimChannel(pAd);	
        if(Get5GPrimChannel(pAd) < CentToPrim(pAd->CommonCfg.vht_cent_ch2))
        { 
			pDfsParam->PrimBand = BAND0;
        }	
		else
		{	
			pDfsParam->PrimBand = BAND1;
		}	
		pDfsParam->Band0Ch = (pDfsParam->PrimBand == BAND0) ? Get5GPrimChannel(pAd) : CentToPrim(pAd->CommonCfg.vht_cent_ch2);
        pDfsParam->Band1Ch = (pDfsParam->PrimBand == BAND0) ? CentToPrim(pAd->CommonCfg.vht_cent_ch2) : Get5GPrimChannel(pAd);        
    }
	else
#endif
    {
        pDfsParam->PrimCh = Get5GPrimChannel(pAd);
		pDfsParam->PrimBand = BAND0;
	    pDfsParam->Band0Ch= Get5GPrimChannel(pAd);
		pDfsParam->Band1Ch= 0;
    }

	pDfsParam->Bw = pAd->CommonCfg.BBPCurrentBW;
	pDfsParam->Dot11_H.RDMode = pAd->Dot11_H.RDMode;	
	pDfsParam->RegTxSettingBW = pAd->CommonCfg.RegTransmitSetting.field.BW;
	pDfsParam->bIEEE80211H = pAd->CommonCfg.bIEEE80211H;
	pDfsParam->ChannelListNum = pAd->ChannelListNum;
	pDfsParam->bDBDCMode = pAd->CommonCfg.dbdc_mode;
	pDfsParam->bDfsEnable = pAd->CommonCfg.DfsParameter.bDfsEnable;	
	
	for(i=0; i<pDfsParam->ChannelListNum; i++)
	{
   	    pDfsParam->DfsChannelList[i].Channel = pAd->ChannelList[i].Channel;		
        pDfsParam->DfsChannelList[i].Flags = pAd->ChannelList[i].Flags;
		pDfsParam->DfsChannelList[i].NonOccupancy = pAd->ChannelList[i].RemainingTimeForUse;
	}

}

VOID DfsParamUpdateChannelList(
	IN PRTMP_ADAPTER	pAd)
{
	UINT_8 i;
	for(i=0; i < pAd->ChannelListNum; i++)
		{
			pAd->ChannelList[i].RemainingTimeForUse = 0;
		}
}

VOID DfsParamInit(
		IN PRTMP_ADAPTER	pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	pDfsParam->PrimBand = BAND0;
	pDfsParam->DfsChBand[0] = FALSE; //Smaller channel
	pDfsParam->DfsChBand[1] = FALSE; // Larger channel number
	pDfsParam->RadarDetected[0] = FALSE; //Smaller channel number
	pDfsParam->RadarDetected[1] = FALSE; // larger channel number
	pDfsParam->RadarDetectState = FALSE;
	pDfsParam->IsSetCountryRegion = FALSE;
	pDfsParam->bDfsCheck = FALSE;
	pDfsParam->bNoSwitchCh = FALSE;
	pDfsParam->bShowPulseInfo = FALSE;
	pDfsParam->bDBDCMode = pAd->CommonCfg.dbdc_mode;
	DfsStateMachineInit(pAd, &pAd->CommonCfg.DfsParameter.DfsStatMachine, pAd->CommonCfg.DfsParameter.DfsStateFunc);
}

VOID DfsStateMachineInit(
	IN RTMP_ADAPTER *pAd,
	IN STATE_MACHINE *Sm,
	OUT STATE_MACHINE_FUNC Trans[])
{
	StateMachineInit(Sm, (STATE_MACHINE_FUNC *)Trans, DFS_MAX_STATE, DFS_MAX_MSG, (STATE_MACHINE_FUNC)Drop, DFS_BEFORE_SWITCH, DFS_MACHINE_BASE);
	StateMachineSetAction(Sm, DFS_BEFORE_SWITCH, DFS_CAC_END, (STATE_MACHINE_FUNC)DfsCacEndUpdate);
	StateMachineSetAction(Sm, DFS_BEFORE_SWITCH, DFS_CHAN_SWITCH_TIMEOUT, (STATE_MACHINE_FUNC)DfsChannelSwitchTimeoutAction);
}

INT Set_RadarDetectStart_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg)
{
	ULONG value, ret1, ret2;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	value = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_RadarDetectStart_Proc: \n"));	

	if(value == 0)
	{	
	    ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
	    ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);
		ret1= mtRddControl(pAd, RDD_NODETSTOP, HW_RDD0, 0);
		pDfsParam->bNoSwitchCh = TRUE;
	}
	else if(value == 1)
	{	     
        ret1= mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
        ret1= mtRddControl(pAd, RDD_START, HW_RDD1, 0);
        ret1= mtRddControl(pAd, RDD_NODETSTOP, HW_RDD1, 0);
        pDfsParam->bNoSwitchCh = TRUE;
	}	
	else if(value == 2)
	{
#ifdef DOT11_VHT_AC	
        ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
		ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);
		ret1= mtRddControl(pAd, RDD_NODETSTOP, HW_RDD0, 0);
        if(pAd->CommonCfg.vht_bw == VHT_BW_8080 || pAd->CommonCfg.vht_bw == VHT_BW_160)
        {
			ret2= mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
			ret2= mtRddControl(pAd, RDD_START, HW_RDD1, 0);
			ret2= mtRddControl(pAd, RDD_NODETSTOP, HW_RDD1, 0);
        }	
		else
#endif	
  	    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_RadarDetectStart_Proc: Bandwidth not 80+80 or 160\n"));	     
        pDfsParam->bNoSwitchCh = TRUE;
	}
    else if(value == 3)
	{	
		ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
		ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);
		ret1= mtRddControl(pAd, RDD_DETSTOP, HW_RDD0, 0);
		pDfsParam->bNoSwitchCh = TRUE;
	}
    else if(value == 4)
    {
        ret1 = mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
        ret1 = mtRddControl(pAd, RDD_START, HW_RDD1, 0);
        ret1 = mtRddControl(pAd, RDD_DETSTOP, HW_RDD1, 0);
        pDfsParam->bNoSwitchCh = TRUE;
	}	
	else if(value == 5)
	{
        ret1 = mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
        ret1 = mtRddControl(pAd, RDD_START, HW_RDD0, 0);
        ret1 = mtRddControl(pAd, RDD_DETSTOP, HW_RDD0, 0);
#ifdef DOT11_VHT_AC	
		if(pAd->CommonCfg.vht_bw == VHT_BW_8080 || pAd->CommonCfg.vht_bw == VHT_BW_160)
		{
			ret2 = mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
			ret2 = mtRddControl(pAd, RDD_START, HW_RDD1, 0);
			ret2 = mtRddControl(pAd, RDD_DETSTOP, HW_RDD1, 0);
		}	
		else
#endif	
		    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_RadarDetectStart_Proc: Bandwidth not 80+80 or 160\n"));		 
	    pDfsParam->bNoSwitchCh = TRUE;
	}		

	else
		;
	return TRUE;
}


INT Set_RadarDetectStop_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg)
{
		ULONG value, ret1, ret2;
		value = simple_strtol(arg, 0, 10);
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_RadarDetectStop_Proc: \n")); 

		if(value == 0)
			ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
		else if(value == 1)
		{
			ret1= mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
		}	
		else if(value == 2)
		{
		    ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
#ifdef DOT11_VHT_AC	
			if(pAd->CommonCfg.vht_bw == VHT_BW_8080 || pAd->CommonCfg.vht_bw == VHT_BW_160)
			{
				ret2= mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);
			}	
			else
#endif	
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_RadarDetectStop_Proc: Bandwidth not 80+80 or 160\n"));		 
		}		
		else
			;
		return TRUE;
}

INT Set_ByPassCac_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg)
{
	ULONG value; //CAC time
	value = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_ByPassCac_Proc\n"));		 

	pAd->Dot11_H.ChMovingTime = value;
	return TRUE;
}

INT Set_RDDReport_Proc(
    RTMP_ADAPTER *pAd, 
    RTMP_STRING *arg)
{
	ULONG value;
	value = simple_strtol(arg, 0, 10);
	WrapDfsRddReportHandle(pAd, value);
	return TRUE;
}

INT Set_DfsChannelShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;
	value = simple_strtol(arg, 0, 10);

    if(!HcGetChannelByRf(pAd,RFIC_5GHZ))
    {
	    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("No 5G band channel\n"));	 
	}
	else	
	{
	    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("Channel num is %d\n", 
		HcGetChannelByRf(pAd,RFIC_5GHZ)));
	}
	return TRUE;
}

INT Set_DfsBwShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;
	value = simple_strtol(arg, 0, 10);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("Current Bw is %d\n", 
	pAd->CommonCfg.BBPCurrentBW));
   
	return TRUE;
}

INT Set_DfsRDModeShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;
	value = simple_strtol(arg, 0, 10);

    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("RDMode is %d\n", 
            pAd->Dot11_H.RDMode));  

	return TRUE;
}

INT Set_DfsRDDRegionShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;
	value = simple_strtol(arg, 0, 10);
    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("RDD Region is %d\n", 
            pAd->CommonCfg.RDDurRegion));  
	return TRUE;
}

INT Set_DfsNonOccupancyShow_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;	
	UINT_8 i;
	value = simple_strtol(arg, 0, 10);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_DfsNonOccupancyShow_Proc: \n"));  
	for(i=0; i<pAd->ChannelListNum; i++)
	{
	    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("DfsChannelList[%d].Channel = %d, DfsReq = %d, NonOccupancy = %d\n",
	    i, pAd->ChannelList[i].Channel,
	    pAd->ChannelList[i].DfsReq,
	    pAd->ChannelList[i].RemainingTimeForUse));
	}
	return TRUE;    
}

INT Set_DfsPulseInfoMode_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;	
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	value = simple_strtol(arg, 0, 10);
	pDfsParam->bShowPulseInfo = TRUE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_DfsPulseInfoShow_Proc: \n"));  

    mtRddControl(pAd, RDD_PULSEDBG, HW_RDD0, 0); 
	return TRUE;    
}

INT Set_DfsPulseInfoRead_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN RTMP_STRING *arg)
{
	ULONG value;	
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	value = simple_strtol(arg, 0, 10);
	pDfsParam->bShowPulseInfo = FALSE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_TRACE, ("In Set_DfsPulseInfoRead_Proc: \n"));  

	MtCmdFwLog2Host(pAd, 0, 2);
	mtRddControl(pAd, RDD_READPULSE, HW_RDD0, 0); 
	MtCmdFwLog2Host(pAd, 0, 0);
	
	if(pAd->Dot11_H.RDMode == RD_NORMAL_MODE)
	{
       pAd->Dot11_H.RDMode = RD_SILENCE_MODE;
	   pAd->Dot11_H.ChMovingTime = 5;
	}
	WrapDfsRadarDetectStart(pAd);
	return TRUE;    
}

BOOLEAN DfsEnterSilence(
    IN PRTMP_ADAPTER pAd)
{
    PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	return pDfsParam->bDfsEnable;
}

VOID DfsSetCalibration(
	    IN PRTMP_ADAPTER pAd, UINT_32 DisableDfsCal)
{
	if(!DisableDfsCal)
	    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_OFF, ("Enable DFS calibration in firmware. \n"));				
	else
	{
	    MTWF_LOG(DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_OFF, ("Disable DFS calibration in firmware. \n"));
        mtRddControl(pAd, RDD_DFSCAL, HW_RDD0, 0);    
	}	
}

BOOLEAN DfsRadarChannelCheck(
    IN PRTMP_ADAPTER pAd)
{
    PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
    DfsGetSysParameters(pAd); 
	if(!pDfsParam->bDfsEnable)
	    return FALSE;	
#ifdef DOT11_VHT_AC
    if(pAd->CommonCfg.vht_bw == VHT_BW_8080)
    {
		return (RadarChannelCheck(pAd, pDfsParam->Band0Ch) || RadarChannelCheck(pAd, pDfsParam->Band1Ch));   		
    }
	else
#endif
    return RadarChannelCheck(pAd, pDfsParam->Band0Ch);
}

VOID DfsSetCountryRegion(
    IN PRTMP_ADAPTER pAd)
{
    PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;	
	pDfsParam->IsSetCountryRegion = TRUE;
}

VOID DfsCacEndUpdate(
	RTMP_ADAPTER *pAd,
	MLME_QUEUE_ELEM *Elem)
{
	mtRddControl(pAd, RDD_CACEND, HW_RDD0, 0);		
}

VOID DfsChannelSwitchTimeoutAction(
	RTMP_ADAPTER *pAd,
	MLME_QUEUE_ELEM *Elem)
{
    //UINT32 i;
    //MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Timeout Channel Switch\n"));
    APStop(pAd);
    APStartUp(pAd);

#ifdef MT_DFS_SUPPORT
        if (pAd->CommonCfg.dbdc_mode)
        {
                MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Dot11HCntDownTimeoutAction. SetDfsTxStart\n"));

                MtCmdSetDfsTxStart(pAd, BAND1);
        }
        else
                MtCmdSetDfsTxStart(pAd, BAND0);
#endif	
}

VOID DfsCacNormalStart(
    IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;	
    DfsGetSysParameters(pAd);
	if ((pAd->CommonCfg.RDDurRegion == CE) && DfsCacRestrictBand(pAd, pDfsParam))
		pAd->Dot11_H.ChMovingTime = 605;
	else
		pAd->Dot11_H.ChMovingTime = 65;

    if(pAd->Dot11_H.RDMode == RD_SILENCE_MODE)
    {
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[APStartUp][MT_DFS]CAC time start !!!!!\n\n\n\n"));
        mtRddControl(pAd, RDD_CACSTART, HW_RDD0, 0);
    }
	else if(pAd->Dot11_H.RDMode == RD_NORMAL_MODE)
    {
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[APStartUp][MT_DFS]Normal start !!!!!\n\n\n\n"));
        mtRddControl(pAd, RDD_NORMALSTART, HW_RDD0, 0);
    }
	else
		;
}

BOOLEAN DfsCacRestrictBand(
	IN PRTMP_ADAPTER pAd, IN PDFS_PARAM pDfsParam)
{
#ifdef DOT11_VHT_AC
    if(pAd->CommonCfg.vht_bw == VHT_BW_8080)
    {
		return (RESTRICTION_BAND_1(pAd, pDfsParam->Band0Ch) || RESTRICTION_BAND_1(pAd, pDfsParam->Band1Ch));   		
    }
	else if((pAd->CommonCfg.vht_bw == VHT_BW_160) && (pDfsParam->Band0Ch >= GROUP3_LOWER && pDfsParam->Band0Ch <= RESTRICTION_BAND_HIGH))    
        return TRUE;
	else
#endif   
        return RESTRICTION_BAND_1(pAd, pDfsParam->Band0Ch);
}
VOID DfsSaveNonOccupancy(
    IN PRTMP_ADAPTER pAd)
{
	UINT_8 i;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	if(pDfsParam->IsSetCountryRegion == FALSE)
	{
        for(i=0; i<pAd->ChannelListNum; i++)
        {
            pDfsParam->DfsChannelList[i].NonOccupancy = pAd->ChannelList[i].RemainingTimeForUse;
		}
	}
}

VOID DfsRecoverNonOccupancy(
    IN PRTMP_ADAPTER pAd)
{
	UINT_8 i;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	if(pDfsParam->IsSetCountryRegion == FALSE)
	{
        for(i=0; i<pAd->ChannelListNum; i++)
        {
            pAd->ChannelList[i].RemainingTimeForUse = pDfsParam->DfsChannelList[i].NonOccupancy;

        }
	}
    pDfsParam->IsSetCountryRegion = FALSE;
}

BOOLEAN DfsSwitchCheck(
		IN PRTMP_ADAPTER	pAd)
{		
	if (pAd->Dot11_H.RDMode == RD_SILENCE_MODE)
	{
	    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSwitchCheck]: Silence mode. TRUE\n"));
	    return TRUE;
	}	
	else
	{
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSwitchCheck]: Normal mode. FALSE\n"));
		return FALSE;
	}	

}

VOID DfsNonOccupancyCountDown( /*RemainingTimeForUse --*/
	IN PRTMP_ADAPTER pAd)
{
	UINT_8 i;
	for (i=0; i<pAd->ChannelListNum; i++)
	{
		if (pAd->ChannelList[i].RemainingTimeForUse > 0)
		{
			pAd->ChannelList[i].RemainingTimeForUse --;	
		}
	}
}

VOID WrapDfsSetNonOccupancy( /*Set Channel non-occupancy time */
	IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;	
    DfsGetSysParameters(pAd);

	DfsSetNonOccupancy(pAd, pDfsParam);	
}

VOID DfsSetNonOccupancy( /*Set Channel non-occupancy time */
	IN PRTMP_ADAPTER pAd, IN PDFS_PARAM pDfsParam)
{
    UINT_8 i;
	if(pDfsParam->Dot11_H.RDMode == RD_SWITCHING_MODE) 
        return;	
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("\n[DfsSetNonOccupancy]: pDfsParam->Bw is %d\n", pDfsParam->Bw));	
	if((pDfsParam->Bw == BW_160) && (pDfsParam->DfsChBand[0] || pDfsParam->DfsChBand[1]))
	{
	    for(i=0; i<pDfsParam->ChannelListNum; i++)
	    {
			if(vht_cent_ch_freq(pDfsParam->DfsChannelList[i].Channel, VHT_BW_160) == vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_160))	
			    pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;	
		}
	}
	if((pDfsParam->Bw == BW_80) && pDfsParam->DfsChBand[0])
	{
	    for(i=0; i<pDfsParam->ChannelListNum; i++)
	    {
			if(vht_cent_ch_freq(pDfsParam->DfsChannelList[i].Channel, VHT_BW_80) == vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_80))	
			    pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;	
		}
	}
	else if(pDfsParam->Bw == BW_40 && pDfsParam->DfsChBand[0])
	{
	    for(i=0; i<pDfsParam->ChannelListNum; i++)
	    {
	        if((pDfsParam->Band0Ch == pDfsParam->DfsChannelList[i].Channel))
	        {
				pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;
			}
			else if(((pDfsParam->Band0Ch)>>2 & 1) && ((pDfsParam->DfsChannelList[i].Channel-pDfsParam->Band0Ch)==4))
			{
				pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;
			}
			else if(!((pDfsParam->Band0Ch)>>2 & 1) && ((pDfsParam->Band0Ch-pDfsParam->DfsChannelList[i].Channel)==4))
			{
				pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;
			}
			else
			    ;
		}		
	}
	else if(pDfsParam->Bw == BW_20 && pDfsParam->DfsChBand[0])
	{
        for(i=0; i<pDfsParam->ChannelListNum; i++)
        {
            if((pDfsParam->Band0Ch == pDfsParam->DfsChannelList[i].Channel))
            {
				pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;
			}
		}
	}
	else if(pDfsParam->Bw == BW_8080 && pDfsParam->DfsChBand[0] && pDfsParam->RadarDetected[0])
	{        
        for(i=0; i<pDfsParam->ChannelListNum; i++)
        {
            if(vht_cent_ch_freq(pDfsParam->DfsChannelList[i].Channel, VHT_BW_8080) == vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_8080))	
            {
                pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;				    
            }
        }	
	}

    else if(pDfsParam->Bw == BW_8080 && pDfsParam->DfsChBand[1] && pDfsParam->RadarDetected[1])
    {
	    for(i=0; i<pDfsParam->ChannelListNum; i++)
		{
		    if(vht_cent_ch_freq(pDfsParam->DfsChannelList[i].Channel, VHT_BW_8080) == vht_cent_ch_freq(pDfsParam->Band1Ch, VHT_BW_8080))	
		    {
			    pAd->ChannelList[i].RemainingTimeForUse = CHAN_NON_OCCUPANCY;					
			}
		}	
	}
			
	else
	    ;	
}

VOID WrapDfsRddReportHandle( /*handle the event of EXT_EVENT_ID_RDD_REPORT*/
	IN PRTMP_ADAPTER pAd, UCHAR ucRddIdx)
{
	UCHAR PhyMode;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;

	pDfsParam->Dot11_H.RDMode = pAd->Dot11_H.RDMode;
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[WrapDfsRddReportHandle]:  Radar detected !!!!!!!!!!!!!!!!!\n")); 		
    
	if(pDfsParam->bNoSwitchCh)
	{
		return;
	}
	if(pDfsParam->Band0Ch > 14)
		PhyMode = HcGetPhyModeByRf(pAd,RFIC_5GHZ);
	else
		PhyMode = HcGetPhyModeByRf(pAd,RFIC_24GHZ);
	
	if(DfsRddReportHandle(pDfsParam, ucRddIdx))
    {
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[WrapDfsRddReportHandle]: pDfsParam->RadarDetected[0] is %d\n",pDfsParam->RadarDetected[0]));
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[WrapDfsRddReportHandle]: pDfsParam->RadarDetected[1] is %d\n",pDfsParam->RadarDetected[1]));
	    WrapDfsSetNonOccupancy(pAd);		
	    WrapDfsSelectChannel(pAd);

#ifdef DOT11_N_SUPPORT
	    N_ChannelCheck(pAd,PhyMode,pDfsParam->PrimCh);
#endif 

		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[WrapDfsRddReportHandle] RD Mode is %d\n", pDfsParam->Dot11_H.RDMode));		

        if (pDfsParam->Dot11_H.RDMode == RD_NORMAL_MODE)
        {
		    
			pDfsParam->DfsChBand[0] = FALSE;
			pDfsParam->DfsChBand[1] = FALSE;		
			pDfsParam->RadarDetected[0] = FALSE;
			pDfsParam->RadarDetected[1] = FALSE;	
			NotifyChSwAnnToPeerAPs(pAd, ZERO_MAC_ADDR, pAd->CurrentAddress, 1, pDfsParam->PrimCh);
		    pAd->Dot11_H.RDMode = RD_SWITCHING_MODE;
		    pAd->Dot11_H.CSCount = 0;
			pAd->Dot11_H.new_channel = pDfsParam->PrimCh;
			pAd->Dot11_H.org_ch = HcGetChannelByRf(pAd, RFIC_5GHZ);
			Set5GPrimChannel(pAd, pDfsParam->PrimCh);			
			if(HcUpdateCsaCntByChannel(pAd, pDfsParam->PrimCh) != 0)
            {
                ;
            }			
        }
        else if (pDfsParam->Dot11_H.RDMode == RD_SILENCE_MODE)
        {

			pDfsParam->DfsChBand[0] = FALSE;
			pDfsParam->DfsChBand[1] = FALSE;		
			pDfsParam->RadarDetected[0] = FALSE;
			pDfsParam->RadarDetected[1] = FALSE;	
 	        Set5GPrimChannel(pAd, pDfsParam->PrimCh);
			MlmeEnqueue(pAd, DFS_STATE_MACHINE, DFS_CHAN_SWITCH_TIMEOUT, 0, NULL, 0);			
 		    RTMP_MLME_HANDLER(pAd);
        }
	}	
}

BOOLEAN DfsRddReportHandle( /*handle the event of EXT_EVENT_ID_RDD_REPORT*/
	IN PDFS_PARAM pDfsParam, UCHAR ucRddIdx)
{
    BOOLEAN RadarDetected = FALSE;
    if(ucRddIdx == 0 && (pDfsParam->RadarDetected[0] == FALSE) && (pDfsParam->DfsChBand[0])
	&& (pDfsParam->Dot11_H.RDMode != RD_SWITCHING_MODE))
    {    
        pDfsParam->RadarDetected[0] = TRUE;	
		RadarDetected = TRUE;
		if(pDfsParam->Bw == BW_160 
		&&(pDfsParam->Band0Ch >= GROUP1_LOWER && pDfsParam->Band0Ch <= GROUP2_UPPER))
		{
            pDfsParam->RadarDetected[0] = FALSE;	
		    RadarDetected = FALSE;
		}
    }	

#ifdef DOT11_VHT_AC	
    if(ucRddIdx == 1 && (pDfsParam->RadarDetected[1] == FALSE) && (pDfsParam->DfsChBand[1])
	&&(pDfsParam->Dot11_H.RDMode != RD_SWITCHING_MODE))
    {
        pDfsParam->RadarDetected[1] = TRUE;		
        RadarDetected = TRUE;
	}
#endif
    if(pDfsParam->bDBDCMode)
    {
		if(ucRddIdx == 1 && (pDfsParam->RadarDetected[1] == FALSE) && (pDfsParam->DfsChBand[0])
		&&(pDfsParam->Dot11_H.RDMode != RD_SWITCHING_MODE))
		{
			pDfsParam->RadarDetected[1] = TRUE; 	
			RadarDetected = TRUE;
		}
	}
    return RadarDetected;
}

VOID DfsCacEndHandle( /*handle the event of EXT_EVENT_ID_CAC_END*/
	IN PRTMP_ADAPTER pAd, UCHAR ucRddIdx)
{
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("DfsCacEndHandle]:\n")); 	
}

VOID WrapDfsSelectChannel(  /*Select new channel*/
	IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;

    DfsGetSysParameters(pAd);
	DfsSelectChannel(pAd,pDfsParam);
	
#ifdef DOT11_VHT_AC		
    
	if(pAd->CommonCfg.vht_bw == VHT_BW_8080)
    {
	
        if(pDfsParam->PrimBand == BAND0)
        { 
			pAd->CommonCfg.vht_cent_ch2 
		    = vht_cent_ch_freq(pDfsParam->Band1Ch, VHT_BW_8080);//Central channel 2;
        }	
		else
		{	
			pAd->CommonCfg.vht_cent_ch2 
		    = vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_8080);//Central channel 2;;
		}	
    }

#endif
	
}

VOID DfsSelectChannel( /*Select new channel*/
	IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam)
{
    UCHAR tempCh=0;
    if(pDfsParam->Bw == BW_8080)
    {
	    if(pDfsParam->Band0Ch < pDfsParam->Band1Ch)
	    { 
	        if(pDfsParam->RadarDetected[0] && pDfsParam->DfsChBand[0])
            {
                pDfsParam->Band0Ch = 124; //WrapDfsRandomSelectChannel(pAd);
                while(vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_8080) 
				   == vht_cent_ch_freq(pDfsParam->Band1Ch, VHT_BW_8080))
				{
				    pDfsParam->Band0Ch = WrapDfsRandomSelectChannel(pAd);
				}
	        }
	        if(pDfsParam->RadarDetected[1] && pDfsParam->DfsChBand[1])
            {
                pDfsParam->Band1Ch = WrapDfsRandomSelectChannel(pAd);
				while(vht_cent_ch_freq(pDfsParam->Band1Ch, VHT_BW_8080) 
				   == vht_cent_ch_freq(pDfsParam->Band0Ch, VHT_BW_8080))
				{
				    pDfsParam->Band1Ch = WrapDfsRandomSelectChannel(pAd);
				}	                
	        }
        }	
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSelectChannel]: 80+80MHz band, selected is %d, %d\n",pDfsParam->Band0Ch, pDfsParam->Band1Ch));

        if(pDfsParam->PrimBand == BAND0)
        {
		    pDfsParam->PrimCh = pDfsParam->Band0Ch;		
        }
		else
		{
            pDfsParam->PrimCh = pDfsParam->Band1Ch; 
		}
		if(pDfsParam->Band1Ch < pDfsParam->Band0Ch)
		{
            tempCh = pDfsParam->Band1Ch;
			pDfsParam->Band1Ch = pDfsParam->Band0Ch;
			pDfsParam->Band0Ch = tempCh;
		}
		if(pDfsParam->PrimCh == pDfsParam->Band0Ch)
		{
            pDfsParam->PrimBand = BAND0;
		}
		else
		{
			pDfsParam->PrimBand = BAND1;
		}
			
    }

    else
    {
    	if((pDfsParam->Bw == BW_160) 
		&& ((pDfsParam->RadarDetected[0] && pDfsParam->DfsChBand[0]) 
		    || (pDfsParam->RadarDetected[1] && pDfsParam->DfsChBand[1])))
    	{
		    pDfsParam->Band0Ch = WrapDfsRandomSelectChannel(pAd);
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSelectChannel]: Single band, selected is %d\n",pDfsParam->Band0Ch));					    
		}
	    else if(pDfsParam->RadarDetected[0] && pDfsParam->DfsChBand[0])
  	    {
			pDfsParam->Band0Ch = WrapDfsRandomSelectChannel(pAd);	
			MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSelectChannel]: Single band, selected is %d\n",pDfsParam->Band0Ch));
	    }
		else
            ;	
        if(pDfsParam->bDBDCMode)
        {
		    if(pDfsParam->RadarDetected[1] && pDfsParam->DfsChBand[0])
            {
			    pDfsParam->Band0Ch = WrapDfsRandomSelectChannel(pAd);	
                MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsSelectChannel]: DBDC band, selected is %d\n",pDfsParam->Band0Ch));            
		    }
        }	

    pDfsParam->PrimCh = pDfsParam->Band0Ch;
    pDfsParam->PrimBand = BAND0;
    }	
    
}

UCHAR WrapDfsRandomSelectChannel( /*Select new channel using random selection*/
	IN PRTMP_ADAPTER pAd)
{
	//UINT_8 i;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	DfsGetSysParameters(pAd);

	return DfsRandomSelectChannel(pAd, pDfsParam);
}


UCHAR DfsRandomSelectChannel( /*Select new channel using random selection*/
	IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam)
{

	UINT_8 i, cnt, ch, flag;
	UINT_8 TempChList[MAX_NUM_OF_CHANNELS] = {0};

	cnt = 0;
	
	if(pDfsParam->bIEEE80211H)		
	{
	    for (i = 0; i < pDfsParam->ChannelListNum; i++)
	    {
	        if (pDfsParam->DfsChannelList[i].NonOccupancy)
			    continue;
		    if (AutoChannelSkipListCheck(pAd, pDfsParam->DfsChannelList[i].Channel) == TRUE)
		        continue;
			if(pDfsParam->bDBDCMode)
			{
			    if (HcGetBandByChannel(pAd, pDfsParam->DfsChannelList[i].Channel) == 0)
				    continue;
			}	
	
#ifdef DOT11_N_SUPPORT
		    if (pDfsParam->Bw == BW_40 
			&& (!(pDfsParam->DfsChannelList[i].Flags & CHANNEL_40M_CAP) || CH_NOSUPPORT_BW40(pDfsParam->DfsChannelList[i].Channel)))
		        continue;
#endif /* DOT11_N_SUPPORT */
	
#ifdef DOT11_VHT_AC
            if ((pDfsParam->Bw == BW_80 || pDfsParam->Bw == BW_8080) 
			&& !(pDfsParam->DfsChannelList[i].Flags & CHANNEL_80M_CAP))
                continue;
			if ((pDfsParam->Bw == BW_160) && (pDfsParam->DfsChannelList[i].Channel > UPPER_BW_160(pAd)))
				continue;
#endif /* DOT11_VHT_AC */

		/* Store available channel to temp list */
		    TempChList[cnt++] = pDfsParam->DfsChannelList[i].Channel;
	    }
		if(cnt)
		{
		    ch = TempChList[RandomByte(pAd)%cnt];
		}
		else
		{			
            USHORT MinTime = 0xFFFF;
			ch = 0;
		    flag = 0; 
			for(i=0; i<pDfsParam->ChannelListNum; i++)
			{
                if(pDfsParam->DfsChannelList[i].NonOccupancy < MinTime)
                {
				    MinTime = pDfsParam->DfsChannelList[i].NonOccupancy;
					ch = pDfsParam->DfsChannelList[i].Channel;
					flag = pDfsParam->DfsChannelList[i].Flags;
				}
			}
		}
	}

    else
    {
        ch = pDfsParam->DfsChannelList[RandomByte(pAd)%pDfsParam->ChannelListNum].Channel;
		if(ch==0)
		{
		    ch = pDfsParam->DfsChannelList[0].Channel;
		}
	}

    return ch;

}

VOID WrapDfsRadarDetectStart( /*Start Radar Detection or not*/
	IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	if(pDfsParam->bShowPulseInfo)
	    return;	

	DfsGetSysParameters(pAd);
    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("[WrapDfsRadarDetectStart]: Band0Ch is %d", pDfsParam->Band0Ch));
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("[WrapDfsRadarDetectStart]: Band1Ch is %d", pDfsParam->Band1Ch));
	pDfsParam->DfsChBand[0] = RadarChannelCheck(pAd, pDfsParam->Band0Ch);	

#ifdef DOT11_VHT_AC	
	if(pAd->CommonCfg.vht_bw == VHT_BW_8080)
	{
        pDfsParam->DfsChBand[1] = RadarChannelCheck(pAd, pDfsParam->Band1Ch);
	}
	if(pAd->CommonCfg.vht_bw == VHT_BW_160)
	{
        pDfsParam->DfsChBand[1] = pDfsParam->DfsChBand[0];	
	}
#endif

    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[WrapDfsRadarDetectStart]: "));
    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Bandwidth: %d, RDMode: %d\n"
	,pDfsParam->Bw, pDfsParam->Dot11_H.RDMode));
	DfsRadarDetectStart(pAd, pDfsParam);
}

VOID DfsRadarDetectStart( /*Start Radar Detection or not*/
   IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam)
{
    INT ret1 = TRUE;	
	if(ScanRunning(pAd) || (pDfsParam->Dot11_H.RDMode == RD_SWITCHING_MODE))
	    return;	

	if(pDfsParam->Dot11_H.RDMode == RD_SILENCE_MODE)
    {
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[DfsRadarDetectStart]:start\n"));		
		if((pDfsParam->RadarDetectState == FALSE))
		{
		    if(pDfsParam->bDBDCMode)
		    {
		        ret1= mtRddControl(pAd, RDD_START, HW_RDD1, 0);
		    }	
#ifdef DOT11_VHT_AC				
			else if (pDfsParam->Bw == BW_160)
			{
			    ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);
				ret1= mtRddControl(pAd, RDD_START, HW_RDD1, 0);
			}
            else if(pDfsParam->Bw == BW_8080)
            {
                if(pDfsParam->DfsChBand[0])
				    ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);				
				if(pDfsParam->DfsChBand[1])
					ret1= mtRddControl(pAd, RDD_START, HW_RDD1, 0);				
            }
#endif			
			else
			{
               ret1= mtRddControl(pAd, RDD_START, HW_RDD0, 0);
			}				
		}
	    pDfsParam->RadarDetectState = TRUE;
	}	
}

VOID WrapDfsRadarDetectStop( /*Start Radar Detection or not*/
	IN PRTMP_ADAPTER pAd)
{
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	DfsGetSysParameters(pAd);
	DfsRadarDetectStop(pAd, pDfsParam);
}

VOID DfsRadarDetectStop( /*Start Radar Detection or not*/
   IN PRTMP_ADAPTER pAd, PDFS_PARAM pDfsParam)
{
    INT ret1 = TRUE, ret2 = TRUE;	
	pDfsParam->RadarDetectState = FALSE;
	if(!pDfsParam->bDfsEnable)
	    return;	
	ret1= mtRddControl(pAd, RDD_STOP, HW_RDD0, 0);
	ret2= mtRddControl(pAd, RDD_STOP, HW_RDD1, 0);		
}
 
/*----------------------------------------------------------------------------*/
/*!
* \brief     Configure (Enable/Disable) HW RDD and RDD wrapper module
*
* \param[in] ucRddCtrl
*            ucRddIdex
*
*
* \return    None
*/
/*----------------------------------------------------------------------------*/

INT mtRddControl(
        IN struct _RTMP_ADAPTER *pAd,
        IN UCHAR ucRddCtrl,
        IN UCHAR ucRddIdex,
        IN UCHAR ucRddInSel)
{
    INT ret = TRUE;

    MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[mtRddControl]RddCtrl=%d, RddIdx=%d, RddInSel=%d\n", ucRddCtrl, ucRddIdex, ucRddInSel));
    ret = MtCmdRddCtrl(pAd, ucRddCtrl, ucRddIdex, ucRddInSel);
	MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("[mtRddControl]complete\n"));

    return ret;
}
#endif /*MT_DFS_SUPPORT*/

