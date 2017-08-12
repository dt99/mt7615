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
    bcn.c

    Abstract:
    separate Bcn related function

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Carter      2014-1121     created for all interface could send bcn.

 */

#include "rt_config.h"

UCHAR PowerConstraintIE[3] = {IE_POWER_CONSTRAINT, 1, 3};

#ifdef MT_MAC
VOID static mt_asic_write_bcn_buf(RTMP_ADAPTER *pAd, UCHAR *tmac_info, INT info_len, UCHAR *bcn_buf, INT buf_len, UINT32 hw_addr)
{
#ifdef RT_BIG_ENDIAN
    MTMacInfoEndianChange(pAd, tmac_info, TYPE_TXWI, sizeof(TMAC_TXD_L));

    /* update BEACON frame content. start right after the mac_info field. */
    RTMPFrameEndianChange(pAd, bcn_buf, DIR_WRITE, FALSE);
#endif
    // TODO: shiang-MT7603, Send to ASIC!
}
#endif /* MT_MAC */


#if defined(RTMP_MAC) || defined(RLT_MAC)
VOID static rt_asic_write_bcn_buf(RTMP_ADAPTER *pAd, UCHAR *tmac_info, INT info_len, UCHAR *bcn_buf, INT buf_len, UINT32 hw_addr)
{
    INT i;
    UCHAR *ptr;
    UINT32 longValue, reg_base = hw_addr;

#ifdef RT_BIG_ENDIAN
    RTMPWIEndianChange(pAd, tmac_info, TYPE_TXWI);
    RTMPFrameEndianChange(pAd, bcn_buf, DIR_WRITE, FALSE);
#endif

    ptr = tmac_info;
    for (i = 0; i < info_len; i += 4)
    {
        longValue = *ptr + (*(ptr+1)<<8) + (*(ptr+2)<<16) + (*(ptr+3)<<24);
        RTMP_CHIP_UPDATE_BEACON(pAd, reg_base + i, longValue, 4);
        ptr += 4;
    }

    /* update BEACON frame content. start right after the TXWI field. */
    ptr = bcn_buf;
    reg_base = hw_addr + info_len;
    for (i= 0; i< buf_len; i+=4)
    {
        longValue =  *ptr + (*(ptr+1)<<8) + (*(ptr+2)<<16) + (*(ptr+3)<<24);
        RTMP_CHIP_UPDATE_BEACON(pAd, reg_base + i, longValue, 4);
        ptr += 4;
    }
}
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */

VOID asic_write_bcn_buf(RTMP_ADAPTER *pAd, UCHAR *tmac_info, INT info_len, UCHAR *bcn_buf, INT buf_len, UINT32 hw_addr)
{
#ifdef MT_MAC
    if (pAd->chipCap.hif_type == HIF_MT)
    {
        mt_asic_write_bcn_buf(pAd, tmac_info, info_len, bcn_buf, buf_len, hw_addr);
    }
#endif /* MT_MAC */

#if defined(RTMP_MAC) || defined(RLT_MAC)
    if (pAd->chipCap.hif_type == HIF_RTMP || pAd->chipCap.hif_type == HIF_RLT)
    {
        rt_asic_write_bcn_buf(pAd, tmac_info, info_len, bcn_buf, buf_len, hw_addr);
    }
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */
}

VOID write_tmac_info_beacon(
        RTMP_ADAPTER *pAd,
        struct wifi_dev *wdev,
        UCHAR *tmac_buf,
        HTTRANSMIT_SETTING *BeaconTransmit,
        ULONG frmLen
)
{
#ifdef MT_MAC
    if (pAd->chipCap.hif_type == HIF_MT)
    {
        mt_write_tmac_info_beacon(pAd, wdev, tmac_buf, BeaconTransmit, frmLen);
    }
#endif /* MT_MAC */

#if defined(RTMP_MAC) || defined(RLT_MAC)
    if (pAd->chipCap.hif_type == HIF_RTMP || pAd->chipCap.hif_type == HIF_RLT)
    {
        rt_write_tmac_info_beacon(pAd, wdev, tmac_buf, BeaconTransmit, frmLen);
    }
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */
}


/*
    ==========================================================================
    Description:
        Used to check the necessary to send Beancon.
    return value
        0: mean no necessary.
        0: mean need to send Beacon for the service.
    ==========================================================================
*/
BOOLEAN BeaconTransmitRequired(RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
    BOOLEAN result = FALSE;
    BCN_BUF_STRUC *bcn_info = &wdev->bcn_buf;
#ifdef DOT11K_RRM_SUPPORT
#ifdef QUIET_SUPPORT
    UCHAR apidx = wdev->func_idx;
#endif /* QUIET_SUPPORT */
#endif /* DOT11K_RRM_SUPPORT */

    if (!WDEV_WITH_BCN_ABILITY(wdev->wdev_type))
        return result;

    if (bcn_info == NULL)
        return result;

    if (bcn_info->BeaconPkt == NULL) {
        MTWF_LOG(
                DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s(): no BeaconPkt\n",
                    __FUNCTION__));
        return result;
    }

    do
    {
#ifdef CONFIG_AP_SUPPORT
#ifdef WDS_SUPPORT
        if (pAd->WdsTab.Mode == WDS_BRIDGE_MODE)
            break;
#endif /* WDS_SUPPORT */
#ifdef CARRIER_DETECTION_SUPPORT
        if (isCarrierDetectExist(pAd) == TRUE)
            break;
#endif /* CARRIER_DETECTION_SUPPORT */
#ifdef DOT11K_RRM_SUPPORT
#ifdef QUIET_SUPPORT
        if ((apidx < pAd->ApCfg.BssidNum) && IS_RRM_QUIET(pAd, apidx))
            break;
#endif /* QUIET_SUPPORT */
#endif /* DOT11K_RRM_SUPPORT */
#endif /*CONFIG_AP_SUPPORT */

#if defined(RTMP_MAC) || defined(RLT_MAC)
        if (bcn_info->BcnBufIdx >= HW_BEACON_MAX_NUM)
            break;
#endif
#if !defined(MT7615) && !defined(MT7622)
            if (wdev->wdev_type == WDEV_TYPE_AP)
            {
                RTMP_SEM_LOCK(&pAd->BcnRingLock);
                if (wdev->bcn_buf.bcn_state != BCN_TX_IDLE)
                {
                    if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
                    {
                        MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                            ("%s(): wdev->OmacIdx = %x, != BCN_TX_IDLE\n", __func__, wdev->OmacIdx));
                    }
                    RTMP_SEM_UNLOCK(&pAd->BcnRingLock);
                    result = FALSE;
                    break;
                }
                RTMP_SEM_UNLOCK(&pAd->BcnRingLock);
            }
#endif /* !defined(MT7615) && !defined(MT7622) */
        if (bcn_info->bBcnSntReq == TRUE)
        {
            result = TRUE;
            break;
        }
    }
    while (FALSE);

    return result;
}

INT bcn_buf_init(RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    BCN_BUF_STRUC *bcn_info = &wdev->bcn_buf;

    /*this value with meaning for RT chip. for MT, it's doesn't matter.*/
    bcn_info->BcnBufIdx = HW_BEACON_MAX_NUM;
    bcn_info->cap_ie_pos = 0;
    bcn_info->pWdev = wdev;

    NdisAllocateSpinLock(pAd, &bcn_info->BcnContentLock);

    if (!bcn_info->BeaconPkt)
    {
        Status = RTMPAllocateNdisPacket(pAd, &bcn_info->BeaconPkt, NULL, 0, NULL, MAX_BEACON_LENGTH);
        if (Status == NDIS_STATUS_FAILURE)
            return Status;
    }
    else
    {
        MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s():BcnPkt is allocated!\n", __FUNCTION__));
    }

#ifdef MT_MAC
    if (pAd->chipCap.hif_type == HIF_MT)
    {
        RTMP_SEM_LOCK(&pAd->BcnRingLock);
        bcn_info->bcn_state = BCN_TX_IDLE;
        RTMP_SEM_UNLOCK(&pAd->BcnRingLock);
#ifdef BCN_OFFLOAD_SUPPORT
        if (pAd->chipCap.fgBcnOffloadSupport == TRUE)
        {
            bcn_info->BcnUpdateMethod = BCN_GEN_BY_FW;
            bcn_info->archUpdateBeaconToAsic = RT28xx_UpdateBcnAndTimToMcu;
        }
        else
#endif /* BCN_OFFLOAD_SUPPORT */
        {
            bcn_info->BcnUpdateMethod = BCN_GEN_BY_HOST_IN_PRETBTT;
            bcn_info->archUpdateBeaconToAsic = RT28xx_UpdateBeaconToAsic;
        }
    }
    else
#endif /* MT_MAC */
    {
        bcn_info->BcnUpdateMethod = BCN_GEN_BY_HW_SHARED_MEM;//RT chip method.
        bcn_info->archUpdateBeaconToAsic = RT28xx_UpdateBeaconToAsic;
    }

    return Status;
}

INT bcn_buf_deinit(RTMP_ADAPTER *pAd, BCN_BUF_STRUC *bcn_info)
{
#ifdef MT_MAC
    if (pAd->chipCap.hif_type == HIF_MT)
    {
        RTMP_SEM_LOCK(&pAd->BcnRingLock);
        if (bcn_info->bcn_state != BCN_TX_IDLE) {
            MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s(): Bcn not in idle(%d) when try to free it!\n",
                        __FUNCTION__, bcn_info->bcn_state));
        }
        bcn_info->bcn_state = BCN_TX_UNINIT;
        RTMP_SEM_UNLOCK(&pAd->BcnRingLock);
    }
