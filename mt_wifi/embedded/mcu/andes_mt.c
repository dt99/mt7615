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
	andes_mt.c
*/

#include	"rt_config.h"


#ifdef UNIFY_FW_CMD
/* static decaration */
static VOID AndesMTFillTxDHeader(struct cmd_msg *msg, PNDIS_PACKET net_pkt);
#endif /* UNIFY_FW_CMD */



#if defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT)
INT32 AndesMTPciKickOutCmdMsg(PRTMP_ADAPTER pAd, struct cmd_msg *msg)
{
    int ret = NDIS_STATUS_SUCCESS;
    unsigned long flags = 0;
	ULONG FreeNum;
	PNDIS_PACKET net_pkt = msg->net_pkt;
	UINT32 SwIdx = 0;
	UCHAR *pSrcBufVA;
	UINT SrcBufLen = 0;
	PACKET_INFO PacketInfo;
	TXD_STRUC *pTxD;
	struct MCU_CTRL *ctl = &pAd->MCUCtrl;
#ifdef RT_BIG_ENDIAN
	TXD_STRUC *pDestTxD;
	UCHAR tx_hw_info[TXD_SIZE];
#endif

	if (!OS_TEST_BIT(MCU_INIT, &ctl->flags))
    {
        return -1;
    }

	FreeNum = GET_CTRLRING_FREENO(pAd);
	if (FreeNum == 0)
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_WARN,
                    ("%s FreeNum == 0 (TxCpuIdx = %d, "
                    "TxDmaIdx = %d, TxSwFreeIdx = %d)\n",
        		    __FUNCTION__, pAd->CtrlRing.TxCpuIdx,
        		    pAd->CtrlRing.TxDmaIdx, pAd->CtrlRing.TxSwFreeIdx));
		return NDIS_STATUS_FAILURE;
	}

	RTMP_SPIN_LOCK_IRQSAVE(&pAd->CtrlRingLock, &flags);

	RTMP_QueryPacketInfo(net_pkt, &PacketInfo, &pSrcBufVA, &SrcBufLen);
	if (pSrcBufVA == NULL)
    {
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->CtrlRingLock, &flags);
		return NDIS_STATUS_FAILURE;
	}

	SwIdx = pAd->CtrlRing.TxCpuIdx;

#ifdef RT_BIG_ENDIAN
	pDestTxD  = (TXD_STRUC *)pAd->CtrlRing.Cell[SwIdx].AllocVa;
	NdisMoveMemory(&tx_hw_info[0], (UCHAR *)pDestTxD, TXD_SIZE);
 	pTxD = (TXD_STRUC *)&tx_hw_info[0];
#else
	pTxD  = (TXD_STRUC *)pAd->CtrlRing.Cell[SwIdx].AllocVa;
#endif

#ifdef RT_BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif /* RT_BIG_ENDIAN */

	pAd->CtrlRing.Cell[SwIdx].pNdisPacket = net_pkt;
	pAd->CtrlRing.Cell[SwIdx].pNextNdisPacket = NULL;

	pAd->CtrlRing.Cell[SwIdx].PacketPa = PCI_MAP_SINGLE(pAd, (pSrcBufVA),
                                    (SrcBufLen), 0, RTMP_PCI_DMA_TODEVICE);

	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->SDLen0 = SrcBufLen;
	pTxD->SDLen1 = 0;
	pTxD->SDPtr0 = pAd->CtrlRing.Cell[SwIdx].PacketPa;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

#ifdef RT_BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
	WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif

	/* flush dcache if no consistent memory is supported */
	RTMP_DCACHE_FLUSH(SrcBufPA, SrcBufLen);
	RTMP_DCACHE_FLUSH(pAd->CtrlRing.Cell[SwIdx].AllocPa, TXD_SIZE);

   	/* Increase TX_CTX_IDX, but write to register later.*/
	INC_RING_INDEX(pAd->CtrlRing.TxCpuIdx, CTL_RING_SIZE);

	if (IS_CMD_MSG_NEED_SYNC_WITH_FW_FLAG_SET(msg))
    {
        AndesQueueTailCmdMsg(&ctl->ackq, msg, wait_ack);
        msg->sending_time_in_jiffies = jiffies;
    }
    else
    {
        AndesQueueTailCmdMsg(&ctl->tx_doneq, msg, tx_done);
    }
	if (!OS_TEST_BIT(MCU_INIT, &ctl->flags))
    {
        RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->CtrlRingLock, &flags);
        return -1;
	}

	HIF_IO_WRITE32(pAd, pAd->CtrlRing.hw_cidx_addr, pAd->CtrlRing.TxCpuIdx);

	RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->CtrlRingLock, &flags);

	return ret;
}

#if defined(MT7615) || defined(MT7622)
INT32 AndesMTPciKickOutCmdMsgFwDlRing(PRTMP_ADAPTER pAd, struct cmd_msg *msg)
{
	int ret = NDIS_STATUS_SUCCESS;
	unsigned long flags = 0;
	ULONG FreeNum;
	PNDIS_PACKET net_pkt = msg->net_pkt;
	UINT32 SwIdx = 0;
	UCHAR *pSrcBufVA;
	UINT SrcBufLen = 0;
	PACKET_INFO PacketInfo;
	TXD_STRUC *pTxD;
	struct MCU_CTRL *ctl = &pAd->MCUCtrl;
	RTMP_RING * pRing;
#ifdef RT_BIG_ENDIAN
	TXD_STRUC *pDestTxD;
	UCHAR tx_hw_info[TXD_SIZE];
#endif

	if (!OS_TEST_BIT(MCU_INIT, &ctl->flags))
	{
		return -1;
	}
	pRing = (RTMP_RING *)(&(pAd->FwDwloRing));
	FreeNum = GET_FWDWLORING_FREENO(pRing);
	if (FreeNum == 0)
	{
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_WARN,
            ("%s FreeNum == 0 (TxCpuIdx = %d, TxDmaIdx = %d, TxSwFreeIdx = %d)\n",
		    __FUNCTION__, pRing->TxCpuIdx, pRing->TxDmaIdx, pRing->TxSwFreeIdx));
		return NDIS_STATUS_FAILURE;
	}

	RTMP_SPIN_LOCK_IRQSAVE(&pRing->RingLock, &flags);

	RTMP_QueryPacketInfo(net_pkt, &PacketInfo, &pSrcBufVA, &SrcBufLen);
	if (pSrcBufVA == NULL)
	{
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pRing->RingLock, &flags);
		return NDIS_STATUS_FAILURE;
	}

	SwIdx = pRing->TxCpuIdx;

#ifdef RT_BIG_ENDIAN
	pDestTxD  = (TXD_STRUC *)pRing->Cell[SwIdx].AllocVa;
	NdisMoveMemory(&tx_hw_info[0], (UCHAR *)pDestTxD, TXD_SIZE);
 	pTxD = (TXD_STRUC *)&tx_hw_info[0];
#else
	pTxD  = (TXD_STRUC *)pRing->Cell[SwIdx].AllocVa;
#endif

#ifdef RT_BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif /* RT_BIG_ENDIAN */

	pRing->Cell[SwIdx].pNdisPacket = net_pkt;
	pRing->Cell[SwIdx].pNextNdisPacket = NULL;
	pRing->Cell[SwIdx].PacketPa = PCI_MAP_SINGLE(pAd, (pSrcBufVA),
                            (SrcBufLen), 0, RTMP_PCI_DMA_TODEVICE);

	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->SDLen0 = SrcBufLen;
	pTxD->SDLen1 = 0;
	pTxD->SDPtr0 = pRing->Cell[SwIdx].PacketPa;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

#ifdef RT_BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
	WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif

	/* flush dcache if no consistent memory is supported */
	RTMP_DCACHE_FLUSH(pRing->Cell[SwIdx].PacketPa, SrcBufLen);
	RTMP_DCACHE_FLUSH(pRing->Cell[SwIdx].AllocPa, TXD_SIZE);

   	/* Increase TX_CTX_IDX, but write to register later.*/
	INC_RING_INDEX(pRing->TxCpuIdx, CTL_RING_SIZE);

	if (IS_CMD_MSG_NEED_SYNC_WITH_FW_FLAG_SET(msg))
	{
	    AndesQueueTailCmdMsg(&ctl->ackq, msg, wait_ack);
	}
    else
    {
    AndesQueueTailCmdMsg(&ctl->tx_doneq, msg, tx_done);
    }

	if (!OS_TEST_BIT(MCU_INIT, &ctl->flags))
	{
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pRing->RingLock, &flags);
		return -1;
	}

	HIF_IO_WRITE32(pAd, pRing->hw_cidx_addr, pRing->TxCpuIdx);

	RTMP_SPIN_UNLOCK_IRQRESTORE(&pRing->RingLock, &flags);

	return ret;
}
#endif /* defined(MT7615) || defined(MT7622) */
#endif /* defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT) */

static VOID EventExtCmdResult(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                    (struct _EVENT_EXT_CMD_RESULT_T *)Data;


	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->ucExTenCID));

	EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.u4Status = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->u4Status));
}