#endif /* MT_MAC */

    if (bcn_info->BeaconPkt) {
        RTMP_SEM_LOCK(&bcn_info->BcnContentLock);
        RTMPFreeNdisPacket(pAd, bcn_info->BeaconPkt);
        bcn_info->BeaconPkt = NULL;

		if (bcn_info->tim_buf.TimPkt)
		{
			RTMPFreeNdisPacket(pAd, bcn_info->tim_buf.TimPkt);
			bcn_info->tim_buf.TimPkt = NULL;
		}

        RTMP_SEM_UNLOCK(&bcn_info->BcnContentLock);
    }

    NdisFreeSpinLock(&bcn_info->BcnContentLock);

    return TRUE;
}

#if defined(RTMP_MAC) || defined(RLT_MAC)
/*
    ==========================================================================
    Description:
        Pre-build All BEACON frame in the shared memory
    ==========================================================================
*/
static UCHAR GetBcnNum(RTMP_ADAPTER *pAd)
{
    int i;
    int NumBcn;
    BCN_BUF_STRUC *bcn_info;

    NumBcn = 0;
    for (i=0; i<pAd->ApCfg.BssidNum; i++)
    {
        bcn_info = &pAd->ApCfg.MBSSID[i].wdev.bcn_buf;
        if (bcn_info->bBcnSntReq)
        {
            bcn_info->BcnBufIdx = NumBcn;
            NumBcn ++;
        }
    }


    return NumBcn;
}
#endif /*defined(RTMP_MAC) || defined(RLT_MAC)*/

INT clearHwBcnTxMem(RTMP_ADAPTER *pAd)
{
    INT NumOfBcns = 0;

#if defined(RTMP_MAC) || defined(RLT_MAC)
    /* choose the Beacon number */
    NumOfBcns = GetBcnNum(pAd);

    if ((pAd->chipCap.hif_type == HIF_RTMP) || (pAd->chipCap.hif_type == HIF_RLT)) {
        INT i, j;

        /*
            before MakeBssBeacon, clear all beacon TxD's valid bit

            Note: can not use MAX_MBSSID_NUM here, or
                1. when MBSS_SUPPORT is enabled;
                2. MAX_MBSSID_NUM will be 8;
                3. if HW_BEACON_OFFSET is 0x0200,
            we will overwrite other shared memory of chip.

            use pAd->ApCfg.BssidNum to avoid the case is best
        */
        UINT8 TXWISize = pAd->chipCap.TXWISize;

        for (i=0; i<HW_BEACON_MAX_COUNT(pAd); i++)
        {
            for (j=0; j < TXWISize; j+=4)
            {
                RTMP_CHIP_UPDATE_BEACON(pAd, pAd->BeaconOffset[i] + j, 0, 4);
            }
        }
    }
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */
    return NumOfBcns;
}

VOID transmitBcnJudgemnet(
    struct _RTMP_ADAPTER *pAd,
    struct wifi_dev *wdev,
    ULONG FrameLen,
    ULONG UpdatePos,
    UCHAR UpdatePktType,
    BOOLEAN UpdateRoutine
)
{
    BCN_BUF_STRUC *bcn_info = &wdev->bcn_buf;

    if (bcn_info->BcnUpdateMethod == BCN_GEN_BY_FW)
    {
        /*
            Beacon is FW offload,
            we will not send template to FW in updateRoutine,
            and there shall not be updateRoutine happened in HOST.
        */
        if (UpdateRoutine == TRUE)
            return;
        else
        {
            goto transmitBcn;
        }
    }
    else
    {
        if (UpdateRoutine == TRUE)
        {
            goto transmitBcn;
        }
        else
            return;
    }

transmitBcn:
    if (bcn_info->archUpdateBeaconToAsic != NULL)
    {
        bcn_info->archUpdateBeaconToAsic(pAd, wdev, FrameLen, UpdatePos, PKT_BCN);
    }
    else
    {
        MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s(): no archUpdateBeaconToAsic\n",
                    __func__));
    }
    return;
}
/*
    ==========================================================================
    Description:
        Pre-build a BEACON frame in the shared memory
    ==========================================================================
*/
VOID MakeBeacon(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, BOOLEAN UpdateRoutine)
{
    ULONG FrameLen = 0, UpdatePos = 0;
    UCHAR *pBeaconFrame, *tmac_info;
    HTTRANSMIT_SETTING BeaconTransmit = {.word = 0};   /* MGMT frame PHY rate setting when operatin at HT rate. */
    UINT8 TXWISize = pAd->chipCap.TXWISize;
    UINT8 tx_hw_hdr_len = pAd->chipCap.tx_hw_hdr_len;
    UCHAR PhyMode;
#ifdef CONFIG_AP_SUPPORT
    UCHAR *ptr = NULL;
    BSS_STRUCT *pMbss = NULL;
#endif
    BCN_BUF_STRUC *pbcn_buf = NULL;

#ifdef CONFIG_FPGA_MODE
    if (pAd->fpga_ctl.no_bcn) {
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                    ("%s():Bcn Tx is blocked!\n", __FUNCTION__));
        return;
    }

    if (pAd->fpga_ctl.fpga_on & 0x1) {
        if (pAd->fpga_ctl.tx_kick_cnt == 0)
        {
            MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                    ("%s():fpga_ctl.tx_kick_cnt = 0\n", __FUNCTION__));
            return;
        }
        else if (pAd->fpga_ctl.tx_kick_cnt < 65535)
            pAd->fpga_ctl.tx_kick_cnt--;
    }
#endif /* CONFIG_FPGA_MODE */

    if (wdev == NULL)
    {
        MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s(): no wdev\n", __func__));
        return;
    }

    pbcn_buf = &wdev->bcn_buf;

    if(!BeaconTransmitRequired(pAd, wdev)) {
        MTWF_LOG(
                DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s(): BeaconTransmitRequired wdev->OmacIdx = %x\n",
                    __func__, wdev->OmacIdx)
        );

        if (pbcn_buf && pbcn_buf->BcnUpdateMethod == BCN_GEN_BY_FW)
        {
            if (!wdev->bss_info_argument.fgInitialized)
            {   /*
                    no link to BssInfo,
                    send bcn_offload command to fw will cause fw assert.
                */
                MTWF_LOG(DBG_CAT_TX, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s(): wdev->bss_info_argument.fgInitialized != TRUE\n",
                    __func__));
                return;
            }
            /* ugly code for issue a bcn update cmd with disable parameter. */
            pbcn_buf->bBcnSntReq = FALSE;

            goto bcn_tx_judge;
        }
        else
        {
            return;
        }
    }
    RTMP_SEM_LOCK(&pbcn_buf->BcnContentLock);

    tmac_info = (UCHAR *)GET_OS_PKT_DATAPTR(pbcn_buf->BeaconPkt);
    if (pAd->chipCap.hif_type == HIF_MT)
        pBeaconFrame = (UCHAR *)(tmac_info + tx_hw_hdr_len);
    else
        pBeaconFrame = (UCHAR *)(tmac_info + TXWISize);

    //if (UpdateRoutine == FALSE)
    //{
        /* not periodically update case, need take care Header and IE which is before TIM ie. */
    FrameLen = ComposeBcnPktHead(pAd, wdev, pBeaconFrame);
    pbcn_buf->TimIELocationInBeacon = (UCHAR)FrameLen;
    //}

    UpdatePos = pbcn_buf->TimIELocationInBeacon;
    PhyMode = wdev->PhyMode;
    if (UpdateRoutine == TRUE)
        FrameLen = UpdatePos;//update routine, no FrameLen information, update it for later use.

#ifdef CONFIG_AP_SUPPORT
    if (wdev->wdev_type == WDEV_TYPE_AP)
    {
        pMbss = wdev->func_dev;
        //Tim IE, AP mode only.
        pbcn_buf->cap_ie_pos = sizeof(HEADER_802_11) + TIMESTAMP_LEN + 2;

        /*
            step 1 - update AP's Capability info, since it might be changed.
        */
        ptr = pBeaconFrame + pbcn_buf->cap_ie_pos;
        *ptr = (UCHAR)(pMbss->CapabilityInfo & 0x00ff);
        *(ptr+1) = (UCHAR)((pMbss->CapabilityInfo & 0xff00) >> 8);

        /*
            step 2 - update TIM IE
            TODO: enlarge TIM bitmap to support up to 64 STAs
            TODO: re-measure if RT2600 TBTT interrupt happens faster than BEACON sent out time
        */
        ptr = pBeaconFrame + pbcn_buf->TimIELocationInBeacon;
        FrameLen += BcnTimUpdate(pAd, wdev, ptr);
        UpdatePos = FrameLen;
    }
#endif /* CONFIG_AP_SUPPORT */

    ComposeBcnPktTail(pAd, wdev, &UpdatePos, pBeaconFrame);
    FrameLen = UpdatePos;//update newest FrameLen.

   /* step 6. Since FrameLen may change, update TXWI. */
#ifdef A_BAND_SUPPORT
    if (wdev->channel > 14) {
        BeaconTransmit.field.MODE = MODE_OFDM;
        BeaconTransmit.field.MCS = MCS_RATE_6;
    }
#endif /* A_BAND_SUPPORT */

    write_tmac_info_beacon(pAd, wdev, tmac_info, &BeaconTransmit, FrameLen);

    RTMP_SEM_UNLOCK(&pbcn_buf->BcnContentLock);

bcn_tx_judge:
    transmitBcnJudgemnet(pAd, wdev, FrameLen, UpdatePos, PKT_BCN, UpdateRoutine);
    return;
}


VOID ComposeRSNIE(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;

    ULONG TempLen = 0;
    CHAR rsne_idx = 0;
    struct _SECURITY_CONFIG *pSecConfig = &wdev->SecConfig;

    for (rsne_idx=0; rsne_idx < SEC_RSNIE_NUM; rsne_idx++)
    {
        if (pSecConfig->RSNE_Type[rsne_idx] == SEC_RSNIE_NONE)
            continue;

        MakeOutgoingFrame(pBeaconFrame+FrameLen, &TempLen,
                1, &pSecConfig->RSNE_EID[rsne_idx][0],
                1, &pSecConfig->RSNE_Len[rsne_idx],
                pSecConfig->RSNE_Len[rsne_idx], &pSecConfig->RSNE_Content[rsne_idx][0],
                END_OF_ARGS);

	FrameLen += TempLen;
    }

    *pFrameLen = FrameLen;
}

VOID ComposeWPSIE(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;

#ifdef CONFIG_AP_SUPPORT
    BSS_STRUCT *pMbss = NULL;
#if defined(HOSTAPD_SUPPORT) || defined(WSC_AP_SUPPORT)
    BOOLEAN bHasWpsIE = FALSE;
#endif
#endif

#ifdef CONFIG_AP_SUPPORT
    if (wdev->wdev_type == WDEV_TYPE_AP)
        pMbss = wdev->func_dev;

#ifdef HOSTAPD_SUPPORT
    if (pMbss->HostapdWPS && (pMbss->WscIEBeacon.ValueLen))
        bHasWpsIE = TRUE;
#endif
#endif

#ifdef WSC_AP_SUPPORT
    /* add Simple Config Information Element */
    if (((pMbss->WscControl.WscConfMode >= 1) && (pMbss->WscIEBeacon.ValueLen)))
        bHasWpsIE = TRUE;

    if (bHasWpsIE)
    {
        ULONG WscTmpLen = 0;

        MakeOutgoingFrame(pBeaconFrame+FrameLen, &WscTmpLen,
                        pMbss->WscIEBeacon.ValueLen, pMbss->WscIEBeacon.Value,
                        END_OF_ARGS);
        FrameLen += WscTmpLen;
    }

    if ((pMbss->WscControl.WscConfMode != WSC_DISABLE) &&
#ifdef DOT1X_SUPPORT
        IS_IEEE8021X_Entry(&pMbss->wdev) &&
#endif /* DOT1X_SUPPORT */
        IS_CIPHER_WEP_Entry(&pMbss->wdev))
    {
        ULONG TempLen = 0;
        UCHAR PROVISION_SERVICE_IE[7] = {0xDD, 0x05, 0x00, 0x50, 0xF2, 0x05, 0x00};
        MakeOutgoingFrame(pBeaconFrame+FrameLen,        &TempLen,
                          7,                            PROVISION_SERVICE_IE,
                          END_OF_ARGS);
        FrameLen += TempLen;
    }
#endif /* WSC_AP_SUPPORT */


    *pFrameLen = FrameLen;
}

VOID MakeErpIE(
            RTMP_ADAPTER *pAd,
            struct wifi_dev *wdev,
            ULONG *pFrameLen,
            UCHAR *pBeaconFrame
)
{
    ULONG FrameLen = *pFrameLen;
    UCHAR *ptr = NULL;

    /* fill ERP IE */
    ptr = (UCHAR *)pBeaconFrame + FrameLen;
    *ptr = IE_ERP;
    *(ptr + 1) = 1;
#ifdef CONFIG_AP_SUPPORT
    if (wdev->wdev_type == WDEV_TYPE_AP)
        *(ptr + 2) = pAd->ApCfg.ErpIeContent;
#endif
    FrameLen += 3;

    *pFrameLen = FrameLen;
}

#if defined (A_BAND_SUPPORT) && defined (CONFIG_AP_SUPPORT)
VOID MakeChSwitchAnnounceIEandExtend(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    UCHAR *ptr = NULL;
    ULONG FrameLen = *pFrameLen;
    COMMON_CONFIG *pComCfg = &pAd->CommonCfg;

#ifdef DOT11_VHT_AC
    UCHAR PhyMode = wdev->PhyMode;
#endif /*DOT11_VHT_AC*/

    wdev->bcn_buf.CsaIELocationInBeacon = FrameLen;
    ptr = pBeaconFrame + FrameLen;
    *ptr = IE_CHANNEL_SWITCH_ANNOUNCEMENT;
    *(ptr + 1) = 3;
    *(ptr + 2) = 1;
    *(ptr + 3) = wdev->channel;
    *(ptr + 4) = (pAd->Dot11_H.CSPeriod - pAd->Dot11_H.CSCount - 1);
    ptr += 5;
    FrameLen += 5;

#ifdef DOT11_N_SUPPORT
    /* Extended Channel Switch Announcement Element */
    if (pComCfg->bExtChannelSwitchAnnouncement)
    {
        HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE HtExtChannelSwitchIe;
        build_ext_channel_switch_ie(pAd, &HtExtChannelSwitchIe,wdev->channel);
        NdisMoveMemory(ptr, &HtExtChannelSwitchIe, sizeof(HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE));
        ptr += sizeof(HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE);
        FrameLen += sizeof(HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE);
    }

#ifdef DOT11_VHT_AC
    if (WMODE_CAP_AC(PhyMode)) {
        INT tp_len, wb_len = 0;
        UCHAR *ch_sw_wrapper;
        VHT_TXPWR_ENV_IE txpwr_env;

        *ptr = IE_CH_SWITCH_WRAPPER;
        ch_sw_wrapper = (UCHAR *)(ptr + 1); // reserve for length
        ptr += 2; // skip len

        if (pComCfg->RegTransmitSetting.field.BW == BW_40) {

            WIDE_BW_CH_SWITCH_ELEMENT wb_info;

            *ptr = IE_WIDE_BW_CH_SWITCH;
            *(ptr + 1) = sizeof(WIDE_BW_CH_SWITCH_ELEMENT);
            ptr += 2;
            NdisZeroMemory(&wb_info, sizeof(WIDE_BW_CH_SWITCH_ELEMENT));

	    switch(pComCfg->vht_bw){
		    case VHT_BW_2040:
			    wb_info.new_ch_width = 0;
			    break;
		    case VHT_BW_80:
			    wb_info.new_ch_width = 1;
			    wb_info.center_freq_1 = vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw);
			    wb_info.center_freq_2 = 0;
			    break;
		    case VHT_BW_160:
#ifdef DOT11_VHT_R2 
			    wb_info.new_ch_width = 1;
			    wb_info.center_freq_1 = (vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw) - 8);
			    wb_info.center_freq_2 = vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw);
#else
			    wb_info.new_ch_width = 2;
			    wb_info.center_freq_1 = vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw);
#endif /* DOT11_VHT_R2 */
			    break;
		    case VHT_BW_8080:
#ifdef DOT11_VHT_R2 
			    wb_info.new_ch_width = 1;
			    wb_info.center_freq_1 = vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw);
			    wb_info.center_freq_2 = vht_cent_ch_freq(pAd->CommonCfg.vht_cent_ch2, pComCfg->vht_bw);
#else
			    wb_info.new_ch_width = 3;
			    wb_info.center_freq_1 = vht_cent_ch_freq(wdev->channel, pComCfg->vht_bw);
			    wb_info.center_freq_2 = vht_cent_ch_freq(pAd->CommonCfg.vht_cent_ch2, pComCfg->vht_bw);
#endif /* DOT11_VHT_R2 */
			    break;
	    }

            NdisMoveMemory(ptr, &wb_info, sizeof(WIDE_BW_CH_SWITCH_ELEMENT));
            wb_len = sizeof(WIDE_BW_CH_SWITCH_ELEMENT);
            ptr += wb_len;
            wb_len += 2;
        }

        *ptr = IE_VHT_TXPWR_ENV;
        NdisZeroMemory(&txpwr_env, sizeof(VHT_TXPWR_ENV_IE));
        tp_len = build_vht_txpwr_envelope(pAd, (UCHAR *)&txpwr_env);
        *(ptr + 1) = tp_len;
        ptr += 2;
        NdisMoveMemory(ptr, &txpwr_env, tp_len);
        ptr += tp_len;
        tp_len += 2;
        *ch_sw_wrapper = wb_len + tp_len;

        FrameLen += (2 + wb_len + tp_len);
    }
#endif /* DOT11_VHT_AC */
#endif /* DOT11_N_SUPPORT */
    *pFrameLen = FrameLen;
}
#endif /* A_BAND_SUPPORT */


static VOID ExtChannelCheck(RTMP_ADAPTER *pAd,struct wifi_dev *wdev)
{
    /* NOTE: 2015-Aug-10

        EXTCHA will be updated in HcBbpSetBwByChannel,
        but when 20/40 Coexist happened in protocol stage,
        we shall consider the station which connected already with BW_40.
        so we cannot switch HW to BW_20 directly, since we don't disconnect other station.

        so, RegTransmitSetting.field.EXTCHA will be set to "SCN" when recv 20/40 related mgmt frames.

        TODO: saparate the EXTCHA/BW7/Intolerant40 setting from common setting to per wdev scope.
    */

	/* sanity check for extention channel */
	N_ChannelCheck(pAd,wdev->PhyMode,wdev->channel);
	pAd->CommonCfg.AddHTInfo.ControlChan = wdev->channel;
    if ((wdev->extcha == EXTCHA_NONE) &&
        (pAd->CommonCfg.LastBSSCoexist2040.field.BSS20WidthReq ||
        pAd->CommonCfg.LastBSSCoexist2040.field.Intolerant40))
    {
        pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = BW_20;
    }
    else
    {
        if (HcGetBw(pAd, wdev) > BW_20)
            pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 1;
        else
            pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 0;    	 
    }
    pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = wdev->extcha;
}

VOID MakeHTIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG TmpLen;
    ULONG FrameLen = *pFrameLen;
    UCHAR HtLen, HtLen1;
    COMMON_CONFIG *pComCfg = &pAd->CommonCfg;
    /*UCHAR i; */

    HT_CAPABILITY_IE HtCapabilityTmp;
#ifdef RT_BIG_ENDIAN
    ADD_HT_INFO_IE  addHTInfoTmp;
#endif

    /* add HT Capability IE */
    HtLen = sizeof(pComCfg->HtCapability);
    HtLen1 = sizeof(pComCfg->AddHTInfo);

    /*update control channel for HT IE*/
    ExtChannelCheck(pAd,wdev);

#ifndef RT_BIG_ENDIAN
    NdisMoveMemory(&HtCapabilityTmp, &pComCfg->HtCapability, HtLen);
    HtCapabilityTmp.HtCapInfo.ChannelWidth = pComCfg->AddHTInfo.AddHtInfo.RecomWidth;