#if defined(MT7636) && defined(TXBF_SUPPORT)
INT32 CmdETxBfSoundingPeriodicTriggerCtrl(
                    RTMP_ADAPTER    *pAd,
                    UCHAR			SndgEn,
                    UCHAR			SndgBW,
                    UCHAR			NDPAMcs,
                    UINT32          u4SNDPeriod,
                    UINT32          wlanidx)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T ETxBfSdPeriodicTriggerCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};


	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: Sounding trigger enable = %d\n", __FUNCTION__, SndgEn));

	msg = AndesAllocCmdMsg(pAd, sizeof(ETxBfSdPeriodicTriggerCtrl));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    os_zero_mem(&ETxBfSdPeriodicTriggerCtrl, sizeof(ETxBfSdPeriodicTriggerCtrl));

    ETxBfSdPeriodicTriggerCtrl.ucWlanIdx        = (wlanidx);
    ETxBfSdPeriodicTriggerCtrl.ucWMMIdx         = (1);
    ETxBfSdPeriodicTriggerCtrl.ucBW             = (SndgBW);
    ETxBfSdPeriodicTriggerCtrl.u2NDPARateCode   = cpu2le16(NDPAMcs);
    ETxBfSdPeriodicTriggerCtrl.u2NDPRateCode    = cpu2le16(8);      // MCS8
    ETxBfSdPeriodicTriggerCtrl.u4SoundingInterval = cpu2le32(u4SNDPeriod);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr,
        ((SndgEn) ? EXT_CMD_BF_SOUNDING_START : EXT_CMD_BF_SOUNDING_STOP));
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    AndesInitCmdMsg(msg, attr);

	AndesAppendCmdMsg(msg, (char *)&ETxBfSdPeriodicTriggerCtrl,
                        sizeof(ETxBfSdPeriodicTriggerCtrl));

	ret = AndesSendCmdMsg(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}
#endif /* MT7636 && TXBF_SUPPORT */


#ifdef UNIFY_FW_CMD
static VOID AndesMTFillTxDHeader(struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    TMAC_TXD_L *txd;

    txd = (TMAC_TXD_L *)OS_PKT_HEAD_BUF_EXTEND(net_pkt, sizeof(TMAC_TXD_L));
    NdisZeroMemory(txd, sizeof(TMAC_TXD_L));

	txd->TxD0.TxByteCount = GET_OS_PKT_LEN(net_pkt);
	txd->TxD0.p_idx = (msg->pq_id & 0x8000) >> 15;
	txd->TxD0.q_idx = (msg->pq_id & 0x7c00) >> 10;
    txd->TxD1.ft = 0x1;
    txd->TxD1.hdr_format = 0x1;
    
    if (msg->attr.type == MT_FW_SCATTER)
    {
        txd->TxD1.pkt_ft = TMI_PKT_FT_HIF_FW;
    }
    else
    {
        txd->TxD1.pkt_ft = TMI_PKT_FT_HIF_CMD;
    }
    return;
}
#endif /* UNIFY_FW_CMD */

VOID AndesMTFillCmdHeader(struct cmd_msg *msg, VOID *pkt)
{
	FW_TXD *fw_txd = NULL;
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;
#ifndef  UNIFY_FW_CMD
	struct MCU_CTRL *Ctl = &pAd->MCUCtrl;
#endif /*UNIFY_FW_CMD*/
	PNDIS_PACKET net_pkt = (PNDIS_PACKET) pkt;

#ifdef UNIFY_FW_CMD
    fw_txd = (FW_TXD *)OS_PKT_HEAD_BUF_EXTEND(net_pkt, sizeof(FW_TXD));
    AndesMTFillTxDHeader(msg, net_pkt);
#else

    if (Ctl->Stage == FW_RUN_TIME)
    {
        fw_txd = (FW_TXD *)OS_PKT_HEAD_BUF_EXTEND(net_pkt, sizeof(*fw_txd));
    }
    else
    {
        if (IS_MT7615(pAd) || IS_MT7622(pAd))
        {
            fw_txd = (FW_TXD *)OS_PKT_HEAD_BUF_EXTEND(net_pkt,
                                pAd->chipCap.cmd_header_len);
        }
        else
        {
            fw_txd = (FW_TXD *)OS_PKT_HEAD_BUF_EXTEND(net_pkt, 12);
        }
    }
#endif /* UNIFY_FW_CMD */


#ifndef UNIFY_FW_CMD
#if defined(MT7615) || defined(MT7622)
		/*
		 * minium packet shouldn't less then length of TXD
		 * added padding bytes if packet length less then length of TXD.
		 */
		if (GET_OS_PKT_LEN(net_pkt) < sizeof(FW_TXD))
		{
            OS_PKT_TAIL_BUF_EXTEND(net_pkt,
                    (sizeof(*fw_txd) - GET_OS_PKT_LEN(net_pkt)));
        }
#endif /* defined(MT7615) || defined(MT7622) */
#endif /* UNIFY_FW_CMD */
#ifdef UNIFY_FW_CMD
	if (IS_MT7615(pAd) || IS_MT7622(pAd))
	{
		NdisZeroMemory(fw_txd, sizeof(FW_TXD));
		fw_txd->fw_txd_0.field.length =
                    GET_OS_PKT_LEN(net_pkt) - sizeof(TMAC_TXD_L);
	}
	else
	{
		fw_txd->fw_txd_0.field.length = GET_OS_PKT_LEN(net_pkt);
	}
#else
	 	fw_txd->fw_txd_0.field.length = GET_OS_PKT_LEN(net_pkt);
#endif


	fw_txd->fw_txd_0.field.pq_id = msg->pq_id;
    fw_txd->fw_txd_1.field.cid = msg->attr.type;

	fw_txd->fw_txd_1.field.pkt_type_id = PKT_ID_CMD;
    fw_txd->fw_txd_1.field.set_query = IS_CMD_MSG_NA_FLAG_SET(msg) ?
                            CMD_NA : IS_CMD_MSG_SET_QUERY_FLAG_SET(msg);

#if (defined(MT7615) || defined(MT7622)) && !defined(UNIFY_FW_CMD)
    fw_txd->fw_txd_1.field1.seq_num = msg->seq;

    if (msg->attr.type == MT_FW_SCATTER)
    {
        fw_txd->fw_txd_1.field1.pkt_ft = TMI_PKT_FT_HIF_FW;
    }
    else
    {
        fw_txd->fw_txd_1.field1.pkt_ft = TMI_PKT_FT_HIF_CMD;
    }
#else
    fw_txd->fw_txd_1.field.seq_num = msg->seq;
#endif /* (defined(MT7615) || defined(MT7622)) && !defined(UNIFY_FW_CMD) */

	fw_txd->fw_txd_2.field.ext_cid = msg->attr.ext_type;

#if defined(MT7615) || defined(MT7622)
    if (IS_MT7615(pAd) || IS_MT7622(pAd))
    {

        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s: mcu_dest(%d):%s\n", __FUNCTION__, msg->attr.mcu_dest,
                (msg->attr.mcu_dest == HOST2N9) ? "HOST2N9" : "HOST2CR4"));

		if (msg->attr.mcu_dest == HOST2N9)
        {
			fw_txd->fw_txd_2.field.ucS2DIndex = HOST2N9;
		}
		else
		{
			fw_txd->fw_txd_2.field.ucS2DIndex = HOST2CR4;
		}
	}
#endif /* defined(MT7615) || defined(MT7622) */

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: fw_txd: 0x%x 0x%x 0x%x, Length=%d\n", __FUNCTION__,
        fw_txd->fw_txd_0.word, fw_txd->fw_txd_1.word,
        fw_txd->fw_txd_2.word, fw_txd->fw_txd_0.field.length));

    if ((IS_EXT_CMD_AND_SET_NEED_RSP(msg)) && !(IS_CMD_MSG_NA_FLAG_SET(msg)))
    {
		fw_txd->fw_txd_2.field.ext_cid_option = EXT_CID_OPTION_NEED_ACK;
	}
	else
	{
		fw_txd->fw_txd_2.field.ext_cid_option = EXT_CID_OPTION_NO_NEED_ACK;
	}

	fw_txd->fw_txd_0.word = cpu2le32(fw_txd->fw_txd_0.word);
	fw_txd->fw_txd_1.word = cpu2le32(fw_txd->fw_txd_1.word);
	fw_txd->fw_txd_2.word = cpu2le32(fw_txd->fw_txd_2.word);

#ifdef CONFIG_TRACE_SUPPORT
	TRACE_MCU_CMD_INFO(fw_txd->fw_txd_0.field.length,
	    fw_txd->fw_txd_0.field.pq_id, fw_txd->fw_txd_1.field.cid,
	    fw_txd->fw_txd_1.field.pkt_type_id, fw_txd->fw_txd_1.field.set_query,
	    fw_txd->fw_txd_1.field.seq_num, fw_txd->fw_txd_2.field.ext_cid,
	    fw_txd->fw_txd_2.field.ext_cid_option,
		(char *)(GET_OS_PKT_DATAPTR(net_pkt)), GET_OS_PKT_LEN(net_pkt));
#endif /* CONFIG_TRACE_SUPPORT */
}


static VOID EventChPrivilegeHandler(RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
	struct MCU_CTRL *ctl = &pAd->MCUCtrl;
	UINT32 Value;

	if (IS_MT7603(pAd) || IS_MT7628(pAd)  || IS_MT76x6(pAd) || IS_MT7637(pAd))
	{
		RTMP_IO_READ32(pAd, RMAC_RMCR, &Value);

		if (ctl->RxStream0 == 1)
        {
            Value |= RMAC_RMCR_RX_STREAM_0;
        }
        else
        {
            Value &= ~RMAC_RMCR_RX_STREAM_0;
        }
        if (ctl->RxStream1 == 1)
        {
            Value |= RMAC_RMCR_RX_STREAM_1;
        }
        else
        {
            Value &= ~RMAC_RMCR_RX_STREAM_1;
        }
		RTMP_IO_WRITE32(pAd, RMAC_RMCR, Value);
	}

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s\n", __FUNCTION__));
}


#ifdef MT_DFS_SUPPORT //Jelly20140123
static VOID ExtEventCacEndHandler(RTMP_ADAPTER *pAd,
                            UINT8 *Data, UINT32 Length)
{
    struct _EXT_EVENT_CAC_END_T *pExtEventCacEnd =
                                            (struct _EXT_EVENT_CAC_END_T *)Data;
    UCHAR ucRddIdx = pExtEventCacEnd->ucRddIdx;
	DfsCacEndHandle(pAd, ucRddIdx);
}
static VOID ExtEventRddReportHandler(RTMP_ADAPTER *pAd,
                            UINT8 *Data, UINT32 Length)
{
    struct _EXT_EVENT_RDD_REPORT_T *pExtEventRddReport =
                                        (struct _EXT_EVENT_RDD_REPORT_T *)Data;
    UCHAR ucRddIdx = pExtEventRddReport->ucRddIdx;
	WrapDfsRddReportHandle(pAd, ucRddIdx);
}

#endif

#ifdef INTERNAL_CAPTURE_SUPPORT
NTSTATUS WifiSpectrumRawDataDumpHandler(PRTMP_ADAPTER pAd, PCmdQElmt CMDQelmt)                            
{

    ExtEventWifiSpectrumRawDataDumpHandler(pAd, CMDQelmt->buffer, CMDQelmt->bufferlength);

    return 0;
}

VOID ExtEventWifiSpectrumRawDataDumpHandler(RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)                            
{
    PUINT32 pData = NULL;
    SHORT I_0, Q_0, LNA, LPF;
    UINT32 i, CaptureNode;
    UCHAR msg[64] , retval;
    RTMP_OS_FS_INFO osFSInfo;
    EVENT_WIFI_ICAP_DUMP_RAW_DATA_T *prIcapGetEvent = (EVENT_WIFI_ICAP_DUMP_RAW_DATA_T *)Data;
    pAd->WifiSpectrumStatus = NDIS_STATUS_FAILURE;   

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n",__FUNCTION__));

    os_alloc_mem(pAd, (UCHAR **)&pData, WIFISPECTRUM_EVENT_DATA_SAMPLE * sizeof(UINT32));
    NdisZeroMemory(pData, WIFISPECTRUM_EVENT_DATA_SAMPLE*sizeof(UINT32));
  
    CaptureNode = Get_Icap_WifiSpec_Capture_Node_Info(pAd);
  
    RtmpOSFSInfoChange(&osFSInfo, TRUE); // Change limits of authority in order to read/write file

    for (i=0;i < WIFISPECTRUM_EVENT_DATA_SAMPLE; i++)
    {        
        pData[i] = prIcapGetEvent->Data[i];
                
        if ((CaptureNode == WF0_ADC) || (CaptureNode == WF1_ADC) 
            || (CaptureNode == WF2_ADC) || (CaptureNode == WF3_ADC))//Dump 1-way RXADC
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
            ("%s : Dump 1-way RXADC\n", __FUNCTION__));      
            
            //Parse and dump I/Q data
            Q_0 = (pData[i] & 0x3FF);
            I_0 = ((pData[i] & (0x3FF<<10))>>10);
            if (Q_0 >= 512)
            {
                Q_0 -= 1024;
            }    
            if (I_0 >= 512)
            {
                I_0 -= 1024;
            }    
            sprintf(msg, "%+04d\t%+04d\n", I_0, Q_0);
            retval = RtmpOSFileWrite(pAd->Srcf_IQ, (RTMP_STRING *)msg, strlen(msg));

            //Parse and dump LNA/LPF data
            LNA = ((pData[i] & (0x3<<28))>>28);
            LPF = ((pData[i] & (0xF<<24))>>24);
            sprintf(msg, "%+04d\t%+04d\n", LNA, LPF);
            retval = RtmpOSFileWrite(pAd->Srcf_Gain, (RTMP_STRING *)msg, strlen(msg));
        }          
        else if ((CaptureNode == WF0_FIIQ) || (CaptureNode == WF1_FIIQ) 
                 || (CaptureNode == WF2_FIIQ) || (CaptureNode == WF3_FIIQ)
                 || (CaptureNode == WF0_FDIQ) || (CaptureNode == WF1_FDIQ)
                 || (CaptureNode == WF2_FDIQ) || (CaptureNode == WF3_FDIQ))//Dump 1-way RXIQC
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
            ("%s : Dump 1-way RXIQC\n", __FUNCTION__));
            
            //Parse and dump I/Q data
            Q_0 = (pData[i] & 0xFFF);
            I_0 = ((pData[i] & (0xFFF<<12))>>12);
            if (Q_0 >= 2048)
            {
                Q_0 -= 4096;
            }    
            if (I_0 >= 2048)
            {
                I_0 -= 4096;
            }    
            sprintf(msg, "%+04d\t%+04d\n", I_0, Q_0);
            retval = RtmpOSFileWrite(pAd->Srcf_IQ, (RTMP_STRING *)msg, strlen(msg));

            //Parse and dump LNA/LPF data
            LNA = ((pData[i] & (0x3<<28))>>28);
            LPF = ((pData[i] & (0xF<<24))>>24);
            sprintf(msg, "%+04d\t%+04d\n", LNA, LPF);
            retval = RtmpOSFileWrite(pAd->Srcf_Gain, (RTMP_STRING *)msg, strlen(msg));
        }    
   
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
        ("%s : 0x%08x\n", __FUNCTION__, *(pData + i)));   
    }
    
    RtmpOSFSInfoChange(&osFSInfo, FALSE); // Change limits of authority in order to read/write file

    pAd->WifiSpectrumDataCounter += 1;
    if(pAd->WifiSpectrumDataCounter == WIFISPECTRUM_SYSRAM_SIZE)
    {
        pAd->WifiSpectrumDataCounter = 0;

        pAd->WifiSpectrumStatus = NDIS_STATUS_SUCCESS;
            
        RTMP_OS_COMPLETE(&pAd->WifiSpectrumDumpDataDone);
        
        retval = RtmpOSFileClose(pAd->Srcf_IQ);
        if (retval)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
            ("--> Error %d closing %s\n", -retval, pAd->Src_IQ));
            goto error;
        }
        
        retval = RtmpOSFileClose(pAd->Srcf_Gain);
        if (retval)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
            ("--> Error %d closing %s\n", -retval, pAd->Src_Gain));
            goto error;
        }
    }
    
    os_free_mem(pData);
      
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n",__FUNCTION__));

    return;
error:
    os_free_mem(pData);
  
}                           
#endif/*INTERNAL_CAPTURE_SUPPORT*/

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
static VOID ExtEventThroughputBurst(RTMP_ADAPTER *pAd,
                            UINT8 *Data, UINT32 Length)
{
	BOOLEAN fgEnable = FALSE;
	if (Data != NULL)
	{
		fgEnable = (BOOLEAN)(*Data);
		pAd->bDisableRtsProtect = fgEnable;

#if !defined(COMPOS_WIN) || !defined(COMPOS_TESTMODE_WIN)
        if (pAd->bDisableRtsProtect)
        {
            RTMP_UPDATE_RTS_THRESHOLD(pAd,
                MAX_RTS_PKT_THRESHOLD, MAX_RTS_THRESHOLD);
        }
        else
        {
            RTMP_UPDATE_RTS_THRESHOLD(pAd, pAd->CommonCfg.RtsPktThreshold,
                                            pAd->CommonCfg.RtsThreshold);
        }
#endif
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		            ("%s::%d\n", __FUNCTION__, fgEnable));
	}
    else
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                    ("%s:: Data is NULL\n", __FUNCTION__));
	}

}
#endif /* RACTRL_FW_OFFLOAD_SUPPORT */