#ifdef TXBF_SUPPORT
    if (HcIsBfCapSupport(wdev) == FALSE)
    {
        UCHAR ucEBfCap;

        ucEBfCap = pAd->CommonCfg.ETxBfEnCond;
        pAd->CommonCfg.ETxBfEnCond = 0;
            
        mt_WrapSetETxBFCap(pAd, &HtCapabilityTmp.TxBFCap);

        pAd->CommonCfg.ETxBfEnCond = ucEBfCap;
    }
#endif /* TXBF_SUPPORT */    

    MakeOutgoingFrame(pBeaconFrame+FrameLen,         &TmpLen,
                                1,                                &HtCapIe,
                                1,                                &HtLen,
                                HtLen,          &HtCapabilityTmp,
                                1,                                &AddHtInfoIe,
                                1,                                &HtLen1,
                                HtLen1,          &pComCfg->AddHTInfo,
                        END_OF_ARGS);
#else
    NdisMoveMemory(&HtCapabilityTmp, &pComCfg->HtCapability, HtLen);
    HtCapabilityTmp.HtCapInfo.ChannelWidth = pComCfg->AddHTInfo.AddHtInfo.RecomWidth;

#ifdef TXBF_SUPPORT
    if (HcIsBfCapSupport(wdev) == FALSE)
    {
        UCHAR ucEBfCap;

        ucEBfCap = pAd->CommonCfg.ETxBfEnCond;
        pAd->CommonCfg.ETxBfEnCond = 0;
            
        mt_WrapSetETxBFCap(pAd, &HtCapabilityTmp.TxBFCap);

        pAd->CommonCfg.ETxBfEnCond = ucEBfCap;
    }
#endif /* TXBF_SUPPORT */

    *(USHORT *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
    {
        EXT_HT_CAP_INFO extHtCapInfo;

        NdisMoveMemory((PUCHAR)(&extHtCapInfo), (PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), sizeof(EXT_HT_CAP_INFO));
        *(USHORT *)(&extHtCapInfo) = cpu2le16(*(USHORT *)(&extHtCapInfo));
        NdisMoveMemory((PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), (PUCHAR)(&extHtCapInfo), sizeof(EXT_HT_CAP_INFO));
    }
#else
    *(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

    NdisMoveMemory(&addHTInfoTmp, &pComCfg->AddHTInfo, HtLen1);
    *(USHORT *)(&addHTInfoTmp.AddHtInfo2) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo2));
    *(USHORT *)(&addHTInfoTmp.AddHtInfo3) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo3));

    MakeOutgoingFrame(pBeaconFrame+FrameLen,         &TmpLen,
                                1,                                &HtCapIe,
                                1,                                &HtLen,
                                HtLen,                   &HtCapabilityTmp,
                                1,                                &AddHtInfoIe,
                                1,                                &HtLen1,
                                HtLen1,                   &addHTInfoTmp,
                        END_OF_ARGS);
#endif
    FrameLen += TmpLen;

#ifdef DOT11N_DRAFT3
    /*
        P802.11n_D3.03, 7.3.2.60 Overlapping BSS Scan Parameters IE
    */
    if ((wdev->channel <= 14) &&
        (pComCfg->HtCapability.HtCapInfo.ChannelWidth == 1))
    {
        OVERLAP_BSS_SCAN_IE  OverlapScanParam;
        ULONG   TmpLen;
        UCHAR   OverlapScanIE, ScanIELen;

        OverlapScanIE = IE_OVERLAPBSS_SCAN_PARM;
        ScanIELen = 14;
        OverlapScanParam.ScanPassiveDwell = cpu2le16(pComCfg->Dot11OBssScanPassiveDwell);
        OverlapScanParam.ScanActiveDwell = cpu2le16(pComCfg->Dot11OBssScanActiveDwell);
        OverlapScanParam.TriggerScanInt = cpu2le16(pComCfg->Dot11BssWidthTriggerScanInt);
        OverlapScanParam.PassiveTalPerChannel = cpu2le16(pComCfg->Dot11OBssScanPassiveTotalPerChannel);
        OverlapScanParam.ActiveTalPerChannel = cpu2le16(pComCfg->Dot11OBssScanActiveTotalPerChannel);
        OverlapScanParam.DelayFactor = cpu2le16(pComCfg->Dot11BssWidthChanTranDelayFactor);
        OverlapScanParam.ScanActThre = cpu2le16(pComCfg->Dot11OBssScanActivityThre);

        MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                            1,          &OverlapScanIE,
                            1,          &ScanIELen,
                            ScanIELen,  &OverlapScanParam,
                            END_OF_ARGS);

        FrameLen += TmpLen;
    }
#endif /* DOT11N_DRAFT3 */
    *pFrameLen = FrameLen;
}

#ifdef CONFIG_HOTSPOT
VOID MakeHotSpotIE(BSS_STRUCT *pMbss, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;
    ULONG TmpLen;

    /* Indication element */
    MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                        pMbss->HotSpotCtrl.HSIndicationIELen,
                        pMbss->HotSpotCtrl.HSIndicationIE, END_OF_ARGS);

    FrameLen += TmpLen;

    /* Interworking element */
    MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                        pMbss->HotSpotCtrl.InterWorkingIELen,
                        pMbss->HotSpotCtrl.InterWorkingIE, END_OF_ARGS);

    FrameLen += TmpLen;

    /* Advertisement Protocol element */
    MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                        pMbss->HotSpotCtrl.AdvertisementProtoIELen,
                        pMbss->HotSpotCtrl.AdvertisementProtoIE, END_OF_ARGS);

    FrameLen += TmpLen;

    /* Roaming Consortium element */
    MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                        pMbss->HotSpotCtrl.RoamingConsortiumIELen,
                        pMbss->HotSpotCtrl.RoamingConsortiumIE, END_OF_ARGS);

    FrameLen += TmpLen;

    /* P2P element */
    MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
                        pMbss->HotSpotCtrl.P2PIELen,
                        pMbss->HotSpotCtrl.P2PIE, END_OF_ARGS);

    FrameLen += TmpLen;

    *pFrameLen = FrameLen;
}
#endif /*CONFIG_HOTSPOT_IE*/

#ifdef CONFIG_AP_SUPPORT
VOID MakeExtCapIE(RTMP_ADAPTER *pAd, BSS_STRUCT *pMbss, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    /* 7.3.2.27 Extended Capabilities IE */
    COMMON_CONFIG *pComCfg = &pAd->CommonCfg;
    struct wifi_dev* wdev = &pMbss->wdev;
    ULONG TmpLen, infoPos;
    PUCHAR pInfo;
    UCHAR extInfoLen;
    BOOLEAN bNeedAppendExtIE = FALSE;
    EXT_CAP_INFO_ELEMENT    extCapInfo;
    UCHAR PhyMode = wdev->PhyMode;
    ULONG FrameLen = *pFrameLen;

    extInfoLen = sizeof(EXT_CAP_INFO_ELEMENT);
    NdisZeroMemory(&extCapInfo, extInfoLen);

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
    /* P802.11n_D1.10, HT Information Exchange Support */
    if (WMODE_CAP_N(PhyMode) && (wdev->channel <= 14) &&
        (wdev->DesiredHtPhyInfo.bHtEnable) &&
        (pComCfg->bBssCoexEnable == TRUE)
    )
    {
        extCapInfo.BssCoexistMgmtSupport = 1;
    }
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

#ifdef CONFIG_DOT11V_WNM
    if (pMbss->WNMCtrl.ProxyARPEnable)
        extCapInfo.proxy_arp = 1;

#ifdef CONFIG_HOTSPOT_R2
    if (pMbss->WNMCtrl.WNMNotifyEnable)
        extCapInfo.wnm_notification = 1;

    if (pMbss->HotSpotCtrl.QosMapEnable)
        extCapInfo.qosmap= 1;
#endif /* CONFIG_HOTSPOT_R2 */
#endif /* CONFIG_DOT11V_WNM */

#ifdef CONFIG_HOTSPOT
    if (pMbss->HotSpotCtrl.HotSpotEnable)
        extCapInfo.interworking = 1;
#endif /* CONFIG_HOTSPOT */

#ifdef DOT11V_WNM_SUPPORT
    if (IS_BSS_TRANSIT_MANMT_SUPPORT(pAd, apidx))
    {
        extCapInfo.BssTransitionManmt = 1;
    }
    if (IS_WNMDMS_SUPPORT(pAd, apidx))
    {
        extCapInfo.DMSSupport = 1;
    }
#endif /* DOT11V_WNM_SUPPORT */
#ifdef DOT11_VHT_AC
    if (WMODE_CAP_AC(PhyMode) &&
        (wdev->channel> 14))
        extCapInfo.operating_mode_notification = 1;
#endif /* DOT11_VHT_AC */
#ifdef FTM_SUPPORT
    if (pAd->pFtmCtrl->bSetCivicRpt)
        extCapInfo.civic_location = 1;

    if (pAd->pFtmCtrl->bSetLciRpt)
        extCapInfo.geospatial_location = 1;

    extCapInfo.ftm_resp = 1;
#endif /* FTM_SUPPORT */

    pInfo = (PUCHAR)(&extCapInfo);
    for (infoPos = 0; infoPos < extInfoLen; infoPos++)
    {
        if (pInfo[infoPos] != 0)
        {
            bNeedAppendExtIE = TRUE;
            break;
        }
    }

    if (bNeedAppendExtIE == TRUE)
    {
        for (infoPos = (extInfoLen - 1); infoPos >= EXT_CAP_MIN_SAFE_LENGTH; infoPos--)
        {
            if (pInfo[infoPos] == 0)
                extInfoLen --;
            else
                break;
        }

        MakeOutgoingFrame(pBeaconFrame+FrameLen, &TmpLen,
                        1, &ExtCapIe,
                        1, &extInfoLen,
                        extInfoLen, &extCapInfo,
                        END_OF_ARGS);
        FrameLen += TmpLen;
    }

    *pFrameLen = FrameLen;
}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
VOID MakeWmmIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG TmpLen = 0;
    ULONG FrameLen = *pFrameLen;
    UCHAR i;
    UCHAR WmeParmIe[26] = {IE_VENDOR_SPECIFIC, 24, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0, 0};
    UINT8 AIFSN[4];