static VOID ExtEventAssertDumpHandler(RTMP_ADAPTER *pAd, UINT8 *Data,
                                UINT32 Length, EVENT_RXD *event_rxd)
{
	struct _EXT_EVENT_ASSERT_DUMP_T *pExtEventAssertDump =
                                        (struct _EXT_EVENT_ASSERT_DUMP_T *)Data;
	static int assert_line = 0;

	if (strncmp(";<ASSERT>", pExtEventAssertDump->aucBuffer, strlen(";<ASSERT>")) == 0)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
			("**************************************************\n"));
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
			("%s\n", pExtEventAssertDump->aucBuffer));
		assert_line++;
	}
	else if ((assert_line == 1) || (assert_line == 2))
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s\n", pExtEventAssertDump->aucBuffer));
		assert_line++;
	}

	if (assert_line == 3)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
			("**************************************************\n\n"));
		assert_line = 0;
	}


#ifdef FW_DUMP_SUPPORT

	if (!pAd->fw_dump_buffer)
	{
		os_alloc_mem(pAd, &pAd->fw_dump_buffer, pAd->fw_dump_max_size);
		pAd->fw_dump_size = 0;
		pAd->fw_dump_read = 0;

		if (pAd->fw_dump_buffer)
		{
			if (event_rxd->fw_rxd_2.field.s2d_index == N92HOST)
			{
			    RTMP_OS_FWDUMP_PROCCREATE(pAd, "_N9");
			}
            else if (event_rxd->fw_rxd_2.field.s2d_index == CR42HOST)
			{
			    RTMP_OS_FWDUMP_PROCCREATE(pAd, "_CR4");
			}
            else
            {
                RTMP_OS_FWDUMP_PROCCREATE(pAd, "\0");
            }
		}
		else
		{
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
				("%s: cannot alloc mem for FW dump\n", __func__));
		}
	}

	if (pAd->fw_dump_buffer)
	{
		if ((pAd->fw_dump_size+Length) <= pAd->fw_dump_max_size)
		{
			os_move_mem(pAd->fw_dump_buffer+pAd->fw_dump_size, Data, Length);
			pAd->fw_dump_size += Length;
		}
		else
		{
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
				("%s: FW dump size too big\n", __func__));
		}
	}

#endif

}


#if defined(MT_MAC) && (!defined(MT7636)) && defined(TXBF_SUPPORT)
VOID ExtEventBfStatusRead(RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
    struct _EXT_EVENT_BF_STATUS_T *pExtEventBfStatus = (struct _EXT_EVENT_BF_STATUS_T *)Data;
    struct _EXT_EVENT_IBF_STATUS_T *pExtEventIBfStatus = (struct _EXT_EVENT_IBF_STATUS_T *)Data;

#if defined(CONFIG_ATE) && defined(CONFIG_QA)
	if (ATE_ON(pAd))
		HQA_BF_INFO_CB(pAd, Data, Length);
#endif
    switch (pExtEventBfStatus->ucBfDataFormatID)
    {
    case BF_PFMU_TAG:
        TxBfProfileTagPrint(pAd,
                            pExtEventBfStatus->fgBFer,
                            pExtEventBfStatus->aucBuffer);
        break;
    case BF_PFMU_DATA:
        TxBfProfileDataPrint(pAd,
                             pExtEventBfStatus->u2subCarrIdx,
                             pExtEventBfStatus->aucBuffer);
        break;
    case BF_PFMU_PN:
        TxBfProfilePnPrint(pExtEventBfStatus->ucBw,
                           pExtEventBfStatus->aucBuffer);
        break;
    case BF_PFMU_MEM_ALLOC_MAP:
        TxBfProfileMemAllocMap(pExtEventBfStatus->aucBuffer);
        break;
    case BF_STAREC:
        StaRecBfRead(pAd, pExtEventBfStatus->aucBuffer);
        break;
    case BF_CAL_PHASE:
        iBFPhaseCalReport(pAd, 
                          pExtEventIBfStatus->ucGroup_L_M_H,
                          pExtEventIBfStatus->ucGroup,
                          pExtEventIBfStatus->fgSX2,
                          pExtEventIBfStatus->ucStatus,
                          pExtEventIBfStatus->ucPhaseCalType,
                          pExtEventIBfStatus->aucBuffer);
        break;
    default:
        break;
    }
}
#endif /* MT_MAC && TXBF_SUPPORT */


static VOID ExtEventFwLog2HostHandler(RTMP_ADAPTER *pAd, UINT8 *Data,
                                UINT32 Length, EVENT_RXD *event_rxd)
{
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s: s2d_index = 0x%x\n", __FUNCTION__,
            event_rxd->fw_rxd_2.field.s2d_index));

	if (event_rxd->fw_rxd_2.field.s2d_index == N92HOST)
	{
#ifdef CAL_TO_FLASH_SUPPORT
		if(pAd->KtoFlashDebug)
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                                ("%s", Data));
        else
#endif /*CAL_TO_FLASH_SUPPORT*/
        	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                                ("N9 LOG: %s\n", Data));
	}
	else if (event_rxd->fw_rxd_2.field.s2d_index == CR42HOST)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                                ("CR4 LOG: %s\n", Data));
	}
	else
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                        ("unknow MCU LOG: %s", Data));
	}
}

#ifdef MT_MAC_BTCOEX
static VOID ExtEventBTCoexHandler(RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
	UCHAR SubOpCode;
	MAC_TABLE_ENTRY *pEntry;
	struct _EVENT_EXT_COEXISTENCE_T *coext_event_t =
                                    (struct _EVENT_EXT_COEXISTENCE_T *)Data;
	SubOpCode = coext_event_t->ucSubOpCode;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("SubOpCode: 0x%x\n", coext_event_t->ucSubOpCode));
	hex_dump("Coex Event payload ", coext_event_t->aucBuffer, Length);

	if (SubOpCode == 0x01)
	{
		struct _EVENT_COEX_CMD_RESPONSE_T *CoexResp =
            (struct _EVENT_COEX_CMD_RESPONSE_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->Cmd_Resp=0x%x\n", CoexResp->u4Status));

	}
	else if (SubOpCode == 0x02)
	{
		struct _EVENT_COEX_REPORT_COEX_MODE_T *CoexReportMode =
            (struct _EVENT_COEX_REPORT_COEX_MODE_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->SupportCoexMode=0x%x\n", CoexReportMode->u4SupportCoexMode));
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->CurrentCoexMode=0x%x\n", CoexReportMode->u4CurrentCoexMode));
		pAd->BtCoexSupportMode = ((CoexReportMode->u4SupportCoexMode) & 0x3);
		pAd->BtCoexMode = ((CoexReportMode->u4CurrentCoexMode) & 0x3);
	}
	else if (SubOpCode == 0x03)
	{
		struct _EVENT_COEX_MASK_OFF_TX_RATE_T *CoexMaskTxRate =
            (struct _EVENT_COEX_MASK_OFF_TX_RATE_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->MASK_OFF_TX_RATE=0x%x\n", CoexMaskTxRate->ucOn));

	}
	else if (SubOpCode == 0x04)
	{
		struct _EVENT_COEX_CHANGE_RX_BA_SIZE_T *CoexChgBaSize =
            (struct _EVENT_COEX_CHANGE_RX_BA_SIZE_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->Change_BA_Size ucOn=%d \n", CoexChgBaSize->ucOn));
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->Change_BA_Size=0x%x\n", CoexChgBaSize->ucRXBASize));
		pEntry = &pAd->MacTab.Content[BSSID_WCID];

		if (CoexChgBaSize->ucOn == 1)
		{
			BA_REC_ENTRY *pBAEntry = NULL;
			UCHAR Idx;

			Idx = pEntry->BARecWcidArray[0];
			pBAEntry = &pAd->BATable.BARecEntry[Idx];
			pAd->BtCoexBASize = CoexChgBaSize->ucRXBASize;

			if (pBAEntry->BAWinSize == 0)
            {
                pBAEntry->BAWinSize =
                            pAd->CommonCfg.REGBACapability.field.RxBAWinLimit;
            }
			if (pAd->BtCoexBASize!= 0 &&
               (pAd->BtCoexBASize < pAd->CommonCfg.REGBACapability.field.RxBAWinLimit))
			{
				pAd->CommonCfg.BACapability.field.RxBAWinLimit = pAd->BtCoexBASize;
				BARecSessionTearDown(pAd, BSSID_WCID, 0, FALSE);
				MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                    ("COEX: TDD mode: Set RxBASize to %d\n", pAd->BtCoexBASize));
			}
		}
		else
		{
			pAd->CommonCfg.BACapability.field.RxBAWinLimit =
                        pAd->CommonCfg.REGBACapability.field.RxBAWinLimit;
			BARecSessionTearDown(pAd, BSSID_WCID, 0, FALSE);
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("COEX: TDD mode: Set RxBASize to %d\n", pAd->BtCoexOriBASize));
		}
	}
	else if (SubOpCode == 0x05)
	{
		struct _EVENT_COEX_LIMIT_BEACON_SIZE_T *CoexLimitBeacon =
            (struct _EVENT_COEX_LIMIT_BEACON_SIZE_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->COEX_LIMIT_BEACON_SIZE ucOn =%d\n", CoexLimitBeacon->ucOn));
		pAd->BtCoexBeaconLimit = CoexLimitBeacon->ucOn;
	}
	else if (SubOpCode == 0x06)
	{
		struct _EVENT_COEX_EXTEND_BTO_ROAMING_T *CoexExtendBTORoam =
            (struct _EVENT_COEX_EXTEND_BTO_ROAMING_T *)coext_event_t->aucBuffer;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("--->EVENT_COEX_EXTEND_BNCTO_ROAMING ucOn =%d\n",
                                    CoexExtendBTORoam->ucOn));

	}
}
#endif

#ifdef BCN_OFFLOAD_SUPPORT
VOID RT28xx_UpdateBcnAndTimToMcu(
    IN RTMP_ADAPTER *pAd,
    VOID *wdev_void,
    IN ULONG FrameLen,
    IN ULONG UpdatePos,
    IN UCHAR UpdatePktType)
{
    BCN_BUF_STRUC *bcn_buf = NULL;
#ifdef CONFIG_AP_SUPPORT
    TIM_BUF_STRUC *tim_buf = NULL;
#endif
    UCHAR *buf;
    INT len;
    PNDIS_PACKET *pkt = NULL;

    CMD_BCN_OFFLOAD_T bcn_offload;

    struct wifi_dev *wdev = (struct wifi_dev *)wdev_void;
    BOOLEAN bSntReq = FALSE;
    UCHAR TimIELocation = 0, CsaIELocation = 0;

    NdisZeroMemory(&bcn_offload, sizeof(CMD_BCN_OFFLOAD_T));

    if (!wdev)
    {   
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s(): wdev is NULL!\n", __FUNCTION__));
        return;
    } 
    bcn_buf = &wdev->bcn_buf;
    if (!bcn_buf)
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
               ("%s(): bcn_buf is NULL!\n", __FUNCTION__));
        return;
    }

    if (UpdatePktType == PKT_BCN)
    {
   
        pkt = bcn_buf->BeaconPkt;
        bSntReq = bcn_buf->bBcnSntReq;
        TimIELocation = bcn_buf->TimIELocationInBeacon;
        CsaIELocation = bcn_buf->CsaIELocationInBeacon;
    }
#ifdef CONFIG_AP_SUPPORT
    else /* tim pkt case in AP mode. */
    {
        if (pAd->OpMode == OPMODE_AP)
        {
            tim_buf = &wdev->bcn_buf.tim_buf;
        }
        if (!tim_buf)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                    ("%s(): tim_buf is NULL!\n", __FUNCTION__));
            return;
        }
        pkt = tim_buf->TimPkt;
        bSntReq = tim_buf->bTimSntReq;

        TimIELocation = bcn_buf->TimIELocationInTim;
        CsaIELocation = bcn_buf->CsaIELocationInBeacon;
    }
#endif /* CONFIG_AP_SUPPORT */

    if (pkt)
    {
        buf = (UCHAR *)GET_OS_PKT_DATAPTR(pkt);
        len = FrameLen + pAd->chipCap.tx_hw_hdr_len;//TXD & pkt content.

        bcn_offload.ucEnable = bSntReq;
        bcn_offload.ucWlanIdx = 0;//hardcode at present

        bcn_offload.ucOwnMacIdx = wdev->OmacIdx;
        bcn_offload.ucBandIdx = HcGetBandByWdev(wdev);
        bcn_offload.u2PktLength = len;
        bcn_offload.ucPktType = UpdatePktType;
#ifdef CONFIG_AP_SUPPORT
        bcn_offload.u2TimIePos = TimIELocation + pAd->chipCap.tx_hw_hdr_len;
        bcn_offload.u2CsaIePos = CsaIELocation + pAd->chipCap.tx_hw_hdr_len;
        bcn_offload.ucCsaCount = wdev->csa_count;
#endif
        NdisCopyMemory(bcn_offload.acPktContent, buf, len);
        MtCmdBcnOffloadSet(pAd, bcn_offload);
    }
    else
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s(): BeaconPkt is NULL!\n", __FUNCTION__));
    }
}
#endif /*BCN_OFFLOAD*/

#ifdef PRETBTT_INT_EVENT_SUPPORT
static VOID ExtEventPretbttIntHandler(RTMP_ADAPTER *pAd,
                            UINT8 *Data, UINT32 Length)
{
    if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET))      ||
        (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))   ||
        (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
    {
        return;
    }

    RTMP_HANDLE_PRETBTT_INT_EVENT(pAd);
}
#endif

#ifdef THERMAL_PROTECT_SUPPORT
static VOID EventThermalProtect(RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
	EXT_EVENT_THERMAL_PROTECT_T *EvtThermalProtect;
	UINT8 HLType;
    UINT8 Reason;
#if !defined(MT7615) && !defined(MT7622)
	UINT32 ret;
#endif
    struct wifi_dev *wdev;
    POS_COOKIE pObj;
    
    pObj = (POS_COOKIE) pAd->OS_Cookie;
    
#ifdef CONFIG_AP_SUPPORT
    wdev = &pAd->ApCfg.MBSSID[pObj->ioctl_if].wdev;
#endif


	EvtThermalProtect = (EXT_EVENT_THERMAL_PROTECT_T *)Data;

	HLType = EvtThermalProtect->ucHLType;
    Reason = EvtThermalProtect->ucReason;
	pAd->last_thermal_pro_temp = EvtThermalProtect->cCurrentTemp;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s: HLType = %d, CurrentTemp = %d, Reason = %d\n",
            __FUNCTION__, HLType, pAd->last_thermal_pro_temp, Reason));

    if (Reason == THERAML_PROTECTION_REASON_RADIO)
    {
        pAd->force_radio_off = TRUE;
        //AsicRadioOnOffCtrl(pAd, HcGetBandByWdev(wdev), WIFI_RADIO_OFF);

        /* Set Radio off Process*/
        //Set_RadioOn_Proc(pAd, "0");
        RTMP_SET_THERMAL_RADIO_OFF(pAd);

        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Radio Off due to too high temperature. \n"));

        
    }
    
#if !defined(MT7615) && !defined(MT7622)
    RTMP_SEM_EVENT_WAIT(&pAd->AutoRateLock, ret);
	if (HLType == HIGH_TEMP_THD)
	{
		pAd->force_one_tx_stream = TRUE;
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                            ("Switch TX to 1 stram\n"));
	}
	else
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                            ("Switch TX to 2 stram\n"));
		pAd->force_one_tx_stream = FALSE;
	}
    pAd->fgThermalProtectToggle = TRUE;

    RTMP_SEM_EVENT_UP(&pAd->AutoRateLock);
#endif /* !defined(MT7615) && !defined(MT7622) */

}
#endif
#ifdef CFG_TDLS_SUPPORT
static VOID ExtEventTdlsBackToBaseHandler(RTMP_ADAPTER *pAd,
                                UINT8 *Data, UINT32 Length)
{
	P_EXT_EVENT_TDLS_STATUS_T prEventExtCmdResult =
                                        (P_EXT_EVENT_TDLS_STATUS_T)Data;
	INT chsw_fw_resp = 0;
    chsw_fw_resp = prEventExtCmdResult->ucResultId;

	switch(chsw_fw_resp)
	{
	case 1:
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN,
                ("RX RSP_EVT_TYPE_TDLS_STAY_TIME_OUT\n"));
		PCFG_TDLS_STRUCT pCfgTdls =
                    &pAd->StaCfg[0].wpa_supplicant_info.CFG_Tdls_info;
		cfg_tdls_chsw_resp(pAd, pCfgTdls->CHSWPeerMacAddr,
                pCfgTdls->ChSwitchTime,pCfgTdls->ChSwitchTimeout,0);
	break;
	case 2:
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN,
            ("RX RSP_EVT_TYPE_TDLS_BACK_TO_BASE_CHANNEL\n"));
		pAd->StaCfg[0].wpa_supplicant_info.CFG_Tdls_info.IamInOffChannel = FALSE;
	break;
	default:
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN,
            ("%s : unknown event type %d \n",__FUNCTION__,chsw_fw_resp));
	break;
	}
}

#endif /* CFG_TDLS_SUPPORT */

#ifdef BA_TRIGGER_OFFLOAD
static VOID ExtEventBaTriggerHandler(RTMP_ADAPTER *pAd,
                                UINT8 *Data, UINT32 Length)
{
    P_CMD_BA_TRIGGER_EVENT_T prEventExtBaTrigger =
                                        (P_CMD_BA_TRIGGER_EVENT_T)Data;
	STA_TR_ENTRY *tr_entry = NULL;
    UINT8 wcid = 0;
    UINT8 tid = 0;

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("RX P_CMD_BA_TRIGGER_EVENT_T: Wcid=%d, Tid=%d\n",
            prEventExtBaTrigger->ucWlanIdx, prEventExtBaTrigger->ucTid));

    wcid = prEventExtBaTrigger->ucWlanIdx;
    if (!VALID_UCAST_ENTRY_WCID(pAd, wcid))
        return;

    tid = prEventExtBaTrigger->ucTid;
    tr_entry = &pAd->MacTab.tr_entry[wcid];

	RTMP_BASetup(pAd, tr_entry, tid);
}
#endif /* BA_TRIGGER_OFFLOAD */