#ifdef UAPSD_SUPPORT
    BSS_STRUCT *pMbss = wdev->func_dev;
#endif

    WmeParmIe[8] = pAd->ApCfg.BssEdcaParm.EdcaUpdateCount & 0x0f;

#ifdef UAPSD_SUPPORT
    UAPSD_MR_IE_FILL(WmeParmIe[8], &pMbss->wdev.UapsdInfo);
#endif /* UAPSD_SUPPORT */

    NdisMoveMemory(AIFSN, pAd->ApCfg.BssEdcaParm.Aifsn, sizeof(AIFSN));


    for (i=QID_AC_BE; i<=QID_AC_VO; i++)
    {
        WmeParmIe[10+ (i*4)] = (i << 5)                                         +     /* b5-6 is ACI */
                                ((UCHAR)pAd->ApCfg.BssEdcaParm.bACM[i] << 4)     +     /* b4 is ACM */
                                (AIFSN[i] & 0x0f);              /* b0-3 is AIFSN */
        WmeParmIe[11+ (i*4)] = (pAd->ApCfg.BssEdcaParm.Cwmax[i] << 4)           +     /* b5-8 is CWMAX */
                                (pAd->ApCfg.BssEdcaParm.Cwmin[i] & 0x0f);              /* b0-3 is CWMIN */
        WmeParmIe[12+ (i*4)] = (UCHAR)(pAd->ApCfg.BssEdcaParm.Txop[i] & 0xff);        /* low byte of TXOP */
        WmeParmIe[13+ (i*4)] = (UCHAR)(pAd->ApCfg.BssEdcaParm.Txop[i] >> 8);          /* high byte of TXOP */
    }

    MakeOutgoingFrame(pBeaconFrame+FrameLen,         &TmpLen,
                        26,                            WmeParmIe,
                        END_OF_ARGS);
    FrameLen += TmpLen;

    *pFrameLen = FrameLen;
}
#endif /* CONFIG_AP_SUPPORT */

VOID MakeCountryIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;
    ULONG TmpLen, TmpLen2=0;
    UCHAR *TmpFrame = NULL;
    UCHAR CountryIe = IE_COUNTRY;
#ifdef DOT11K_RRM_SUPPORT
    UCHAR apidx = wdev->func_idx;
#endif /* DOT11K_RRM_SUPPORT */

#ifndef EXT_BUILD_CHANNEL_LIST
    PCH_DESC pChDesc = NULL;
    UINT i;         

    if (WMODE_CAP_2G(wdev->PhyMode)) {
                if (pAd->CommonCfg.pChDesc2G!= NULL) 
                        pChDesc = (PCH_DESC) pAd->CommonCfg.pChDesc2G; 
                else
                        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: pChDesc2G is NULL !!!\n", __FUNCTION__));
    } else if (WMODE_CAP_5G(wdev->PhyMode)) {
                if (pAd->CommonCfg.pChDesc5G!= NULL) 
                        pChDesc = (PCH_DESC) pAd->CommonCfg.pChDesc5G;
                else
                        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: pChDesc5G is NULL !!!\n", __FUNCTION__));  
    }
#endif  


    /* add country IE, power constraint IE */
    if (pAd->CommonCfg.bCountryFlag)
    {
        os_alloc_mem(NULL, (UCHAR **)&TmpFrame, 256);
        if (TmpFrame != NULL)
        {
            NdisZeroMemory(TmpFrame, 256);

            /* prepare channel information */
#ifdef EXT_BUILD_CHANNEL_LIST
            BuildBeaconChList(pAd, wdev, TmpFrame, &TmpLen2);
#else
            {
                UCHAR MaxTxPower = GetCuntryMaxTxPwr(pAd, wdev->PhyMode,wdev->channel);
                for (i=0; pChDesc[i].FirstChannel != 0; i++)
                {
                     MakeOutgoingFrame(TmpFrame+TmpLen2,        &TmpLen,
                                1,      &pChDesc[i].FirstChannel,
                                1,       &pChDesc[i].NumOfCh,
                                1,         &MaxTxPower,
                                END_OF_ARGS);
                     TmpLen2 += TmpLen;
                }

            }
#endif /* EXT_BUILD_CHANNEL_LIST */

#ifdef DOT11K_RRM_SUPPORT
            if (IS_RRM_ENABLE(pAd, apidx)
                && (pAd->CommonCfg.RegulatoryClass[0] != 0))
            {
                TmpLen2 = 0;
                NdisZeroMemory(TmpFrame, sizeof(TmpFrame));
                RguClass_BuildBcnChList(pAd, TmpFrame, &TmpLen2);
            }
#endif /* DOT11K_RRM_SUPPORT */

            /* need to do the padding bit check, and concatenate it */
            if ((TmpLen2%2) == 0)
            {
                UCHAR   TmpLen3 = TmpLen2+4;
                MakeOutgoingFrame(pBeaconFrame+FrameLen,&TmpLen,
                                  1,                    &CountryIe,
                                  1,                    &TmpLen3,
                                  3,                    pAd->CommonCfg.CountryCode,
                                  TmpLen2+1,                TmpFrame,
                                  END_OF_ARGS);
            }
            else
            {
                UCHAR   TmpLen3 = TmpLen2+3;
                MakeOutgoingFrame(pBeaconFrame+FrameLen,&TmpLen,
                                  1,                    &CountryIe,
                                  1,                    &TmpLen3,
                                  3,                    pAd->CommonCfg.CountryCode,
                                  TmpLen2,              TmpFrame,
                                  END_OF_ARGS);
            }
            FrameLen += TmpLen;

            os_free_mem(TmpFrame);
        }
        else
            MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: Allocate memory fail!!!\n", __FUNCTION__));
    }
    *pFrameLen = FrameLen;
}

VOID MakeChReportIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;
#ifdef DOT11K_RRM_SUPPORT
    UCHAR i;
#else
    UCHAR APChannelReportIe = IE_AP_CHANNEL_REPORT;
    ULONG TmpLen;
    UCHAR PhyMode = wdev->PhyMode;
#endif

#ifdef DOT11K_RRM_SUPPORT
    for (i=0; i<MAX_NUM_OF_REGULATORY_CLASS; i++)
    {
        if (pAd->CommonCfg.RegulatoryClass[i] == 0)
            break;

        InsertChannelRepIE(pAd, pBeaconFrame+FrameLen, &FrameLen,
                            (RTMP_STRING *)pAd->CommonCfg.CountryCode,
                            pAd->CommonCfg.RegulatoryClass[i]);

    }
#else
    {
        /*
            802.11n D2.0 Annex J, USA regulatory
                class 32, channel set 1~7
                class 33, channel set 5-11
        */
        UCHAR rclass32[]={32, 1, 2, 3, 4, 5, 6, 7};
        UCHAR rclass33[]={33, 5, 6, 7, 8, 9, 10, 11};
        UCHAR rclasslen = 8; /*sizeof(rclass32); */
        if (PhyMode == (WMODE_B | WMODE_G | WMODE_GN))
        {
            MakeOutgoingFrame(pBeaconFrame+FrameLen,&TmpLen,
                              1,                    &APChannelReportIe,
                              1,                    &rclasslen,
                              rclasslen,            rclass32,
                              1,                    &APChannelReportIe,
                              1,                    &rclasslen,
                              rclasslen,            rclass33,
                              END_OF_ARGS);
            FrameLen += TmpLen;
        }
    }
#endif
    *pFrameLen = FrameLen;
}

VOID MakeExtSuppRateIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;
    ULONG TmpLen;
    UCHAR PhyMode = wdev->PhyMode;

    if ((wdev->rate.ExtRateLen) && (PhyMode != WMODE_B))
    {
        TmpLen = 0;
        MakeOutgoingFrame(pBeaconFrame+FrameLen,         &TmpLen,
                        1,                               &ExtRateIe,
                        1,                               &wdev->rate.ExtRateLen,
                        wdev->rate.ExtRateLen,       wdev->rate.ExtRate,
                        END_OF_ARGS);
        FrameLen += TmpLen;

        *pFrameLen = FrameLen;
    }
}

VOID MakePwrConstraintIe(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    COMMON_CONFIG *pComCfg = &pAd->CommonCfg;
#ifdef DOT11K_RRM_SUPPORT
    UCHAR apidx = wdev->func_idx;
#endif /* DOT11K_RRM_SUPPORT */
    ULONG FrameLen = *pFrameLen;
    ULONG TmpLen = 0;
#ifdef DOT11_VHT_AC
    UCHAR PhyMode = wdev->PhyMode;
#endif

    /*
        Only 802.11a APs that comply with 802.11h are required to include a
        Power Constrint Element(IE=32) in beacons and probe response frames
    */
    if (((wdev->channel > 14) && pComCfg->bIEEE80211H == TRUE)
#ifdef DOT11K_RRM_SUPPORT
        || IS_RRM_ENABLE(pAd, apidx)
#endif /* DOT11K_RRM_SUPPORT */
        )
    {
        UINT8 PwrConstraintIE = IE_POWER_CONSTRAINT;
        UINT8 PwrConstraintLen = 1;
        UINT8 PwrConstraint = pComCfg->PwrConstraint;

        /* prepare power constraint IE */
        MakeOutgoingFrame(pBeaconFrame+FrameLen,    &TmpLen,
                        1,                          &PwrConstraintIE,
                        1,                          &PwrConstraintLen,
                        1,                          &PwrConstraint,
                        END_OF_ARGS);
        FrameLen += TmpLen;
    }

#ifdef DOT11_VHT_AC
    if (WMODE_CAP_AC(PhyMode)) {
        UINT8 vht_txpwr_env_ie = IE_VHT_TXPWR_ENV;
        UINT8 ie_len;
        VHT_TXPWR_ENV_IE txpwr_env;

        TmpLen = 0;

        ie_len = build_vht_txpwr_envelope(pAd, (UCHAR *)&txpwr_env);
        MakeOutgoingFrame(pBeaconFrame+FrameLen, &TmpLen,
                    1,                          &vht_txpwr_env_ie,
                    1,                          &ie_len,
                    ie_len,                     &txpwr_env,
                    END_OF_ARGS);
        FrameLen += TmpLen;
    }
#endif /* DOT11_VHT_AC */

    *pFrameLen = FrameLen;
}