static VOID ExtEventTmrCalcuInfoHandler(
        RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
	P_EXT_EVENT_TMR_CALCU_INFO_T ptmr_calcu_info;
	TMR_FRM_STRUC *p_tmr_frm;

	ptmr_calcu_info = (P_EXT_EVENT_TMR_CALCU_INFO_T)Data;
	p_tmr_frm = (TMR_FRM_STRUC *)ptmr_calcu_info->aucTmrFrm;

	/*Tmr pkt comes to FW event, fw already take cares of the whole calculation.*/
	TmrReportParser(pAd, p_tmr_frm, TRUE);
}


static VOID ExtEventCswNotifyHandler(
        RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length)
{
    P_EXT_EVENT_CSA_NOTIFY_T csa_notify_event = (P_EXT_EVENT_CSA_NOTIFY_T)Data;
    struct wifi_dev *wdev;

    wdev = WdevSearchByOmacIdx(pAd, csa_notify_event->ucOwnMacIdx);

    if (!wdev)
    {
        return ;
    }
    wdev->csa_count = csa_notify_event->ucChannelSwitchCount;

    if ((HcIsRfSupport(pAd,RFIC_5GHZ))
            && (pAd->CommonCfg.bIEEE80211H == 1)
            && (pAd->Dot11_H.RDMode == RD_SWITCHING_MODE))
    {
#ifdef CONFIG_AP_SUPPORT
        pAd->Dot11_H.CSCount = pAd->Dot11_H.CSPeriod;
        ChannelSwitchingCountDownProc(pAd);
#endif
    }
}

static VOID ExtEventBssAcQPktNumHandler(RTMP_ADAPTER *pAd,
                                UINT8 *Data, UINT32 Length)
{
	P_EVENT_BSS_ACQ_PKT_NUM_T prEventBssAcQPktNum =
				(P_EVENT_BSS_ACQ_PKT_NUM_T)Data;
	UINT8 i = 0;
	UINT32 sum = 0;

	P_EVENT_PER_BSS_ACQ_PKT_NUM_T prPerBssInfo = NULL;


	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        	("RX ExtEventBssAcQPktNumHandler: u4BssMap=0x%08X\n",
		prEventBssAcQPktNum->u4BssMap));

	for (i = 0; (i < CR4_CFG_BSS_NUM) && (prEventBssAcQPktNum->u4BssMap & (1 << i)) ; i++)
	{
        	prPerBssInfo = &prEventBssAcQPktNum->bssPktInfo[i];

		sum =   prPerBssInfo->au4AcqPktCnt[WMM_AC_VI] 
        		+ prPerBssInfo->au4AcqPktCnt[WMM_AC_VO];
    
	        if (sum)
        	{
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("BSS[%d], AC_BK = %d, AC_BE = %d, AC_VI = %d, AC_VO = %d\n",
			i, 
			prPerBssInfo->au4AcqPktCnt[WMM_AC_BK],
			prPerBssInfo->au4AcqPktCnt[WMM_AC_BE],
			prPerBssInfo->au4AcqPktCnt[WMM_AC_VI],
			prPerBssInfo->au4AcqPktCnt[WMM_AC_VO]));
			pAd->OneSecondnonBEpackets += sum;
		}
	}
	mt_dynamic_wmm_be_tx_op(pAd, ONE_SECOND_NON_BE_PACKETS_THRESHOLD);

}

static BOOLEAN IsUnsolicitedEvent(EVENT_RXD *event_rxd)
{
    if ((GET_EVENT_FW_RXD_SEQ_NUM(event_rxd) == 0)                          ||
        (GET_EVENT_FW_RXD_EXT_EID(event_rxd) == EXT_EVENT_FW_LOG_2_HOST) 	||
        (GET_EVENT_FW_RXD_EXT_EID(event_rxd) == EXT_EVENT_THERMAL_PROTECT)  ||
        (GET_EVENT_FW_RXD_EXT_EID(event_rxd) == EXT_EVENT_ID_ASSERT_DUMP))
    {
        return TRUE;
    }
    return FALSE;
}


static VOID EventExtEventHandler(RTMP_ADAPTER *pAd, UINT8 ExtEID, UINT8 *Data,
                                        UINT32 Length, EVENT_RXD *event_rxd)
{
	switch (ExtEID)
	{
		case EXT_EVENT_CMD_RESULT:
			EventExtCmdResult(NULL, Data, Length);
			break;
		case EXT_EVENT_FW_LOG_2_HOST:
			ExtEventFwLog2HostHandler(pAd, Data, Length, event_rxd);
			break;
#ifdef MT_MAC_BTCOEX
		case EXT_EVENT_BT_COEX:
			ExtEventBTCoexHandler(pAd, Data, Length);
			break;
#endif /*MT_MAC_BTCOEX*/
#ifdef THERMAL_PROTECT_SUPPORT
		case EXT_EVENT_THERMAL_PROTECT:
			EventThermalProtect(pAd, Data, Length);
			break;
#endif
#ifdef PRETBTT_INT_EVENT_SUPPORT
        case EXT_EVENT_PRETBTT_INT:
            ExtEventPretbttIntHandler(pAd, Data, Length);
            break;
#endif /*PRETBTT_INT_EVENT_SUPPORT*/
#ifdef RACTRL_FW_OFFLOAD_SUPPORT
		case EXT_EVENT_RA_THROUGHPUT_BURST:
			ExtEventThroughputBurst(pAd, Data, Length);
			break;
#endif /* RACTRL_FW_OFFLOAD_SUPPORT */
		case EXT_EVENT_ID_ASSERT_DUMP:
			ExtEventAssertDumpHandler(pAd, Data, Length, event_rxd);
			break;
#ifdef CFG_TDLS_SUPPORT
		case EXT_EVENT_TDLS_STATUS:
			ExtEventTdlsBackToBaseHandler(pAd, Data, Length);
			break;
#endif
#if defined(MT_MAC) && defined(TXBF_SUPPORT)
		case EXT_EVENT_ID_BF_STATUS_READ:
			//ExtEventFwLog2HostHandler(pAd, Data, Length);
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
				("%s: EXT_EVENT_ID_BF_STATUS_READ\n", __FUNCTION__));
			ExtEventBfStatusRead(pAd, Data, Length);
			break;
#endif /* MT_MAC && TXBF_SUPPORT */
#ifdef CONFIG_ATE
		case EXT_EVENT_ID_RF_TEST:
			MT_ATERFTestCB(pAd, Data, Length);
			break;
#endif
#ifdef MT_DFS_SUPPORT
        case EXT_EVENT_ID_CAC_END: //Jelly20150123
            ExtEventCacEndHandler(pAd, Data, Length);
            break;
        case EXT_EVENT_ID_RDD_REPORT:
            ExtEventRddReportHandler(pAd, Data, Length);
            break;
#endif
#ifdef BA_TRIGGER_OFFLOAD
        case EXT_EVENT_ID_BA_TRIGGER:
            ExtEventBaTriggerHandler(pAd, Data, Length);
            break;
#endif /* BA_TRIGGER_OFFLOAD */

#ifdef INTERNAL_CAPTURE_SUPPORT
        case EXT_EVENT_ID_WIFISPECTRUM_ICAP_RAWDATA_DUMP:
            RTEnqueueInternalCmd(pAd, CMDTHRED_WIFISPECTRUM_RAWDATA_DUMP, (VOID*)Data, Length);
        break;
#endif /* INTERNAL_CAPTURE_SUPPORT */

        case EXT_EVENT_CSA_NOTIFY:
            ExtEventCswNotifyHandler(pAd, Data, Length);
            break;

        case EXT_EVENT_TMR_CALCU_INFO:
            ExtEventTmrCalcuInfoHandler(pAd, Data, Length);
            break;

        case EXT_EVENT_ID_BSS_ACQ_PKT_NUM:
            ExtEventBssAcQPktNumHandler(pAd, Data, Length);
            break;

		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
				("%s: Unknown Ext Event(%x)\n", __FUNCTION__, ExtEID));
			break;
	}


}

static VOID EventExtGenericEventHandler(UINT8 *Data)
{
	struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
			__FUNCTION__, EventExtCmdResult->ucExTenCID));

	EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);

	if (EventExtCmdResult->u4Status == CMD_RESULT_SUCCESS)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s: CMD Success\n", __FUNCTION__));
	}
	else if (EventExtCmdResult->u4Status == CMD_RESULT_NONSUPPORT)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s: CMD Non-Support\n", __FUNCTION__));
	}
	else
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s: CMD Fail!, EventExtCmdResult.u4Status = 0x%x\n",
				__FUNCTION__, EventExtCmdResult->u4Status));

	}
}

static VOID EventGenericEventHandler(UINT8 *Data)
{
	struct _INIT_EVENT_CMD_RESULT *EventCmdResult =
                                    (struct _INIT_EVENT_CMD_RESULT *)Data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s: EventCmdResult.ucCID = 0x%x\n",
			__FUNCTION__, EventCmdResult->ucCID));

	if (EventCmdResult->ucStatus == CMD_RESULT_SUCCESS)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("%s: CMD Success\n", __FUNCTION__));
	}
	else if (EventCmdResult->ucStatus == CMD_RESULT_NONSUPPORT)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("%s: CMD Non-Support\n", __FUNCTION__));
	}
	else
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("%s: CMD Fail!, EventCmdResult.ucStatus = 0x%x\n",
				__FUNCTION__, EventCmdResult->ucStatus));

	}
}


static VOID GenericEventHandler(UINT8 EID, UINT8 ExtEID, UINT8 *Data)
{
	switch (EID)
	{
		case EXT_EVENT:
			EventExtGenericEventHandler(Data);
			break;
		case GENERIC_EVENT:
			EventGenericEventHandler(Data);
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
				("%s: Unknown Event(%x)\n", __FUNCTION__, EID));
			break;
	}
}

static VOID UnsolicitedEventHandler(RTMP_ADAPTER *pAd, UINT8 EID, UINT8 ExtEID,
                            UINT8 *Data, UINT32 Length, EVENT_RXD *event_rxd)
{
	switch (EID)
	{
		case EVENT_CH_PRIVILEGE:
			EventChPrivilegeHandler(pAd, Data, Length);
			break;
		case EXT_EVENT:
			EventExtEventHandler(pAd, ExtEID, Data, Length, event_rxd);
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
				("%s: Unknown Event(%x)\n", __FUNCTION__, EID));
			break;
	}
}


static BOOLEAN IsRspLenVariableAndMatchSpecificMinLen(EVENT_RXD *event_rxd,
                                                struct cmd_msg *msg)
{
    if ((msg->attr.ctrl.expect_size <= GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd))
        && (msg->attr.ctrl.expect_size != 0) && IS_CMD_MSG_LEN_VAR_FLAG_SET(msg))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}



static BOOLEAN IsRspLenNonZeroAndMatchExpected(EVENT_RXD *event_rxd,
                                                struct cmd_msg *msg)
{
    if ((msg->attr.ctrl.expect_size == GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd))
        && (msg->attr.ctrl.expect_size != 0))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static VOID HandlSeq0AndOtherUnsolicitedEvents(RTMP_ADAPTER *pAd,
                        EVENT_RXD *event_rxd, PNDIS_PACKET net_pkt)
{
    UnsolicitedEventHandler(pAd,
        GET_EVENT_FW_RXD_EID(event_rxd),
        GET_EVENT_FW_RXD_EXT_EID(event_rxd),
        GET_EVENT_HDR_ADDR(net_pkt),
        GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd), event_rxd);
}


static void CompleteWaitCmdMsgOrFreeCmdMsg(struct cmd_msg *msg)
{

    if (IS_CMD_MSG_NEED_SYNC_WITH_FW_FLAG_SET(msg))
    {
		RTMP_OS_COMPLETE(&msg->ack_done);
	}
	else
	{
		DlListDel(&msg->list);
		AndesFreeCmdMsg(msg);
	}
}

static void FillRspPayloadLenAndDumpExpectLenAndRspLenInfo(
                    EVENT_RXD *event_rxd, struct cmd_msg *msg)
{
    /* Error occurs!!! dump info for debugging */
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
    ("expect response len(%d), command response len(%zd) invalid\n",
    msg->attr.ctrl.expect_size, GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd)));

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:cmd_type = 0x%x, ext_cmd_type = 0x%x, FW_RXD_EXT_EID = 0x%x\n",
            __FUNCTION__, msg->attr.type, msg->attr.ext_type,
                    GET_EVENT_FW_RXD_EXT_EID(event_rxd)));

    msg->attr.ctrl.expect_size = GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd);
}


static VOID HandleLayer1GenericEvent(UINT8 EID, UINT8 ExtEID, UINT8 *Data)
{
    GenericEventHandler(EID, ExtEID, Data);
}

static void CallEventHookHandlerOrDumpErrorMsg(EVENT_RXD *event_rxd,
                            struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    if (msg->attr.rsp.handler == NULL)
    {
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s(): rsp_handler is NULL!!!!(cmd_type = 0x%x, "
            "ext_cmd_type = 0x%x, FW_RXD_EXT_EID = 0x%x)\n",
            __FUNCTION__, msg->attr.type, msg->attr.ext_type,
                    GET_EVENT_FW_RXD_EXT_EID(event_rxd)));
        
        if (GET_EVENT_FW_RXD_EXT_EID(event_rxd) == 0)
        {
            HandleLayer1GenericEvent(GET_EVENT_FW_RXD_EID(event_rxd),
                                GET_EVENT_FW_RXD_EXT_EID(event_rxd),
                                GET_EVENT_HDR_ADDR(net_pkt));
        }
    }
    else
    {
        msg->attr.rsp.handler(msg, GET_EVENT_HDR_ADDR(net_pkt),
            GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd));
    }
}

static void FwDebugPurposeHandler(EVENT_RXD *event_rxd,
                    struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    /* hanle FW debug purpose only */
    CallEventHookHandlerOrDumpErrorMsg(event_rxd, msg, net_pkt);
}

static VOID HandleNormalLayer1Events(EVENT_RXD *event_rxd,
                 struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    CallEventHookHandlerOrDumpErrorMsg(event_rxd, msg, net_pkt);
}

static void EventLenVariableHandler(EVENT_RXD *event_rxd,
                    struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    /* hanle event len variable */
     HandleNormalLayer1Events(event_rxd, msg, net_pkt);
}


static void HandleLayer1Events(EVENT_RXD *event_rxd,
            struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{
    /* handler normal layer 1 event */
    if (IsRspLenNonZeroAndMatchExpected(event_rxd, msg))
    {
        HandleNormalLayer1Events(event_rxd, msg, net_pkt);
    }
    else if (IsRspLenVariableAndMatchSpecificMinLen(event_rxd, msg))
    {
        /* hanle event len variable */
        EventLenVariableHandler(event_rxd, msg, net_pkt);
    }
    else if (IS_IGNORE_RSP_PAYLOAD_LEN_CHECK(msg))
    {
        /* hanle FW debug purpose only */
        FwDebugPurposeHandler(event_rxd, msg, net_pkt);
    }
    else
    {
        FillRspPayloadLenAndDumpExpectLenAndRspLenInfo(event_rxd, msg);
    }
}

static VOID HandleLayer0GenericEvent(UINT8 EID, UINT8 ExtEID, UINT8 *Data)
{
    GenericEventHandler(EID, ExtEID, Data);
}

static BOOLEAN IsNormalLayer0Events(EVENT_RXD *event_rxd)
{
    if ((GET_EVENT_FW_RXD_EID(event_rxd) == MT_FW_START_RSP)            ||
        (GET_EVENT_FW_RXD_EID(event_rxd) == MT_RESTART_DL_RSP)          ||
        (GET_EVENT_FW_RXD_EID(event_rxd) == MT_TARGET_ADDRESS_LEN_RSP)  ||
        (GET_EVENT_FW_RXD_EID(event_rxd) == MT_PATCH_SEM_RSP)           ||
        (GET_EVENT_FW_RXD_EID(event_rxd) == EVENT_ACCESS_REG))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
static void HandleLayer0Events(EVENT_RXD *event_rxd,
            struct cmd_msg *msg, PNDIS_PACKET net_pkt)
{

	/* handle layer0 generic event */
	if (GET_EVENT_FW_RXD_EID(event_rxd) == GENERIC_EVENT)
	{
        HandleLayer0GenericEvent(GET_EVENT_FW_RXD_EID(event_rxd),
                			     GET_EVENT_FW_RXD_EXT_EID(event_rxd),
                                 GET_EVENT_HDR_ADDR(net_pkt) - 4);
	}
	else
	{
		/* handle normal layer0 event */
		if (IsNormalLayer0Events(event_rxd))
		{
			msg->attr.rsp.handler(msg, GET_EVENT_HDR_ADDR(net_pkt) - 4,
                    GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd) + 4);
		}
        else if (IsRspLenVariableAndMatchSpecificMinLen(event_rxd, msg))
        {
            /* hanle event len is variable */
            EventLenVariableHandler(event_rxd, msg, net_pkt);
        }
		else if (IS_IGNORE_RSP_PAYLOAD_LEN_CHECK(msg))
		{
			/* hanle FW debug purpose only */
            FwDebugPurposeHandler(event_rxd, msg, net_pkt);
		}
		else
		{
            FillRspPayloadLenAndDumpExpectLenAndRspLenInfo(event_rxd, msg);
        }

	}
}

static VOID GetMCUCtrlAckQueueSpinLock(struct MCU_CTRL **ctl,
                                            unsigned long *flags)
{
#ifdef RTMP_PCI_SUPPORT
	RTMP_SPIN_LOCK_IRQSAVE(&((*ctl)->ackq_lock), flags);
#endif
}

static VOID ReleaseMCUCtrlAckQueueSpinLock(struct MCU_CTRL **ctl,
                                            unsigned long *flags)
{
#ifdef RTMP_PCI_SUPPORT
    RTMP_SPIN_UNLOCK_IRQRESTORE(&((*ctl)->ackq_lock), flags);
#endif
}

static UINT8 GetEventFwRxdSequenceNumber(EVENT_RXD *event_rxd)
{
    return (UINT8)(GET_EVENT_FW_RXD_SEQ_NUM(event_rxd));
}
static VOID HandleSeqNonZeroNormalEvents(RTMP_ADAPTER *pAd,
                    EVENT_RXD *event_rxd, PNDIS_PACKET net_pkt)
{

    UINT8 peerSeq;
    struct cmd_msg *msg, *msg_tmp;
    struct MCU_CTRL *ctl = &pAd->MCUCtrl;

    unsigned long flags = 0;


    GetMCUCtrlAckQueueSpinLock(&ctl, &flags);

	DlListForEachSafe(msg, msg_tmp, &ctl->ackq, struct cmd_msg, list)
	{
        peerSeq = GetEventFwRxdSequenceNumber(event_rxd);

        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s: msg->seq=%x, field.seq_num=%x, msg->attr.ctrl.expect_size=%d\n",
                __FUNCTION__, msg->seq, peerSeq, msg->attr.ctrl.expect_size));
        if (msg->seq == peerSeq)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                    ("%s (seq=%d)\n", __FUNCTION__, msg->seq));

            ReleaseMCUCtrlAckQueueSpinLock(&ctl, &flags);

            msg->receive_time_in_jiffies = jiffies;

    		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s: CMD_ID(0x%x 0x%x),total spent %ld ms\n", __FUNCTION__,
                 msg->attr.type,msg->attr.ext_type, ((msg->receive_time_in_jiffies - msg->sending_time_in_jiffies) * 1000 / OS_HZ)));


			if (GET_EVENT_FW_RXD_EID(event_rxd) == EXT_EVENT)
			{
                HandleLayer1Events(event_rxd, msg, net_pkt);
			}
			else
			{
			    HandleLayer0Events(event_rxd, msg, net_pkt);
			}

    		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                    ("%s: need_wait=%d\n", __FUNCTION__,
                    IS_CMD_MSG_NEED_SYNC_WITH_FW_FLAG_SET(msg)));

		    CompleteWaitCmdMsgOrFreeCmdMsg(msg);

            GetMCUCtrlAckQueueSpinLock(&ctl, &flags);

			break;
		}
	}
    ReleaseMCUCtrlAckQueueSpinLock(&ctl, &flags);
}