VOID ComposeBcnPktTail(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, ULONG *pFrameLen, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = *pFrameLen;
#ifdef CONFIG_AP_SUPPORT
    COMMON_CONFIG *pComCfg = &pAd->CommonCfg;
    BSS_STRUCT *pMbss = NULL;
    UCHAR apidx = 0;
    //BOOLEAN HotSpotEnable = FALSE;
#endif
    UCHAR PhyMode = wdev->PhyMode;
#ifdef AP_QLOAD_SUPPORT
	QLOAD_CTRL *pQloadCtrl = HcGetQloadCtrl(pAd);
#endif /*AP_QLOAD_SUPPORT*/
#if defined(TXBF_SUPPORT) && defined(VHT_TXBF_SUPPORT)
    UCHAR ucETxBfCap;
#endif /* TXBF_SUPPORT && VHT_TXBF_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
    if (wdev->wdev_type == WDEV_TYPE_AP)
    {
        pMbss = wdev->func_dev;
        apidx = wdev->func_idx;
    }

	/* fix klockwork issue */
	if(pMbss == NULL){
		MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                    ("%s - unexpected pMbss NULL, please check\n",__FUNCTION__));
        return;
	}
#endif /* CONFIG_AP_SUPPORT */


    MakeCountryIe(pAd, wdev, &FrameLen, pBeaconFrame);

#if defined (A_BAND_SUPPORT) && defined (CONFIG_AP_SUPPORT)
    MakePwrConstraintIe(pAd, wdev, &FrameLen, pBeaconFrame);

    /* fill up Channel Switch Announcement Element */
    if ((wdev->channel > 14)
        && (pComCfg->bIEEE80211H == 1)
        &&(pAd->Dot11_H.RDMode == RD_SWITCHING_MODE)
        )
    {
        MakeChSwitchAnnounceIEandExtend(pAd, wdev, &FrameLen, pBeaconFrame);
    }
#endif /* A_BAND_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
#ifdef DOT11K_RRM_SUPPORT
    if (IS_RRM_ENABLE(pAd, apidx))
    {
        InsertTpcReportIE(pAd, pBeaconFrame+FrameLen, &FrameLen,
        RTMP_GetTxPwr(pAd, wdev->rate.MlmeTransmit,wdev->channel), 0);
        RRM_InsertRRMEnCapIE(pAd, pBeaconFrame+FrameLen, &FrameLen, apidx);
    }
#endif /* DOT11K_RRM_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

    MakeChReportIe(pAd, wdev, &FrameLen, pBeaconFrame);

#ifdef DOT11R_FT_SUPPORT
    /*
        The Mobility Domain information element (MDIE) is present in Beacon
        frame when dot11FastBssTransitionEnable is set to true.
    */
    if (pAd->ApCfg.MBSSID[apidx].wdev.FtCfg.FtCapFlag.Dot11rFtEnable)
    {
        PFT_CFG pFtCfg = &pAd->ApCfg.MBSSID[apidx].wdev.FtCfg;
        FT_CAP_AND_POLICY FtCap;
        NdisZeroMemory(&FtCap, sizeof(FT_CAP_AND_POLICY));
        FtCap.field.FtOverDs = pFtCfg->FtCapFlag.FtOverDs;
        FtCap.field.RsrReqCap = pFtCfg->FtCapFlag.RsrReqCap;
        FT_InsertMdIE(pAd, pBeaconFrame + FrameLen, &FrameLen,
                        pFtCfg->FtMdId, FtCap);
    }
#endif /* DOT11R_FT_SUPPORT */

    /* Update ERP */
    if ((wdev->rate.ExtRateLen) && (PhyMode != WMODE_B))
    {
        MakeErpIE(pAd, wdev, &FrameLen, pBeaconFrame);
    }

    MakeExtSuppRateIe(pAd, wdev, &FrameLen, pBeaconFrame);

    ComposeRSNIE(pAd, wdev, &FrameLen, pBeaconFrame);
    ComposeWPSIE(pAd, wdev, &FrameLen, pBeaconFrame);

#ifdef AP_QLOAD_SUPPORT
    if (pQloadCtrl->FlgQloadEnable != 0)
    {
#ifdef CONFIG_HOTSPOT_R2
        if (pMbss->HotSpotCtrl.QLoadTestEnable == 1)
            FrameLen += QBSS_LoadElementAppend_HSTEST(pAd, pBeaconFrame+FrameLen, apidx);
        else if (pMbss->HotSpotCtrl.QLoadTestEnable == 0)
#endif
        FrameLen += QBSS_LoadElementAppend(pAd, pBeaconFrame+FrameLen);
    }
#endif /* AP_QLOAD_SUPPORT */

#ifdef DOT11_N_SUPPORT
    /* step 5. Update HT. Since some fields might change in the same BSS. */
    if (WMODE_CAP_N(PhyMode) && (wdev->DesiredHtPhyInfo.bHtEnable))
    {
#ifdef DOT11_VHT_AC
        struct _build_ie_info vht_ie_info;
#endif /* DOT11_VHT_AC */

        MakeHTIe(pAd, wdev, &FrameLen, pBeaconFrame);
#ifdef CONFIG_HOTSPOT
        if (pMbss->HotSpotCtrl.HotSpotEnable)
        {
            MakeHotSpotIE(pMbss, &FrameLen, pBeaconFrame);
        }
#endif /*CONFIG_HOTSPOT*/
#ifdef DOT11_VHT_AC
        vht_ie_info.frame_buf = (UCHAR *)(pBeaconFrame + FrameLen);
        vht_ie_info.frame_subtype = SUBTYPE_BEACON;
        vht_ie_info.channel = wdev->channel;
        vht_ie_info.phy_mode = PhyMode;

#if defined(TXBF_SUPPORT) && defined(VHT_TXBF_SUPPORT)
        ucETxBfCap = pAd->CommonCfg.ETxBfEnCond;
        if (HcIsBfCapSupport(wdev) == FALSE)
        {
            pAd->CommonCfg.ETxBfEnCond = 0;
        }
#endif /* TXBF_SUPPORT && VHT_TXBF_SUPPORT */ 

        FrameLen += build_vht_ies(pAd, &vht_ie_info);

#if defined(TXBF_SUPPORT) && defined(VHT_TXBF_SUPPORT)
		pAd->CommonCfg.ETxBfEnCond = ucETxBfCap;
#endif /* TXBF_SUPPORT && VHT_TXBF_SUPPORT */ 
#endif /* DOT11_VHT_AC */
    }
#endif /* DOT11_N_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
    /* 7.3.2.27 Extended Capabilities IE */
    MakeExtCapIE(pAd, pMbss, &FrameLen, pBeaconFrame);
#endif /*CONFIG_AP_SUPPORT */

#ifdef WFA_VHT_PF
    if (pAd->force_vht_op_mode == TRUE)
    {
        ULONG TmpLen;
        UCHAR operating_ie = IE_OPERATING_MODE_NOTIFY, operating_len = 1;
        OPERATING_MODE operating_mode;

        operating_mode.rx_nss_type = 0;
        operating_mode.rx_nss = (pAd->vht_pf_op_ss - 1);
        operating_mode.ch_width = pAd->vht_pf_op_bw;

        MakeOutgoingFrame(pBeaconFrame+FrameLen, &TmpLen,
                          1,    &operating_ie,
                          1,    &operating_len,
                          1,    &operating_mode,
                          END_OF_ARGS);
        FrameLen += TmpLen;
    }
#endif /* WFA_VHT_PF */

#ifdef CONFIG_AP_SUPPORT
    if (wdev->bWmmCapable)
    {
        MakeWmmIe(pAd, wdev, &FrameLen, pBeaconFrame);
    }
#endif

#ifdef CONFIG_AP_SUPPORT
#ifdef DOT11K_RRM_SUPPORT
    if (IS_RRM_ENABLE(pAd, apidx))
    {
        PRRM_QUIET_CB pQuietCB = &pMbss->RrmCfg.QuietCB;
        RRM_InsertQuietIE(pAd, pBeaconFrame+FrameLen, &FrameLen,
                pQuietCB->QuietCnt ,pQuietCB->QuietPeriod,
                pQuietCB->QuietDuration, pQuietCB->QuietOffset);

#ifndef APPLE_11K_IOT
        /* Insert BSS AC Access Delay IE. */
        RRM_InsertBssACDelayIE(pAd, pBeaconFrame+FrameLen, &FrameLen);

        /* Insert BSS Available Access Capacity IE. */
        RRM_InsertBssAvailableACIE(pAd, pBeaconFrame+FrameLen, &FrameLen);
#endif /* !APPLE_11K_IOT */

    }
#endif /* DOT11K_RRM_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */


#ifdef CONFIG_AP_SUPPORT
    /* add Ralink-specific IE here - Byte0.b0=1 for aggregation, Byte0.b1=1 for piggy-back */
    FrameLen += build_vendor_ie(pAd, wdev, (pBeaconFrame + FrameLen));
{
}
#endif /*CONFIG_AP_SUPPORT*/


    *pFrameLen = FrameLen;
}
VOID updateBeaconRoutineCase(RTMP_ADAPTER *pAd, BOOLEAN UpdateAfterTim)
{
    INT     i;
	 struct wifi_dev *wdev ;
#ifdef CONFIG_AP_SUPPORT
    BOOLEAN FlgQloadIsAlarmIssued = FALSE;
	wdev = &pAd->ApCfg.MBSSID[0].wdev;

    if (pAd->ApCfg.DtimCount == 0)
        pAd->ApCfg.DtimCount = pAd->ApCfg.DtimPeriod - 1;
    else
        pAd->ApCfg.DtimCount -= 1;

#ifdef AP_QLOAD_SUPPORT
    FlgQloadIsAlarmIssued = QBSS_LoadIsAlarmIssued(pAd);
#endif /* AP_QLOAD_SUPPORT */

    if ((pAd->ApCfg.DtimCount == 0) &&
        (((pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_INFO_SYNC) &&
          (pAd->CommonCfg.bForty_Mhz_Intolerant == FALSE)) ||
        (FlgQloadIsAlarmIssued == TRUE)))
    {
        UCHAR   prevBW, prevExtChOffset;
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                    ("DTIM Period reached, BSS20WidthReq=%d, Intolerant40=%d!\n",
                    pAd->CommonCfg.LastBSSCoexist2040.field.BSS20WidthReq,
                    pAd->CommonCfg.LastBSSCoexist2040.field.Intolerant40));
        pAd->CommonCfg.Bss2040CoexistFlag &= (~BSS_2040_COEXIST_INFO_SYNC);

        prevBW = pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;
        prevExtChOffset = pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset;

        if (pAd->CommonCfg.LastBSSCoexist2040.field.BSS20WidthReq ||
            pAd->CommonCfg.LastBSSCoexist2040.field.Intolerant40 ||
            (pAd->MacTab.fAnyStaFortyIntolerant == TRUE) ||
            (FlgQloadIsAlarmIssued == TRUE))
        {
            pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = BW_20;
            pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = EXTCHA_NONE;
            wdev->extcha = EXTCHA_NONE;
			HcUpdateExtCha(pAd,wdev->channel,wdev->extcha);
        }
        else
        {
            pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = pAd->CommonCfg.RegTransmitSetting.field.BW;
            pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = wdev->extcha;
        }
        MTWF_LOG(DBG_CAT_AP, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("\tNow RecomWidth=%d, ExtChanOffset=%d, prevBW=%d, prevExtOffset=%d\n",
                pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth,
                pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset,
                prevBW, prevExtChOffset));
        pAd->CommonCfg.Bss2040CoexistFlag |= BSS_2040_COEXIST_INFO_NOTIFY;
    }
#endif /* CONFIG_AP_SUPPORT */

    for(i=0; i < WDEV_NUM_MAX; i++)
    {
        wdev = pAd->wdev_list[i];
        if (wdev != NULL)
            MakeBeacon(pAd, wdev, UpdateAfterTim);
    }
}

VOID UpdateBeaconHandler(
        RTMP_ADAPTER *pAd,
        struct wifi_dev *wdev,
        UCHAR BCN_UPDATE_REASON)
{
    UCHAR i;
    UCHAR NumOfBcns;
    BOOLEAN UpdateAfterTim = FALSE;
    BCN_BUF_STRUC *pbcn_buf = NULL;

    switch (BCN_UPDATE_REASON)
    {
        case INTERFACE_STATE_CHANGE:
        {
            /*
            if chip is RT/RLT chip, shall re-order TXD
            otherwise, MT chip just send out bcn of the corresponding wdev.
            */
            if ((pAd->chipCap.hif_type == HIF_RTMP) ||
                (pAd->chipCap.hif_type == HIF_RLT)
            )
            {
                NumOfBcns = AsicDisableBeacon(pAd, wdev);
                for(i=0; i < WDEV_NUM_MAX; i++)
                {
                    if (pAd->wdev_list[i] != NULL)
                        MakeBeacon(pAd, pAd->wdev_list[i], UpdateAfterTim);
                }
                AsicEnableBeacon(pAd, wdev, NumOfBcns);
            }
            else /* MT_CHIP */
            {
                if (wdev != NULL)
                {
                    NumOfBcns = AsicDisableBeacon(pAd, wdev);
                    MakeBeacon(pAd, wdev, UpdateAfterTim);
                    AsicEnableBeacon(pAd, wdev, NumOfBcns);
                }
            }
        }
            break;
        case IE_CHANGE:
        {
            if (wdev != NULL)
            {
                pbcn_buf = &wdev->bcn_buf;
                if (pbcn_buf && pbcn_buf->BcnUpdateMethod == BCN_GEN_BY_FW)
                {
                    /*
                        fw offload bcn.
                        if there is update for IE, shall issue new content to fw.

                        use hw_ctrl task to aviod invalid context happen.
                    */
                    MT_UPDATE_BEACON rMtUpdateBeacon;
                    os_zero_mem(&rMtUpdateBeacon, sizeof(MT_UPDATE_BEACON));
                    rMtUpdateBeacon.wdev = wdev;
                    rMtUpdateBeacon.UpdateReason = IE_CHANGE;
                    HW_BECON_UPDATE(pAd,rMtUpdateBeacon);
                }
            }
        }
            break;
        case AP_RENEW:
        {
             /*
            if chip is RT/RLT chip, shall re-order TXD
            otherwise, MT chip just send out bcn of the corresponding wdev.
            */
            if ((pAd->chipCap.hif_type == HIF_RTMP) ||
                (pAd->chipCap.hif_type == HIF_RLT)
            )
            {
                NumOfBcns = AsicDisableBeacon(pAd, wdev);
                for(i=0; i < WDEV_NUM_MAX; i++)
                {
                    if (pAd->wdev_list[i] != NULL)
                        MakeBeacon(pAd, pAd->wdev_list[i], UpdateAfterTim);
                }
                AsicEnableBeacon(pAd, wdev, NumOfBcns);
            }
            else
            {
                for(i=0; i < WDEV_NUM_MAX; i++)
                {
                    if (pAd->wdev_list[i] != NULL)
                    {
                        NumOfBcns = AsicDisableBeacon(pAd, pAd->wdev_list[i]);
                        MakeBeacon(pAd, pAd->wdev_list[i], UpdateAfterTim);
                        AsicEnableBeacon(pAd, pAd->wdev_list[i], NumOfBcns);
                    }
                }
            }
        }
            break;
        case PRETBTT_UPDATE:
        {
            UpdateAfterTim = TRUE;
            updateBeaconRoutineCase(pAd, UpdateAfterTim);
        }
            break;
        default:
            MTWF_LOG(DBG_CAT_TX,
                    DBG_SUBCAT_ALL,
                    DBG_LVL_ERROR,
                    ("%s(): Wrong Update reason: %d\n",
                        __FUNCTION__, BCN_UPDATE_REASON));
            break;
    }
    return;
}


VOID LeadTimeForBcn(RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    if (WDEV_WITH_BCN_ABILITY(wdev->wdev_type))
    {
        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s, OmacIdx = %x, WDEV_WITH_BCN_ABILITY\n",
                    __FUNCTION__, wdev->OmacIdx));
        Status = bcn_buf_init(pAd, wdev);
        if (Status == NDIS_STATUS_FAILURE)
        {
            MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s, OmacIdx = %x, bcn_buf_init fail\n",
                    __FUNCTION__, wdev->OmacIdx));
            return;
        }
        wdev->bcn_buf.bBcnSntReq = TRUE;

        UpdateBeaconHandler(
                pAd,
                wdev,
                INTERFACE_STATE_CHANGE);
    }
}

#ifdef CONFIG_AP_SUPPORT
INT BcnTimUpdate(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UCHAR *ptr)
{
    INT tim_len = 0, i;
    UCHAR ID_1B, TimFirst, TimLast, *pTim;
    //BSS_STRUCT *pMbss = wdev->func_dev;
    BCN_BUF_STRUC *bcn_buf = &wdev->bcn_buf;


    *ptr = IE_TIM;
    *(ptr + 2) = pAd->ApCfg.DtimCount;
    *(ptr + 3) = pAd->ApCfg.DtimPeriod;

    /* find the smallest AID (PS mode) */
    TimFirst = 0; /* record first TIM byte != 0x00 */
    TimLast = 0;  /* record last  TIM byte != 0x00 */
    pTim = bcn_buf->TimBitmaps;

    for(ID_1B=0; ID_1B<WLAN_MAX_NUM_OF_TIM; ID_1B++)
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

    *(ptr + 1) = 3+(TimLast-TimFirst+1); /* TIM IE length */
    *(ptr + 4) = TimFirst;

    for(i=TimFirst; i<=TimLast; i++)
        *(ptr + 5 + i - TimFirst) = pTim[i];

    /* bit0 means backlogged mcast/bcast */
    if (pAd->ApCfg.DtimCount == 0)
        *(ptr + 4) |= (bcn_buf->TimBitmaps[WLAN_CT_TIM_BCMC_OFFSET] & 0x01);

    /* adjust BEACON length according to the new TIM */
    tim_len = (2 + *(ptr+1));
    return tim_len;
}
#endif