static VOID AndesMTRxProcessEvent(RTMP_ADAPTER *pAd, struct cmd_msg *rx_msg)
{
	PNDIS_PACKET net_pkt = rx_msg->net_pkt;
	EVENT_RXD *event_rxd = (EVENT_RXD *)GET_OS_PKT_DATAPTR(net_pkt);

	//event_rxd->fw_rxd_0.word = le2cpu32(event_rxd->fw_rxd_0.word);
	event_rxd->fw_rxd_1.word = le2cpu32(event_rxd->fw_rxd_1.word);
	event_rxd->fw_rxd_2.word = le2cpu32(event_rxd->fw_rxd_2.word);

#ifdef CONFIG_TRACE_SUPPORT
    TRACE_MCU_EVENT_INFO(GET_EVENT_FW_RXD_LENGTH(event_rxd),
                        GET_EVENT_FW_RXD_PKT_TYPE_ID(event_rxd),
                        GET_EVENT_FW_RXD_EID(event_rxd),
                        GET_EVENT_FW_RXD_SEQ_NUM(event_rxd),
                        GET_EVENT_FW_RXD_EXT_EID(event_rxd),
                        GET_EVENT_HDR_ADDR(net_pkt),
                        GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd));
#endif

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
	    ("%s: seq_num=%d, ext_eid=%x\n", __FUNCTION__,
    			GET_EVENT_FW_RXD_SEQ_NUM(event_rxd),
                GET_EVENT_FW_RXD_EXT_EID(event_rxd)));

    if (IsUnsolicitedEvent(event_rxd))
	{
        HandlSeq0AndOtherUnsolicitedEvents(pAd, event_rxd, net_pkt);
    }
    else
	{
        HandleSeqNonZeroNormalEvents(pAd, event_rxd, net_pkt);
	}
}


VOID AndesMTRxEventHandler(RTMP_ADAPTER *pAd, UCHAR *data)
{
	struct cmd_msg *msg;
	struct MCU_CTRL *ctl = &pAd->MCUCtrl;
	EVENT_RXD *event_rxd = (EVENT_RXD *)data;

	if (!OS_TEST_BIT(MCU_INIT, &ctl->flags))
    {
		return;
	}

    msg = AndesAllocCmdMsg(pAd, GET_EVENT_FW_RXD_LENGTH(event_rxd));
	if (!msg)
    {
		return;
    }
    AndesAppendCmdMsg(msg, (char *)data, GET_EVENT_FW_RXD_LENGTH(event_rxd));

	AndesMTRxProcessEvent(pAd, msg);

#if defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT)
	if (msg->net_pkt)
    {
		RTMPFreeNdisPacket(pAd, msg->net_pkt);
    }
#endif

	AndesFreeCmdMsg(msg);
}


#if defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT)
VOID AndesMTPciFwInit(RTMP_ADAPTER *pAd)
{
	struct MCU_CTRL *Ctl = &pAd->MCUCtrl;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s\n", __FUNCTION__));
	Ctl->Stage = FW_NO_INIT;
#if defined(MT7615)
    PciResetPDMAToMakeSurePreFetchIndexCorrect(pAd);
    if (MTK_REV_GTE(pAd, MT7615, MT7615E3))
    {
        EnhancedPDMAInit(pAd);
    }
#endif
	/* Enable Interrupt*/
	RTMP_IRQ_ENABLE(pAd);
	RT28XXDMAEnable(pAd);
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);
}


VOID AndesMTPciFwExit(RTMP_ADAPTER *pAd)
{
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s\n", __FUNCTION__));
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_START_UP);
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);
	RT28XXDMADisable(pAd);
	RTMP_ASIC_INTERRUPT_DISABLE(pAd);
}
#endif /* defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT) */





#ifdef LED_CONTROL_SUPPORT
#ifdef MT7615
INT AndesLedEnhanceOP(
	RTMP_ADAPTER *pAd,
	UCHAR led_idx,
	UCHAR tx_over_blink,
	UCHAR reverse_polarity,
	UCHAR band,
	UCHAR blink_mode,
	UCHAR off_time,
	UCHAR on_time,
	UCHAR led_control_mode
	)
#else
INT AndesLedEnhanceOP(
	RTMP_ADAPTER *pAd,
	UCHAR led_idx,
	UCHAR tx_over_blink,
	UCHAR reverse_polarity,
	UCHAR blink_mode,
	UCHAR off_time,
	UCHAR on_time,
	UCHAR led_control_mode
	)
#endif
{
	struct cmd_msg *msg;
	CHAR *pos, *buf;
	UINT32 len;
	UINT32 arg0;
	INT32 ret;
	LED_ENHANCE led_enhance;
	struct _CMD_ATTRIBUTE attr = {0};
	
	len = sizeof(LED_ENHANCE) + sizeof(arg0);
	msg = AndesAllocCmdMsg(pAd, len);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_LED);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

	AndesInitCmdMsg(msg, attr);

	/* Led ID and Parameter */
	arg0 = led_idx;
	led_enhance.word = 0;
	led_enhance.field.on_time=on_time;
	led_enhance.field.off_time=off_time;
	led_enhance.field.tx_blink=blink_mode;
#ifdef MT7615
	led_enhance.field.band_select=band;
#endif
	led_enhance.field.reverse_polarity=reverse_polarity;
	led_enhance.field.tx_over_blink=tx_over_blink;
/*
	if (pAd->LedCntl.LedMethod == 1)
	{
		led_enhance.field.tx_blink=2;
		led_enhance.field.reverse_polarity=1;	
		if (led_control_mode == 1 || led_control_mode == 0)
			led_control_mode = ~(led_control_mode) & 0x1;			
	}
*/
	led_enhance.field.idx = led_control_mode;
	os_alloc_mem(pAd, (UCHAR **)&buf, len);
	if (buf == NULL)
	{
		return NDIS_STATUS_RESOURCES;
	}

	NdisZeroMemory(buf, len);
	
	pos = buf;
	/* Parameter */
	
	NdisMoveMemory(pos, &arg0, 4);
	NdisMoveMemory(pos+4, &led_enhance, sizeof(led_enhance));
	

	pos += 4;

	hex_dump("AndesLedOPEnhance: ", buf, len);
	AndesAppendCmdMsg(msg, (char *)buf, len);
	

	ret = AndesSendCmdMsg(pAd, msg);
	if (ret)
		printk("%s: Fail!\n", __FUNCTION__);
	else
		printk("%s: Success!\n", __FUNCTION__);
	os_free_mem(buf);

error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;	
}
#endif