ULONG ComposeBcnPktHead(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UCHAR *pBeaconFrame)
{
    ULONG FrameLen = 0;
    ULONG TmpLen;
    UCHAR DsLen = 1, SsidLen = 0, SupRateLen;
    HEADER_802_11 BcnHdr;
    LARGE_INTEGER FakeTimestamp;
    UCHAR PhyMode;
#ifdef CONFIG_AP_SUPPORT
    BSS_STRUCT *pMbss = NULL;
#endif /* CONFIG_AP_SUPPORT */
    //INT apidx = wdev->func_idx;
    UCHAR *Addr2 = NULL, *Addr3 = NULL, *pSsid = NULL;
    USHORT CapabilityInfo, *pCapabilityInfo = &CapabilityInfo;
    BOOLEAN ess = FALSE;
	UCHAR Channel;

    PhyMode = wdev->PhyMode;
	Channel = wdev->channel;

#ifdef CONFIG_AP_SUPPORT
    if (wdev->wdev_type == WDEV_TYPE_AP)
    {
        pMbss = wdev->func_dev;
        SsidLen = (pMbss->bHideSsid) ? 0 : pMbss->SsidLen;
        Addr2 = wdev->if_addr;
        Addr3 = wdev->bssid;
        pSsid = pMbss->Ssid;
        ess = TRUE;
        pCapabilityInfo = &pMbss->CapabilityInfo;
		/*for 802.11H in Switch mode should take current channel*/
		if(pAd->CommonCfg.bIEEE80211H == TRUE && pAd->Dot11_H.RDMode== RD_SWITCHING_MODE){
			Channel = (pAd->Dot11_H.org_ch!=0) ? pAd->Dot11_H.org_ch : Channel ;
		}
    }
#endif

    MgtMacHeaderInit(pAd,
                    &BcnHdr,
                    SUBTYPE_BEACON,
                    0,
                    BROADCAST_ADDR,
                    Addr2,
                    Addr3);

    MakeOutgoingFrame(
                    pBeaconFrame,           &FrameLen,
                    sizeof(HEADER_802_11),  &BcnHdr,
                    TIMESTAMP_LEN,          &FakeTimestamp,
                    2,                      &pAd->CommonCfg.BeaconPeriod,
                    2,                      pCapabilityInfo,
                    1,                      &SsidIe,
                    1,                      &SsidLen,
                    SsidLen,                pSsid,
                    END_OF_ARGS);

    /*
      if wdev is AP, SupRateLen is global setting,
      shall check each's wdev setting to update SupportedRate.
    */
    SupRateLen = wdev->rate.SupRateLen;
    if (PhyMode == WMODE_B)
        SupRateLen = 4;

    TmpLen = 0;
    MakeOutgoingFrame(pBeaconFrame+FrameLen,      &TmpLen,
                    1,                              &SupRateIe,
                    1,                              &SupRateLen,
                    SupRateLen,                     wdev->rate.SupRate,
                    END_OF_ARGS);

    FrameLen += TmpLen;

    TmpLen = 0;
    MakeOutgoingFrame(pBeaconFrame+FrameLen,        &TmpLen,
                    1,                              &DsIe,
                    1,                              &DsLen,
                    1,                              &Channel,
                    END_OF_ARGS);
    FrameLen += TmpLen;


    return FrameLen;
}

#ifdef MT7615
#ifdef CONFIG_AP_SUPPORT
UINT32 nobcn = 0;
#define CR4_HEART_BEAT_STS 0x80200
VOID BcnCheck(RTMP_ADAPTER *pAd)
{
	if ((pAd->Mlme.PeriodicRound % 50) == 0) {
		UINT32 mac_val;
		UINT32 bcn_cnt = 0;
		UINT32 recoverext=0;
		UINT32 Index;
		BOOLEAN bcnactive = FALSE;
		struct wifi_dev *wdev;

#ifdef ERR_RECOVERY
		if(IsErrRecoveryInIdleStat(pAd) == FALSE)
			return;
#endif
#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
		if (pAd->CommonCfg.bOverlapScanning)
			return;
#endif
#endif

#ifdef CONFIG_ATE
		if (ATE_ON(pAd))
			return;
#endif

#ifdef MT_DFS_SUPPORT
		if (pAd->Dot11_H.RDMode == RD_SILENCE_MODE)
			return;
#endif

		if (!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_SYSEM_READY)))
			return;

		if (RTMP_TEST_FLAG(pAd, (fRTMP_ADAPTER_RADIO_OFF | 
					fRTMP_ADAPTER_HALT_IN_PROGRESS |
					fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			return;

		for (Index = 0; Index < WDEV_NUM_MAX; Index++) {
			wdev = pAd->wdev_list[Index];
			if (wdev == NULL)
				continue;
			if ((HcGetBandByWdev(wdev) == DBDC_BAND0) 
				&& (wdev->bss_info_argument.Active == TRUE)
				&& (wdev->bcn_buf.bBcnSntReq)) {
				bcnactive = TRUE;
				break;
			}
		}
		if (bcnactive == FALSE)
			return;

		MAC_IO_READ32(pAd, MIB_M0SDR0, &mac_val);
		bcn_cnt = (mac_val & 0xffff);

			
		if (bcn_cnt == 0) {
			nobcn++;
			if (nobcn > 4) {
				if (nobcn % 6 == 0) //6*5=30s
					MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
						("%s: no bcn still occur within %d sec!!\n",__FUNCTION__, nobcn * 5));
				if (nobcn == 5)
					MtCmdFwLog2Host(pAd, 0, 0);
				return;
			}
		}
		else if (nobcn != 0) {
			recoverext = 1;
			nobcn = 0;
		}
		else {
			nobcn = 0;
			return;
		}

		if ((nobcn != 0 || recoverext == 1) && DebugLevel >= DBG_LVL_ERROR) {
			UINT32 mac_val;
			UINT32 addr;
			UINT32 idx;
			if (recoverext == 1)
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
					("%s: bcn recover!!\ndebug info dump as below:\n\n",
					__FUNCTION__));
			else
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
					("%s: no bcn occurs within %d sec!!\ndebug info dump as below:\n\n",
					__FUNCTION__, nobcn * 5));
			dump_dmac_mib_info(pAd, "0");
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			show_trinfo_proc(pAd,"");
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			ShowPLEInfo(pAd, NULL);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			dump_dmac_pse_info(pAd);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			show_tpinfo_proc(pAd, NULL);

			MtCmdFwLog2Host(pAd, 0, 2);
			MtCmdFwLog2Host(pAd, 1, 2);
			MtCmdFwLog2Host(pAd, 1, 155);
			MtCmdFwLog2Host(pAd, 1, 156);
			MtCmdFwLog2Host(pAd, 1, 39);
			MtCmdFwLog2Host(pAd, 0, 4);
			MtCmdFwLog2Host(pAd, 0, 11);

			MAC_IO_READ32(pAd, ARB_BFCR, &mac_val);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("ARB_BFCR(0x820f3190)=0x%08x\n", mac_val));

			MAC_IO_READ32(pAd, ARB_SCR, &mac_val);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("ARB_SCR(0x820f3080)=0x%08x\n", mac_val));

			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("dump 0x820f3100~0x820f3154:\n"));
			for (addr = ARB_TQSW0; addr <= ARB_TQPM1; addr = addr + 4) {
				if ((addr & 0xf) == 0 && (addr != ARB_TQSW0))
					MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
				MAC_IO_READ32(pAd, addr, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr 0x%05x=0x%08x ", addr, mac_val));
			}
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			MAC_IO_READ32(pAd, ARB_BFCR, &mac_val);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("ARB_BFCR(0x820f3190)=0x%08x\n", mac_val));
			MAC_IO_READ32(pAd, CR4_HEART_BEAT_STS, &mac_val);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("CR4 heart beat status (0x80200)=0x%08x\n", mac_val));
			MtCmdFwLog2Host(pAd, 1, 0);

			MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS, &mac_val);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("RO_BAND0_PHYCTRL_STS(0x82070230)= 0x%08x\n", mac_val));
			// 0x82070618~0x8207065c
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("dump 0x82070618~0x8207065c:\n"));
			for (addr = PHY_BAND0_PHYMUX_6; addr <= PHY_BAND0_PHYMUX_23; addr = addr + 4) {
				if ((addr & 0xf) == 8 && (addr != PHY_BAND0_PHYMUX_6))
					MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
				MAC_IO_READ32(pAd, addr, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr 0x%05x=0x%08x ", addr, mac_val));
			}
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			// 0x8207227c~0x82072294
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("dump 0x8207227c~0x82072294:\n"));
			for (addr = RO_BAND0_RXTD_DEBUG0; addr <= RO_BAND0_RXTD_DEBUG6; addr = addr + 4) {
				if (addr == RO_BAND0_RXTD_DEBUG4)
					MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
				MAC_IO_READ32(pAd, addr, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr 0x%05x=0x%08x ", addr, mac_val));
			}
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			// 0x820721a0~0x820721b8
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("dump 0x820721a0~0x820721b8:\n"));
			for (addr = RO_BAND0_AGC_DEBUG_0; addr <= RO_BAND0_AGC_DEBUG_6; addr = addr + 4) {
				if (addr == RO_BAND0_AGC_DEBUG_4)
					MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
				MAC_IO_READ32(pAd, addr, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr 0x%05x=0x%08x ", addr, mac_val));
			}
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));

			/* 
				
				Write 0x82070614[22:20]=0
				Write 0x82070614[22:20]=5
			
				// read the following registers by 10 times
				0X8207_020C
				0x8207_0210
				0x8207_0214
				0x8207_021C
				0x8207_0220
				// End of Loop
				
			*/
			MAC_IO_READ32(pAd, PHY_BAND0_PHYMUX_5, &mac_val);
			mac_val &= ~(BITS(20,22));
			MAC_IO_WRITE32(pAd, PHY_BAND0_PHYMUX_5, mac_val);
			MAC_IO_READ32(pAd, PHY_BAND0_PHYMUX_5, &mac_val);
			mac_val &= ~(BITS(20,22));
			mac_val |= (BIT(20) | BIT(22));
			MAC_IO_WRITE32(pAd, PHY_BAND0_PHYMUX_5, mac_val);
			for (idx = 0; idx < 10; idx++) {
				MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS0, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("loop %d addr(0x8207020c)=0x%08x ",idx, mac_val));
				MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS1, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr(0x82070210)=0x%08x ", mac_val));
				MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS2, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr(0x82070214)=0x%08x ", mac_val));
				MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS4, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr(0x8207021c)=0x%08x ", mac_val));
				MAC_IO_READ32(pAd, RO_BAND0_PHYCTRL_STS5, &mac_val);
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("addr(0x82070220)=0x%08x\n", mac_val));
				
			}
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
			if (recoverext == 1)
				MtCmdFwLog2Host(pAd, 0, 0);
		}
	}
}
#endif
#endif

