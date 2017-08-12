/****************************************************************************
* Mediatek Inc.
* 5F., No.5, Taiyuan 1st St., Zhubei City, 
* Hsinchu County 302, Taiwan, R.O.C.
* (c) Copyright 2014, Mediatek, Inc.
*
* All rights reserved. Mediatek's source code is an unpublished work and the
* use of a copyright notice does not imply otherwise. This source code
* contains confidential trade secret material of Mediatek. Any attemp
* or participation in deciphering, decoding, reverse engineering or in any
* way altering the source code is stricitly prohibited, unless the prior
* written consent of Mediatek, Inc. is obtained.
****************************************************************************

	Module Name:
	ftm.c

	Abstract:
	802.11mc FTM protocol responder side (AP) implementation.

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
	annie        2014.11.22   Initial version: FTM responder function

*/

#ifdef FTM_SUPPORT
#include "rt_config.h"


/*
========================================================================
Routine Description:
	Check a packet is Action frame or not

Arguments:
	pAd 			- WLAN control block pointer
	pbuf			- packet buffer

Return Value:
	TRUE		- yes
	FALSE		- no

========================================================================
*/
BOOLEAN 
IsPublicActionFrame(
	IN PRTMP_ADAPTER	pAd,
	IN VOID				*pbuf
	)
{
	HEADER_802_11 *pHeader = pbuf;
	UINT8 *ptr = pbuf;

	if (pHeader->FC.Type != FC_TYPE_MGMT)
		return FALSE;

	if (pHeader->FC.SubType != SUBTYPE_ACTION)
		return FALSE;

	ptr += sizeof(HEADER_802_11);
	if (*ptr == CATEGORY_PUBLIC)
		return TRUE;
	else
		return FALSE;
}


/*
========================================================================
Routine Description:
	Dubug purpose, dump hex buffer content

Arguments:
	str				- message name
	pSrcBufVA		- pointer to target buffer
	SrcBufLen		- length of target buffer (in byte)
	DbgLvl			- message level

Return Value:
	None

========================================================================
*/
VOID 
FtmHexDump(
	IN char *str, 
	IN UINT8 *pSrcBufVA, 
	IN UINT SrcBufLen, 
	IN UINT32 DbgLvl
	)
{
#ifdef DBG
	if (DebugLevel < DbgLvl)
		return;

	hex_dump(str, pSrcBufVA, SrcBufLen);
#endif
}


/*
========================================================================
Routine Description:
	Dubug purpose, dump FTM parameter

Arguments:
	pAd				- WLAN control block pointer
	parm			- Target FTM parameter

Return Value:
	None

========================================================================
*/
BOOLEAN 
FtmParmDump(
	IN PRTMP_ADAPTER    pAd,
	IN PFTM_PARAMETER 	parm,
	IN UINT32			DbgLvl
	)
{
	if (!parm)
		return FALSE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("  FTM Parameter Dump\n"));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    status: %d\n", parm->status));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    value: %d\n", parm->value));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    num_burst_exponent=%d\n", parm->num_burst_exponent));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    burst_duration=%d\n", parm->burst_duration));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    min_delta_ftm=%d\n", parm->min_delta_ftm));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    partial_tsf_timer=0x%04X\n", parm->partial_tsf_timer));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    asap_capable=%d\n", parm->asap_capable));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    asap=%d\n", parm->asap));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    ftms_per_burst=%d\n", parm->ftms_per_burst));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    ftm_format_and_bw=%d\n", parm->ftm_format_and_bw));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("	    burst_period=%d\n", parm->burst_period));

	return TRUE;
}


/*
========================================================================
Routine Description:
	Dubug purpose, dump FTM entry value

Arguments:
	pAd				- WLAN control block pointer
	pEntry			- FTM Peer Entry pointer

Return Value:
	if the pEntry is valid and alive (> FTMPEER_UNUSED), return TRUE.

========================================================================
*/
BOOLEAN 
FtmEntryDump(
	IN PRTMP_ADAPTER    pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN UINT32			DbgLvl
	)
{
	if (!pAd || !pEntry)
		return FALSE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("[FTM Entry Dump] idx=%d 0x%p\n", GET_FTM_PEER_IDX(pAd, pEntry), pEntry));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- FTM State: %d\n", pEntry->State));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Address: %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(pEntry->Addr)));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- PeerReqParm\n"));
	FtmParmDump(pAd, &pEntry->PeerReqParm, DbgLvl);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- VerdictParm\n"));
	FtmParmDump(pAd, &pEntry->VerdictParm, DbgLvl);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- FTM DialogToken: %d\n", pEntry->DialogToken));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- BurstCntDown: %d\n", pEntry->BurstCntDown));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- FtmCntDown: %d\n", pEntry->FtmCntDown));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- PendingPid: 0x%02X\n", pEntry->PendingPid));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- TransAndRetrans: %u\n", pEntry->TransAndRetrans));
	FtmPidPendingQDump(pAd, DbgLvl);
#ifdef FTM_INITIATOR
	FtmRxTmrQDump(pAd, pEntry, DbgLvl);
#endif /* FTM_INITIATOR */

	FtmHexDump("Last TmrHwReport", (UINT8 *)&pEntry->Tmr.HwReport, sizeof(TMR_FRM_STRUC), DbgLvl);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("TMR calibration result is 0x%X\n", pAd->pTmrCtrlStruct->TmrCalResult));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("\n[Location IE]\n"));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Civic Report: %d\n", pEntry->bCivicMsmtReport));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- LCI Report: %d\n", pEntry->bLciMsmtReport));

	if (pEntry->State > FTMPEER_UNUSED)
		return TRUE;
	else
		return FALSE;
}


/*
========================================================================
Routine Description:
	Dubug purpose, dump PidPendingQ link list content.

Arguments:
	pAd				- WLAN control block pointer
	DbgLvl			- Level of debug message

Return Value:
	None

========================================================================
*/
VOID 
FtmPidPendingQDump(
	IN PRTMP_ADAPTER    pAd,
	IN UINT32			DbgLvl
	)
{
	PFTM_CTRL pFtm = pAd->pFtmCtrl;
	PFTM_PEER_ENTRY pNode, pNext;
	ULONG IrqFlags = 0;
	int num = 0;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("FTM PidPendingQ Dump\n"));

	RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pFtm->PidPendingQ, FTM_PEER_ENTRY, PidList)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, 
			("    EntryIdx:%d PID:0x%02X bGotTmr=%d (Next:0x%p Prev:0x%p)\n", 
			GET_FTM_PEER_IDX(pAd, pNode), pNode->PendingPid, pNode->bGotTmr,
			pNode->PidList.Next, pNode->PidList.Prev));

		num ++;
	}
	RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("    Total %d nodes\n", num));
}


/*
========================================================================
Routine Description:
	Dump FTM LCI current setting

Arguments:
	pAd				- WLAN control block pointer
	pLci			- Target LCI
	DbgLvl			- debug level

Return Value:
	None

========================================================================
*/
VOID 
FtmLciValueDump(
	IN PRTMP_ADAPTER    pAd,
	IN PLCI_FIELD 		pLci,
	IN UINT32			DbgLvl
	)
{
	int i;
	UINT64	u8b;

	if (!pLci)
		return;

	/* Latitude */
	u8b = ((UINT64)(pLci->field.Latitude_b2_b33) << 2) | (pLci->field.Latitude_b0_b1);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("[Latitude]\t0x%09llX\n", u8b));

	/* Longitude */
	u8b = ((UINT64)(pLci->field.Longitude_b2_b33) << 2) | (pLci->field.Longitude_b0_b1);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("[Longitude]\t0x%09llX\n", u8b));

	/* Altitude */
	u8b = (pLci->field.Altitude_b22_b29 << 22) | (pLci->field.Altitude_b0_b21);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("[Altitude]\t 0x%08llX\n\n", u8b));


	/* detail */
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("LCI header length: %d\n", pAd->pFtmCtrl->LciHdr.Length));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("LCI Value:\n"));

	for (i = 0; i < sizeof(LCI_FIELD); i ++) {
		if ((i > 0) && (i%4 == 0))
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, (" "));

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("%02X ", pLci->byte[i]));
	}
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("\n\n"));

	/* structure dump */
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- LatitudeUncertainty: 0x%X\n", pLci->field.LatitudeUncertainty));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Latitude_b0_b1: 0x%X\n", pLci->field.Latitude_b0_b1));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Latitude_b2_b33: 0x%X\n", pLci->field.Latitude_b2_b33));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- LongitudeUncertainty: 0x%X\n", pLci->field.LongitudeUncertainty));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Longitude_b0_b1: 0x%X\n", pLci->field.Longitude_b0_b1));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Longitude_b2_b33: 0x%X\n", pLci->field.Longitude_b2_b33));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- AltitudeType: 0x%X\n", pLci->field.AltitudeType));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- AltitudeUncertainty: 0x%X\n", pLci->field.AltitudeUncertainty));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Altitude_b0_b21: 0x%X\n", pLci->field.Altitude_b0_b21));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Altitude_b22_b29: 0x%X\n", pLci->field.Altitude_b22_b29));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Datum: 0x%X\n", pLci->field.Datum));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- RegLocAgreement: 0x%X\n", pLci->field.RegLocAgreement));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- RegLocDSE: 0x%X\n", pLci->field.RegLocDSE));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- Dependent: 0x%X\n", pLci->field.Dependent));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("- STAVersion: 0x%X\n", pLci->field.STAVersion));

	return;
}


/*
========================================================================
Routine Description:
	Convert min_delat_ftm parameter (unit: 100us) to Timer period (unit: 1ms)

Arguments:
	MinDelta, unit: 100us

Return Value:
	millisecond
	
========================================================================
*/
UINT32
FtmConvertMinDelta(
	IN UINT32	MinDelta
	)
{
	UINT32 ms;

	ms = MinDelta /10;

	/* Take Ceiling */
	if (MinDelta %10)
		ms ++;

	/* workaround with Marvell: prevent to send too early, 2015.04.17 */
	ms += 5;

	return ms;
}


/*
========================================================================
Routine Description:
	Convert BurstTimeout parameter to Timer period (unit: 1ms)

Arguments:
	BurstTimeout

Return Value:
	millisecond
	
========================================================================
*/
UINT32
FtmConvertBurstTimeout(
	IN UINT32	BurstTimeout
	)
{
	UINT32 ms;

	switch (BurstTimeout)
	{
		case FTMBST_250US:
		case FTMBST_500US:
		case FTMBST_1MS:
			ms = 1;
			break;

		case FTMBST_2MS:
			ms = 2;
			break;

		case FTMBST_4MS:
			ms = 4;
			break;

		case FTMBST_8MS:
			ms = 8;
			break;

		case FTMBST_16MS:
			ms = 16;
			break;

		case FTMBST_32MS:
			ms = 32;
			break;

		case FTMBST_64MS:
			ms = 64;
			break;
			
		case FTMBST_128MS:
			ms = 128;
			break;

		default:
			ms = 200;
			break;
	}

	return ms;
}


/*
========================================================================
Routine Description:
	Get BurstTimeout code by delta time (unit: 100 us)

Arguments:
	time						- unit: 100 us

Return Value:
	FTM_BURST_DURATION_ENCODING

========================================================================
*/
UINT32
FtmEncodeBurstTimeout(
	IN UINT32	time
	)
{
	UINT32 burst_timeout;	/* FTM_BURST_DURATION_ENCODING */

	if (time > 640)
		burst_timeout = FTMBST_128MS;
	else if (time > 320)
		burst_timeout = FTMBST_64MS;
	else if (time > 160)
		burst_timeout = FTMBST_32MS;
	else if (time > 80)
		burst_timeout = FTMBST_16MS;
	else if (time > 40)
		burst_timeout = FTMBST_8MS;
	else if (time > 20)
		burst_timeout = FTMBST_4MS;
	else if (time > 10)
		burst_timeout = FTMBST_2MS;
	else if (time > 5)
		burst_timeout = FTMBST_1MS;
	else if (time > 2)
		burst_timeout = FTMBST_500US;
	else
		burst_timeout = FTMBST_250US;

	return burst_timeout;
}


/*
========================================================================
Routine Description:
	Get min BurstTimeout code by min_delta_ftm and ftms_per_burst.

Arguments:
	min_delta_ftm			- unit: 100 us
	ftms_per_burst		- unit: number
	OUT *pburst_period	- unit: 100 ms

Return Value:
	FTM_BURST_DURATION_ENCODING

========================================================================
*/
UINT32
FtmGetBurstTimeout(
	IN UINT8	min_delta_ftm,
	IN UINT8	ftms_per_burst,
	OUT UINT16 *pburst_period
	)
{
	const UINT32 TIMEOUT_FACTOR = 1;
	UINT32 burst_timeout;	/* FTM_BURST_DURATION_ENCODING */
	UINT32 burst_period;
	UINT32 time = ftms_per_burst * min_delta_ftm;	/* unit: 100 us */

	/* Get pburst_period */
	burst_period = time / 1000;	/* unit: 100 ms */
 
	if (time % 1000)
		burst_period ++;

	*pburst_period = burst_period;

	/* Get burst_timeout */
	burst_timeout = FtmEncodeBurstTimeout(burst_period *1000 *TIMEOUT_FACTOR);

	return burst_timeout;
}


/*
========================================================================
Routine Description:
	Lookup table of TOA/TOD base offset

Arguments:
	pAd 			- WLAN control block pointer

Return Value:
	the offset, in 01. ns

Note:
	Channel Delta		MT6630		MT7628
	20 - 20			452 ns		428 ns
	40 - 20			1020 ns		980 ns
	40 - 40			651 ns		599 ns

========================================================================
*/
UINT32
FtmGetTmrBaseOffset(
	IN PRTMP_ADAPTER    pAd
	)
{
	UINT32 offset;	/* unit: 0.1 ns */

	switch (pAd->CommonCfg.BBPCurrentBW)
	{
		case BW_40:
			offset = 5990;
			break;

		case BW_20:
			offset = 4280;
			break;

		default:
			offset = 0;
			break;
	}

	return offset;
}


/*
========================================================================
Routine Description:
	Convert TOD/TOA unit from 0.25 ns to 0.1 ns and minus the base offset in TOA.

Arguments:
	pAd 			- WLAN control block pointer
	pTOD		- pinter to TOD
	pTOD		- pinter to TOA

Return Value:
	Success or not

========================================================================
*/
BOOLEAN
FtmConvertTodToa(
	IN PRTMP_ADAPTER	pAd,
	IN UINT64		*pTOD,
	IN UINT64		*pTOA
	)
{
	UINT32 base = FtmGetTmrBaseOffset(pAd);

	*pTOD = *pTOD *5 /2;
	*pTOA = *pTOA *5 /2;
	
	if (*pTOA < base)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("##### TOA is less than base\n"));
		return FALSE;
	}
	else
	{
		*pTOA -= FtmGetTmrBaseOffset(pAd);
		return TRUE;
	}
}



/*
========================================================================
Routine Description:
	Is x greater than y ?
	if the range is limited and unsigned, use half interval as valid compare.

Arguments:
	x, y 			- the number for comparison
	min, max		- the range	

Return Value:
	TRUE		- x >= y
	FALSE		- x < y

========================================================================
*/
inline BOOLEAN
FtmGTE(
    IN UINT8 x,
    IN UINT8 y,
    IN UINT8 min,
    IN UINT8 max
	)
{
	UINT32 diff;
	BOOLEAN xIsLarger;

	if (x >= y)
	{
		diff = x - y;
		xIsLarger = TRUE;
	}
	else
	{
		diff = y - x;
		xIsLarger = FALSE;
	}

	/* if diff > half interval */
	if (diff > ((max - min + 1) >> 1))
		xIsLarger = !xIsLarger;

	return xIsLarger;
}


/*
========================================================================
Routine Description:
	Get Tx PID for Tx packet and TMR report mapping. PID Range: 0x21~0x40.
	Idea: PID is generated from FTM DialogToken.

Arguments:
	pAd 			- WLAN control block pointer

Return Value:
	PID

========================================================================
*/
inline UINT8
FtmGetNewPid(
    IN PRTMP_ADAPTER    pAd
	)
{
	return (pAd->pFtmCtrl->DialogToken & MASK_PID_FTM) + NUMBER_PID_FTM;
}


/*
========================================================================
Routine Description:
	Add new node to FTM PidPendingQ.
	Link list member: "PidList" in FTM_PEER_ENTRY
	The target is pEntry->PendingPid.

Arguments:
	pAd 				- WLAN control block pointer
	pEntry			- FTM Peer Entry pointer
	PID				- Tx Pending PID (is waiting for TMR)

Return Value:
	None
	
========================================================================
*/
VOID
FtmAddPidPendingNode(
    IN PRTMP_ADAPTER    pAd,
    IN PFTM_PEER_ENTRY	pEntry,
    IN UINT8 			PID
	)
{
	PFTM_CTRL pFtm = pAd->pFtmCtrl;
	PFTM_PEER_ENTRY pNode, pNext;
	BOOLEAN bHit = FALSE;
	ULONG IrqFlags = 0;

	/* Check existed nodes */
	RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pFtm->PidPendingQ, FTM_PEER_ENTRY, PidList)
	{
		if (pNode == pEntry)
		{
			if (!bHit)
				bHit = TRUE;
			else
			{	/* more than one node with the same entry */
				MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
					("%s()#%d: WARNING: Entry %d is duplicated => remove old node with PID 0x%02X\n",
					__FUNCTION__, __LINE__, GET_FTM_PEER_IDX(pAd, pNode), pNode->PendingPid));

				DlListDel(&pNode->PidList);
				ASSERT(FALSE);
			}
		}
		else if (FtmGTE(pNode->PendingPid, PID, PID_FTM_MIN, PID_FTM_MAX))
		{
			/* BTW, clear all obsolete nodes */
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
				("%s()#%d: remove obsolete node, EntryIdx:%d PID:0x%02X\n", 
				__FUNCTION__, __LINE__, GET_FTM_PEER_IDX(pAd, pNode), pNode->PendingPid));

			DlListDel(&pNode->PidList); 
		}
	}
	RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);

	if (bHit)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
			("%s()#%d: Entry %d overwrite PID: 0x%02X -> 0x%02X\n",
			__FUNCTION__, __LINE__, GET_FTM_PEER_IDX(pAd, pEntry), pEntry->PendingPid, PID));

		/* Update new PID */
		RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
		pEntry->PendingPid = PID;
		RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);
	}
	else
	{
		/* Insert new node: pEntry */
		RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
		pEntry->PendingPid = PID;
		DlListAddTail(&pFtm->PidPendingQ, &pEntry->PidList);
		RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);
	}

	/* Reset TMR report content */
	NdisZeroMemory(&pEntry->Tmr, sizeof(TMR_NODE));
	pEntry->bGotTmr = FALSE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("Tx: after add node\n"));
	FtmPidPendingQDump(pAd, DBG_LVL_INFO);
}


/*
========================================================================
Routine Description:

Arguments:
	pAd 				- WLAN control block pointer
	pEntry			- FTM Peer Entry pointer

Return Value:
	None
	
========================================================================
*/
INT
FtmDeqPidPendingNode(
    IN PRTMP_ADAPTER    pAd,
    IN PFTM_PEER_ENTRY	pEntry
	)
{
	ULONG IrqFlags = 0;
	INT ret = NDIS_STATUS_FAILURE;	

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("%s:()\n", __FUNCTION__));

	if (pEntry->PidList.Next)
	{
		RTMP_IRQ_LOCK(&pAd->pFtmCtrl->PidPendingQLock, IrqFlags);
		DlListDel(&pEntry->PidList);
		RTMP_IRQ_UNLOCK(&pAd->pFtmCtrl->PidPendingQLock, IrqFlags);

		pEntry->PendingPid = FTMPID_NOT_WAITING;
		
		ret = NDIS_STATUS_SUCCESS;	
	}
	else
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			("%s()#%d: idx=%d, ERROR! no PendingPid !\n", 
			__FUNCTION__, __LINE__, GET_FTM_PEER_IDX(pAd, pEntry)));
		ret = NDIS_STATUS_FAILURE;	
	}

	return ret;
}


/*
========================================================================
Routine Description:
	Search for the node with specific PID from FTM PidPendingQ.

Arguments:
	pAd 			- WLAN control block pointer
	PID

Return Value:
	Target FTM entry
	
========================================================================
*/
PFTM_PEER_ENTRY 
FtmGetPidPendingNode(
    IN PRTMP_ADAPTER    pAd,
    IN UINT8    PID
    )
{
	PFTM_CTRL pFtm = pAd->pFtmCtrl;
	PFTM_PEER_ENTRY pEntry = NULL;
	PFTM_PEER_ENTRY pNode, pNext;
	ULONG IrqFlags = 0;

	RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pFtm->PidPendingQ, FTM_PEER_ENTRY, PidList)
	{
		if (pNode->PendingPid == PID)
		{
			pEntry = pNode;
			break;
		}
	}
	RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);
	
	return pEntry;
}


/*
========================================================================
Routine Description:
	Check the content of duplicated TMR report.

Arguments:
	pAd 				- WLAN control block pointer
	pEntry			- FTM Peer Entry pointer
	pCurTmr			- pointer to Current TMR report
	pCurTOA 			- pointer to Current TOA
	pCurTOD 			- pointer to Current TOD

Return Value:
	None
	
========================================================================
*/
VOID
FtmCheckDuplicatedTmr(
	IN PRTMP_ADAPTER	pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN TMR_FRM_STRUC	*pCurTmr,
	IN UINT64		*pCurTOA,
	IN UINT64		*pCurTOD
	)
{
	BOOLEAN bToaNotChanged = (*pCurTOA == pEntry->Tmr.toa);
	BOOLEAN bTodNotChanged = (*pCurTOD == pEntry->Tmr.tod);

	if (bToaNotChanged || bTodNotChanged || (pCurTmr->TmrD0.ToaVld==0) || (pCurTmr->TmrD0.TodVld==0))
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, 
			("   [TMR]  duplicated PID:0x%02X, TOA[V:%d,Changed:%d] TOD[V:%d,Changed:%d]\n",
			pEntry->PendingPid,
			pCurTmr->TmrD0.ToaVld, !bToaNotChanged, 
			pCurTmr->TmrD0.TodVld, !bTodNotChanged));

		FtmHexDump("TMR - Original", (UINT8 *)&pEntry->Tmr.HwReport, sizeof(TMR_FRM_STRUC), DBG_LVL_INFO);
		FtmHexDump("TMR - Current", (UINT8 *)pCurTmr, sizeof(TMR_FRM_STRUC), DBG_LVL_INFO);
	}
}


/*
========================================================================
Routine Description:
	Initialize FTM struct and initiator entry table

Arguments:
	pAd 			- WLAN control block pointer

Return Value:
	None
	
========================================================================
*/
VOID 
FtmMgmtInit(
    IN PRTMP_ADAPTER    pAd
    )
{
	PFTM_CTRL pFtm = NULL;
	ULONG IrqFlags = 0;
	UINT8 i;
	UINT8 TestVector_LCI[] = {	/* Test Plan 0.0.6 */
		0x52, 0x83, 0x4d, 0x12,  0xef, 0xd2, 0xb0, 0x8b,
		0x9b, 0x4b, 0xf1, 0xcc,  0x2c, 0x00, 0x00, 0x41};
#ifdef CONFIG_AP_SUPPORT
	UCHAR APIndex;
#endif /*CONFIG_AP_SUPPORT*/

	if (os_alloc_mem(NULL, (UINT8 **)&pFtm, sizeof(FTM_CTRL)) == NDIS_STATUS_FAILURE)
		return;

	pAd->pFtmCtrl = pFtm;
	NdisZeroMemory(pFtm, sizeof(FTM_CTRL));

	/* FTM Default Setting */
	pFtm->asap = FTM_DEFAULT_ASAP;
	pFtm->min_delta_ftm = FTM_DEFAULT_MIN_DELTA_FTM;
	pFtm->ftms_per_burst = FTM_DEFAULT_FTMS_PER_BURST;
	pFtm->num_burst_exponent = FTM_DEFAULT_NUM_BURST_EXP;
	pFtm->burst_duration = FTM_DEFAULT_BURST_DURATION;
	pFtm->burst_period = FTM_DEFAULT_BURST_PERIOD;
	pFtm->LciHdr.SubElement = LCI_RPTID_LCI;
	pFtm->LciHdr.Length = sizeof(LCI_FIELD);
	pFtm->LciZ.SubElement = LCI_RPTID_Z;
	pFtm->LciZ.Length = sizeof(Z_ELEMENT) - 2;	
	pFtm->CivicHdr.SubElement = CIVIC_RPTID_CIVIC;
	pFtm->CivicHdr.Length = 0;

	/* Set Test Vector for test plan 0.0.6 */
	NdisCopyMemory(pFtm->LciField.byte, TestVector_LCI, sizeof(LCI_FIELD));
	pFtm->Civic.CountryCode[0] = 'U';	/* US, Ref: http://zh.wikipedia.org/wiki/ISO_3166-1 */
	pFtm->Civic.CountryCode[1] = 'S';
	pFtm->Civic.CA_Type = 2;
	pFtm->Civic.CA_Length = MAX_CIVIC_CA_VALUE_LENGTH;
	for (i = 0; i < MAX_CIVIC_CA_VALUE_LENGTH; i ++)
		pFtm->CA_Value[i] = i + 1;

	pFtm->CivicHdr.Length = sizeof(LOCATION_CIVIC) + pFtm->Civic.CA_Length;

	/* Clear Variables */
	pFtm->DialogToken = 1;
	pFtm->CalPeerIdx = INVALID_FTM_ENTRY_IDX;
	pFtm->LatestJoinPeer = 0;
	pFtm->bNoDurTimeout = TRUE;

	NdisAllocateSpinLock(pAd, &pFtm->PidPendingQLock);
	RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
	DlListInit(&pFtm->PidPendingQ);
	RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);

	FtmPeerTableInit(pAd);

#ifdef CONFIG_AP_SUPPORT
	/* Enable All Bssid Gas Rsp Capability */
	for (APIndex = 0; APIndex < MAX_MBSSID_NUM(pAd); APIndex++)
	{
		pAd->ApCfg.MBSSID[APIndex].GASCtrl.b11U_enable = TRUE;
	}	
#endif /* CONFIG_AP_SUPPORT */

	RTMPInitTimer(pAd, &pFtm->FtmReqTimer, GET_TIMER_FUNCTION(FtmReqTxTimerCallback), pAd, FALSE);
	pFtm->bCal = FALSE;
	pFtm->bSetLciRpt = TRUE;
	pFtm->bSetZRpt = TRUE;
	pFtm->bSetCivicRpt = TRUE;
	pFtm->bSetLciReq = FALSE;
	pFtm->bSetCivicReq = FALSE;
}


/*
========================================================================
Routine Description:
	De-init FTM struct and initiator entry table at exiting

Arguments:
	pAd 			- WLAN control block pointer

Return Value:
	None
	
========================================================================
*/
VOID 
FtmMgmtExit(
    IN PRTMP_ADAPTER    pAd
    )
{
	PFTM_CTRL pFtm = pAd->pFtmCtrl;
	PFTM_PEER_ENTRY	pEntry;
	BOOLEAN Cancelled;
	ULONG IrqFlags = 0;
	int idx;
#ifdef FTM_INITIATOR
	PFTM_MAPPING pNode, pNext;
#endif /* FTM_INITIATOR */

	for (idx = 0; idx < MAX_FTM_TBL_SIZE; idx ++)
	{
		pEntry = pAd->pFtmCtrl->FtmPeer + idx;
		pEntry->State = FTMPEER_UNUSED;
		RTMPCancelTimer(&pEntry->FtmTimer, &Cancelled);

#ifdef FTM_INITIATOR
		RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
		DlListForEachSafe(pNode, pNext, &pEntry->RxTmrQ, FTM_MAPPING, list)
		{ 
			DlListDel(&pNode->list); 
			os_free_mem(pNode); 
		}
		RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);
		NdisFreeSpinLock(&pEntry->RxTmrQLock);
#endif /* FTM_INITIATOR */
	}

	RTMP_IRQ_LOCK(&pFtm->PidPendingQLock, IrqFlags);
	DlListInit(&pFtm->PidPendingQ);
	RTMP_IRQ_UNLOCK(&pFtm->PidPendingQLock, IrqFlags);
	NdisFreeSpinLock(&pFtm->PidPendingQLock);

	RTMPCancelTimer(&pFtm->FtmReqTimer, &Cancelled);

	os_free_mem(pFtm);
	pAd->pFtmCtrl = NULL;
}


/*
========================================================================
Routine Description:
	Initialize FTM initiator entry table

Arguments:
	pAd 			- WLAN control block pointer

Return Value:
	None
	
========================================================================
*/
VOID 
FtmPeerTableInit(
    IN PRTMP_ADAPTER    pAd
    )
{
	int idx;
	PFTM_PEER_ENTRY	pEntry;
#ifdef FTM_INITIATOR
	ULONG IrqFlags = 0;
#endif /* FTM_INITIATOR */

	for (idx = 0; idx < MAX_FTM_TBL_SIZE; idx ++)
	{
		pEntry = pAd->pFtmCtrl->FtmPeer + idx;

		pEntry->State = FTMPEER_UNUSED;
		pEntry->pAd = (PVOID)pAd;
		RTMPInitTimer(pAd, &pEntry->FtmTimer, GET_TIMER_FUNCTION(FtmTxTimerCallback), pEntry, FALSE);

#ifdef FTM_INITIATOR
		NdisAllocateSpinLock(pAd, &pEntry->RxTmrQLock);
		RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
		DlListInit(&pEntry->RxTmrQ);
		RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);
#endif /* FTM_INITIATOR */
	}
}


/*
========================================================================
Routine Description:
	Initialize a FTM initiator entry and assign MAC Address

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM initiator entry pointer
	Addr			- MAC adress of target initiator

Return Value:
	None

Note:
	Here we use NdisZeroMemory since offset of State to clear all old value besides FtmTimer and pAd,
	so DO NOT change the position of member FtmTimer and pAd in FTM_PEER_ENTRY struct.

========================================================================
*/
VOID
FtmEntrySetValid(
    IN PRTMP_ADAPTER    pAd,
    IN PFTM_PEER_ENTRY	pEntry,
    IN UINT8	*Addr
	)
{
	/* Clear all members by setting to default value */
	pEntry->bLciMsmtReq = FALSE;
	pEntry->bCivicMsmtReq = FALSE;
	pEntry->bLciMsmtReport = FALSE;
	pEntry->bCivicMsmtReport = FALSE;
	pEntry->bFtmParm = FALSE;
	NdisZeroMemory(&pEntry->PeerReqParm, sizeof(FTM_PARAMETER));
	NdisZeroMemory(&pEntry->VerdictParm, sizeof(FTM_PARAMETER));
	pEntry->DialogToken = 0;
	pEntry->FollowUpToken = 0;
	pEntry->LciToken = 0;
	pEntry->CivicToken = 0;
	pEntry->BurstCntDown = 0;
	pEntry->FtmCntDown = 0;
	pEntry->bNeedTmr = FALSE;
	NdisZeroMemory(&pEntry->PidList, sizeof(DL_LIST));
	pEntry->PendingPid = FTMPID_NOT_WAITING;
	pEntry->TransAndRetrans = 0;
	pEntry->bGotTmr = FALSE;

	/* Set Address and state */
	NdisCopyMemory(pEntry->Addr, Addr, MAC_ADDR_LEN);
	pEntry->State = FTMPEER_IDLE;
}


/*
========================================================================
Routine Description:
	Search a FTM entry by Addr.

Arguments:
	pAd 			- WLAN control block pointer
	Addr			- MAC adress of target initiator

Return Value:
	FTM entry pointer; NULL means search failed.
	
========================================================================
*/
PFTM_PEER_ENTRY
FtmEntrySearch(
    IN PRTMP_ADAPTER    pAd,
    IN UINT8	*Addr
	)
{
	FTM_PEER_ENTRY *pEntry;
	int HashIdx = FTM_TBL_HASH_INDEX(Addr);
	int idx = HashIdx;

	do {
		pEntry = pAd->pFtmCtrl->FtmPeer + idx;
		if (pEntry->State && NdisEqualMemory(pEntry->Addr, Addr, MAC_ADDR_LEN))
			return pEntry;

		INCREASE_IDX(idx, 0, MAX_FTM_TBL_SIZE);
	} while (idx != HashIdx);

	return NULL;
}


/*
========================================================================
Routine Description:
	If FTM entry search hits, return the entry.
	Otherwise, get a vacancy entry to assign it, and return the new FTM entry.

Arguments:
	pAd 			- WLAN control block pointer
	Addr			- MAC adress of target initiator

Return Value:
	FTM entry pointer; NULL means table full and assign failed.

========================================================================
*/
PFTM_PEER_ENTRY
FtmEntryGet(
    IN PRTMP_ADAPTER    pAd,
    IN UINT8	*Addr
	)
{
	FTM_PEER_ENTRY *pEntry;
	int HashIdx, idx;

	pEntry = FtmEntrySearch(pAd, Addr);

	if (pEntry != NULL)
		return pEntry;

	/* Find an empty entry and occupy it */
	HashIdx = FTM_TBL_HASH_INDEX(Addr);
	idx = HashIdx;

	do 
	{
		pEntry = pAd->pFtmCtrl->FtmPeer + idx;
		if (pEntry->State == FTMPEER_UNUSED)
		{
			FtmEntrySetValid(pAd, pEntry, Addr);
			pAd->pFtmCtrl->LatestJoinPeer = idx;
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				("FTM %d_[%02x:%02x:%02x:%02x:%02x:%02x]\n", 
				idx, PRINT_MAC(pEntry->Addr)));

			return pEntry;
		}

		INCREASE_IDX(idx, 0, MAX_FTM_TBL_SIZE);
	} while (idx != HashIdx);

	return NULL;
}


/*
========================================================================
Routine Description:
	If there is a peer sent FTM request to AP, AP should continue handling it 
	after calibration completed.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

========================================================================
*/
VOID 
FtmTmrCalPeerConti(
	IN  PRTMP_ADAPTER   pAd
	)
{
	UINT8 PeerIdx = pAd->pFtmCtrl->CalPeerIdx;

	if (VALIDATE_FTM_ENTRY_IDX(PeerIdx))
	{
		pAd->pFtmCtrl->CalPeerIdx = INVALID_FTM_ENTRY_IDX;	/* Flag Clear */
		FtmEntryNegoDoneAction(pAd, pAd->pFtmCtrl->FtmPeer + PeerIdx);
	}
}


/*
========================================================================
Routine Description:
	To set TMR hardware CR.

Arguments:
	pAd				- WLAN control block pointer
	TmrType			- TMR Role: TMR_DISABLE/TMR_INITIATOR/TMR_RESPONDER

Return Value:
	None

========================================================================
*/
VOID 
FtmTmrCRSet(
	IN  PRTMP_ADAPTER   pAd,
	IN  UINT8 TmrType
	)
{
#ifdef TMR_SUPPORT
	UINT32  value = 0;

	// TODO: Remove it, workaround for HW callback control
	pAd->pTmrCtrlStruct->TmrEnable = TmrType;

	MAC_IO_READ32(pAd, ARB_SCR, &value);
	value |= MT_ARB_SCR_TXDIS;
	MAC_IO_WRITE32(pAd, ARB_SCR, value);

	MtSetTmrCR(pAd, TmrType);

	if (pAd->pFtmCtrl->bCal)
		MtSetTmrCal(pAd, TmrType, HcGetRadioChannel(pAd), pAd->CommonCfg.BBPCurrentBW);
	else
		FtmTmrCalPeerConti(pAd);

	value &= ~MT_ARB_SCR_TXDIS;
	MAC_IO_WRITE32(pAd, ARB_SCR, value);

#else
	FtmTmrCalPeerConti(pAd);
#endif /* TMR_SUPPORT */
}


/*
========================================================================
Routine Description:
	If any FTM initiators comes in, enable TMR hardware CR.

Arguments:
	pAd				- WLAN control block pointer
	pTarget			- FTM Peer Entry pointer

Return Value:
	FtmTmrCRSet() is called in this function or not.

Note:
	1. only run in MT7628

========================================================================
*/
BOOLEAN
FtmTmrEnableCheck(
    IN PRTMP_ADAPTER    pAd,
    IN PFTM_PEER_ENTRY	pTarget
	)
{
	UINT8 target = GET_FTM_PEER_IDX(pAd, pTarget);
	UINT8 idx;

	/* is anyone alive ? */
	for (idx = 0; idx < MAX_FTM_TBL_SIZE; idx ++)
	{
		if (idx == target)
			continue;

		if (pAd->pFtmCtrl->FtmPeer[idx].State > FTMPEER_UNUSED)
			return FALSE;
	}

	/* The first FTM initiator peer comes in, enable TMR CR */
	pAd->pFtmCtrl->CalPeerIdx = target;
	FtmTmrCRSet(pAd, TMR_INITIATOR);
	return TRUE;
}


/*
========================================================================
Routine Description:
	If all FTM initiators are left, disable TMR hardware CR.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

Note:
	1. only run in MT7628
	2. This function should be called AFTER the last peer state set to FTMPEER_UNUSED.

========================================================================
*/
VOID
FtmTmrDisableCheck(
    IN PRTMP_ADAPTER    pAd
	)
{
	UINT8 idx;

	/* is anyone alive ? */
	for (idx = 0; idx < MAX_FTM_TBL_SIZE; idx ++)
		if (pAd->pFtmCtrl->FtmPeer[idx].State > FTMPEER_UNUSED)
			return;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
			 ("%s(): All initiators peers are left, disable TMR CR.\n", __FUNCTION__));

	/* All initiators peers are left, disable TMR CR */
	FtmTmrCRSet(pAd, TMR_DISABLE);
}


/*
========================================================================
Routine Description:
	Decide the FTM Verdict Parameter and response by Tx FTM frame.

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer
	bChk1stPeer 	- Check it is the first FTM peer or not.

Return Value:
	None
	
========================================================================
*/
VOID
FtmParameterNego(
    IN PRTMP_ADAPTER		pAd,
    IN PFTM_PEER_ENTRY		pEntry,
    IN BOOLEAN				bChk1stPeer
	)
{
	PFTM_CTRL pFtm = pAd->pFtmCtrl;
	pEntry->State = FTMPEER_NEGO;

	/* Obtain VerdictParm */
	NdisCopyMemory(&pEntry->VerdictParm, &pEntry->PeerReqParm, sizeof(FTM_PARAMETER));
	pEntry->VerdictParm.status = FTMSTATUS_SUCCESS;

	/* Overridden: num_burst_exponent */
	switch (pEntry->PeerReqParm.num_burst_exponent)
	{
		case FTMBST_NO_PREFERENCE:
			/* 802.11mc D3.0: 8.4.2.166 (p.1041): 
				The value 15 in an initial Fine Timing Measurement Request frame indicates no preference by the initiating STA.
			*/
			pEntry->VerdictParm.num_burst_exponent = pFtm->num_burst_exponent;
			break;
	}

	/* asap_capable: always set to 1 */
	pEntry->VerdictParm.asap_capable = 1;
	pEntry->VerdictParm.ftm_format_and_bw = FTM_BW_HT_BW20;
	// TODO: Complete it!

	if (pEntry->PeerReqParm.ftms_per_burst == 0)
		pEntry->VerdictParm.ftms_per_burst = pFtm->ftms_per_burst;


	/* Overridden: min_delta_ftm */
	if (pEntry->PeerReqParm.min_delta_ftm % 10)
	{
		pEntry->VerdictParm.min_delta_ftm = (pEntry->PeerReqParm.min_delta_ftm/10 + 1) * 10;
	}

	if (pEntry->PeerReqParm.min_delta_ftm == 0)
		pEntry->VerdictParm.min_delta_ftm = pFtm->min_delta_ftm;

	/* no less than my default setting */
	if (pFtm->min_delta_ftm > pEntry->VerdictParm.min_delta_ftm)
	{
		/* 802.11mc D3.0: 10.24.6.3 (p.1717): 
			The responding STA¡¦s selection of the Min Delta FTM value shall be greater than or equal to 
			the corresponding value requested by the initiating STA.
		*/
		pEntry->VerdictParm.min_delta_ftm = pFtm->min_delta_ftm;
	}

	/* Partial TFS, unit: ms */
	{
		UINT32 hTsf, lTsf;
		UINT16 PartialTSF;

		/* Get current TSF */
		AsicGetTsfTime(pAd, &hTsf, &lTsf, HW_BSSID_0);

		/* partial_tsf_timer: b[25:10] of TSF */
		PartialTSF = (UINT16)((lTsf & 0x03FFFC00) >> 10);

		if (pEntry->PeerReqParm.asap)
		{
			/* workaround with Marvell: prevent to send too early, 2015.04.17 */
			PartialTSF -= 1;
		}
		else
		{
			PartialTSF += FTM_DEFAULT_PTSF_DELTA;

			/* no less than peer's request */
			if (PartialTSF < pEntry->PeerReqParm.partial_tsf_timer)
			{
				MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("Follow up initiator's PTSF: 0x%04X -> 0x%04X\n", 
					PartialTSF, pEntry->PeerReqParm.partial_tsf_timer));

				PartialTSF = pEntry->PeerReqParm.partial_tsf_timer;
			}
		}

		pEntry->VerdictParm.partial_tsf_timer = PartialTSF;
	}

	/* format and bandwidth */
	pEntry->VerdictParm.ftm_format_and_bw = FTM_BW_HT_BW20;

	/* burst_duration */
	/* Note: pEntry->PeerReqParm.burst_duration might be 15, means FTMBST_NO_PREFERENCE */
	pEntry->VerdictParm.burst_duration = pFtm->burst_duration;
	pEntry->VerdictParm.burst_period = pFtm->burst_period;

	/* zero all reserved field */
	pEntry->VerdictParm.rsv_1 = 0;
	pEntry->VerdictParm.rsv_2 = 0;
	pEntry->VerdictParm.rsv_3 = 0;


	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("%s(): Dump FTM Parameters, FTM_Request Rx\n", __FUNCTION__));
	FtmEntryDump(pAd, pEntry, DBG_LVL_INFO);


	/* Check hardware CR setting */
	if (bChk1stPeer && FtmTmrEnableCheck(pAd, pEntry))
		;	/* Waiting for TOAE calibration done (ExtEventTmrCalResultHandler). */
	else
		FtmEntryNegoDoneAction(pAd, pEntry);
}


/*
========================================================================
Routine Description:
	Response FTM frame after nego done.

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer

Return Value:
	None
	
========================================================================
*/
VOID
FtmEntryNegoDoneAction(
    IN PRTMP_ADAPTER		pAd,
    IN PFTM_PEER_ENTRY		pEntry
	)
{
	/* Begin to do FTM measurement */
	SendFTM(pAd, pEntry->Addr, FTMTX_START);

	pEntry->FtmCntDown = pEntry->VerdictParm.ftms_per_burst;
	pEntry->BurstCntDown = (1 << pEntry->VerdictParm.num_burst_exponent);

	/* Update State */
	if (pEntry->VerdictParm.asap)
	{
		pEntry->State = FTMPEER_MEASURING_IN_BURST;
		CNT_DOWN_DECREASE(pEntry->FtmCntDown);
		RTMPSetTimer(&pEntry->FtmTimer, FtmConvertMinDelta(pEntry->VerdictParm.min_delta_ftm));
	}
	else
	{
		pEntry->State = FTMPEER_MEASURING_WAIT_TRIGGER;

		if (pAd->pFtmCtrl->bNoDurTimeout == FALSE)
		{
			// 2015.04.14, add more expired time
			//RTMPSetTimer(&pEntry->FtmTimer, FtmConvertBurstTimeout(pEntry->VerdictParm.burst_duration));
			RTMPSetTimer(&pEntry->FtmTimer, 10000);	/* 10 seconds */
		}
	}
}


/*
========================================================================
Routine Description:
	Maintain the FTM/Burst countdown value and do related action.

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer

Return Value:
	None

========================================================================
*/
VOID 
FtmEntryCntDownAction(
    IN PRTMP_ADAPTER	pAd,
    IN PFTM_PEER_ENTRY	pEntry
	)
{
	/* FtmCntDown */
	CNT_DOWN_DECREASE(pEntry->FtmCntDown);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("   FtmCntDown: %d\n", pEntry->FtmCntDown));

	if (pEntry->FtmCntDown)
	{
		/* Schedule next FTM Tx */
		RTMPSetTimer(&pEntry->FtmTimer, FtmConvertMinDelta(pEntry->VerdictParm.min_delta_ftm));
		return;
	}

	/* BurstCntDown */
	CNT_DOWN_DECREASE(pEntry->BurstCntDown);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("   BurstCntDown: %d\n", pEntry->BurstCntDown));

	if (pEntry->BurstCntDown)
	{
		pEntry->FtmCntDown = pEntry->VerdictParm.ftms_per_burst;
		pEntry->State = FTMPEER_MEASURING_WAIT_TRIGGER;

		// TODO: Complete it!
		RTMPSetTimer(&pEntry->FtmTimer, pEntry->VerdictParm.burst_period * 100);
	}
	else
	{
		/* Finished. */
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,("T1\n"));
		FtmEntryTerminate(pAd, pEntry, FALSE, FALSE);
	}
}


/*
========================================================================
Routine Description:
	FTM modify procedure: terminate and restart

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer
	pRxParm 		- FTM patameter in Rx packet buffer

Return Value:
	None
	
========================================================================
*/
VOID
FtmParmModifyProcedure(
    IN PRTMP_ADAPTER		pAd,
    IN PFTM_PEER_ENTRY		pEntry,
    IN PFTM_PARAMETER		pRxParm
	)
{ 
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,("T2\n"));
	FtmEntryTerminate(pAd, pEntry, FALSE, FALSE);

	NdisCopyMemory(&pEntry->PeerReqParm, pRxParm, sizeof(FTM_PARAMETER));
	FtmParameterNego(pAd, pEntry, FALSE);
}


/*
========================================================================
Routine Description:
	Handle the FTM request with start/continue/modify.

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer
	bTxFTM		- if TRUE, send FTM frame to stop the initiator
	bCheckAllLeft	- if all FTM entry are left, disable TMR CR.

Return Value:
	TRUE: Termination Success
	FALSE: Termination Failed 

========================================================================
*/
BOOLEAN
FtmEntryTerminate(
    IN PRTMP_ADAPTER	pAd,
    IN PFTM_PEER_ENTRY	pEntry,
    IN BOOLEAN			bTxFTM,
    IN BOOLEAN			bCheckAllLeft
	)
{
	BOOLEAN TimerCancelled;
	ULONG IrqFlags = 0;

	if (!pEntry)
		return FALSE;

	if (pEntry->State == FTMPEER_UNUSED)
		return FALSE;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				("Terminate: bTxFTM=%d, bCheckAllLeft=%d, %02x:%02x:%02x:%02x:%02x:%02x\n", 
				bTxFTM, bCheckAllLeft, PRINT_MAC(pEntry->Addr)));

	if (bTxFTM)
		SendFTM(pAd, pEntry->Addr, FTMTX_STOP);

	RTMPCancelTimer(&pEntry->FtmTimer, &TimerCancelled);

	if (pEntry->PidList.Next)
	{
		RTMP_IRQ_LOCK(&pAd->pFtmCtrl->PidPendingQLock, IrqFlags);
		DlListDel(&pEntry->PidList);
		RTMP_IRQ_UNLOCK(&pAd->pFtmCtrl->PidPendingQLock, IrqFlags);
	}

	pEntry->PendingPid = FTMPID_NOT_WAITING;
	pEntry->BurstCntDown = 0;
	pEntry->FtmCntDown = 0;
	pEntry->State = FTMPEER_UNUSED;

	if (bCheckAllLeft)
		FtmTmrDisableCheck(pAd);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("Terminate: after del node\n"));
	FtmPidPendingQDump(pAd, DBG_LVL_INFO);

	return TRUE;
}


/*
========================================================================
Routine Description:
	Handle the FTM request with start/continue/modify.

Arguments:
	pAd 			- WLAN control block pointer
	pEntry		- FTM entry pointer
	pFtmReq		- Rx packet payload (bypass 802.11 header)
	FtmReqLen	- Rx packet length (bypass 802.11 header)

Return Value:
	None

========================================================================
*/
VOID
FtmRequestHandler(
    IN PRTMP_ADAPTER		pAd,
    IN PFTM_PEER_ENTRY		pEntry,
    IN PFTM_REQUEST_FRAME	pFtmReq,
    IN UINT32				FtmReqLen
	)
{
	PEID_STRUCT pEId;
	MEASUREMENT_REQ *pMsmtReq = NULL;
	MEASUREMENT_REQ *pLciReq = NULL;
	MEASUREMENT_REQ *pCivicReq = NULL;
	FTM_PARAMETER *pRxParm = NULL;
	UINT8 *pBuf;
	INT BufLen;
	BOOLEAN TimerCancelled;

	pEntry->bLciMsmtReq = FALSE;
	pEntry->bCivicMsmtReq = FALSE;
	pEntry->bFtmParm = FALSE;
	pBuf = (UINT8 *)(pFtmReq->Variable);
	BufLen = FtmReqLen - sizeof(FTM_REQUEST_FRAME);
	pEId = (PEID_STRUCT)pBuf;

	while (BufLen >= pEId->Len)
	{
		switch (pEId->Eid)
		{
		case IE_MEASUREMENT_REQUEST:
			pMsmtReq = (MEASUREMENT_REQ *)pEId;
			if (pMsmtReq->Length >= 3)	/* offest of "Type" */
			{
				switch (pMsmtReq->Type)
				{
				case MSMT_LCI_REQ:
					pEntry->bLciMsmtReq = TRUE;
					pEntry->LciToken = pMsmtReq->Token;
					pLciReq = pMsmtReq;
						MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
							("IE_MEASUREMENT_REQUEST[%d] Length:%d Type:0x%02X Token:%u\n", 
						IE_MEASUREMENT_REQUEST, pLciReq->Length, pLciReq->Type, pEntry->LciToken));
					break;

				case MSMT_LOCATION_CIVIC_REQ:
					pEntry->bCivicMsmtReq = TRUE;
					pEntry->CivicToken = pMsmtReq->Token;
					pCivicReq = pMsmtReq;
						MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
							("IE_MEASUREMENT_REQUEST[%d] Length:%d Type:0x%02X  Token:%u\n", 
						IE_MEASUREMENT_REQUEST, pCivicReq->Length, pCivicReq->Type, pEntry->CivicToken));
					break;
				}
			}
			break;

		case IE_FTM_PARM:
			/* FTM parameter parse and handle */
			pEntry->bFtmParm = TRUE;
			pRxParm = (FTM_PARAMETER *)(pEId->Octet);
			break;
		}

		/* Go to next IE */
		pBuf += (pEId->Len + 2);
		BufLen -= (pEId->Len + 2);
		pEId = (PEID_STRUCT)pBuf;
	}

	switch (pEntry->State)
	{
	case FTMPEER_IDLE:
		if (pRxParm) {
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("%s(): Rx Trigger FTMReq !!\n", __FUNCTION__));
			NdisCopyMemory(&pEntry->PeerReqParm, pRxParm, sizeof(FTM_PARAMETER));
		}
		FtmParameterNego(pAd, pEntry, pAd->pFtmCtrl->bCal);
		break;

	case FTMPEER_MEASURING_WAIT_TRIGGER:
		RTMPCancelTimer(&pEntry->FtmTimer, &TimerCancelled);
		pEntry->State = FTMPEER_MEASURING_IN_BURST;
		SendFTM(pAd, pEntry->Addr, FTMTX_ONGOING);

		if (pRxParm)
		{	// TODO: shoule modify or terminate FTM session or ...?
		}
		
		break;

	case FTMPEER_MEASURING_IN_BURST:
// Comment out to Prevent retry packets, 2015.04.16
//
//		if (pRxParm && !NdisEqualMemory(pRxParm, &pEntry->PeerReqParm, sizeof(FTM_PARAMETER)))
//		{
//			/* Modify FTM */
//			FtmParmModifyProcedure(pAd, pEntry, pRxParm);
//		}
		break;
	}
}


/*
========================================================================
Routine Description:
	Fillout TOD, TOA, TOD Error and TOA Error field in Tx FTM frame.

Arguments:
	pAd				- WLAN control block pointer
	pEntry			- FTM entry pointer
	pFtmFr			- FTM frame packet buffer

Return Value:
	None

========================================================================
*/
VOID
FtmFilloutToaTod(
	IN PRTMP_ADAPTER	pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN PFTM_FRAME		pFtmFr
	)
{
	TMR_FRM_STRUC	*pTmr = &pEntry->Tmr.HwReport;

	/* TOD, TOA */
	NdisCopyMemory(pFtmFr->TOD, (UINT8 *)&pEntry->Tmr.tod, 6);
	NdisCopyMemory(pFtmFr->TOA, (UINT8 *)&pEntry->Tmr.toa, 6);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, 
		("   [Tx]  TOD:0x%012llX  TOA:0x%012llX\n", pEntry->Tmr.tod, pEntry->Tmr.toa));

	/* Max Error */
	pFtmFr->TODError.MaxError = 150;	/* 150 * 0.2 ns = 30 ns */
	pFtmFr->TOAError.MaxError = 150;
	if (pTmr->TmrD6.field.ToD32 > pTmr->TmrD6.field.ToA32)
	{
		pFtmFr->TODError.NotConti = 1;
		pFtmFr->TOAError.NotConti = 1;
	}
}


ULONG FtmApendFtmHeader(
    PRTMP_ADAPTER		pAd,
	FTM_PEER_ENTRY     *pEntry,
	UINT8              *pOutBuffer,
	UINT8				DialogToken,
	UINT8				FollowUpToken,
	BOOLEAN				bFillTodToa)
{
	ULONG				TmpLen = 0;
	FTM_FRAME			FtmFr;

	NdisZeroMemory(&FtmFr, sizeof(FTM_FRAME));

	FtmFr.Category = CATEGORY_PUBLIC;
	FtmFr.Action = ACTION_FTM;
	FtmFr.DialogToken = DialogToken;
	FtmFr.FollowUpDialogToken = FollowUpToken;
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, 
		("   Token:0x%02X  F:0x%02X\n", FtmFr.DialogToken, FtmFr.FollowUpDialogToken));

	if (bFillTodToa == TRUE)
		FtmFilloutToaTod(pAd, pEntry, &FtmFr);

	MakeOutgoingFrame(pOutBuffer, &TmpLen,
	                  sizeof(FTM_FRAME),  &FtmFr,
	                  END_OF_ARGS);

	return TmpLen;
}


ULONG FtmApendLciReport(
    PRTMP_ADAPTER		pAd,
	FTM_PEER_ENTRY     *pEntry,
	UINT8              *pOutBuffer)
{
	PFTM_CTRL			pFtm = pAd->pFtmCtrl;
	MEASUREMENT_REPORT LciRpt;
	ULONG				FrameLen = 0;
	ULONG				TmpLen = 0;
	PMEASURE_REPORT_MODE pRptMode = NULL;

	NdisZeroMemory(&LciRpt, sizeof(MEASUREMENT_REPORT));
	pEntry->bLciMsmtReport = TRUE;

	LciRpt.ID = IE_MEASUREMENT_REPORT;
	LciRpt.Length = sizeof(MEASUREMENT_REPORT) - 2 ;
	LciRpt.Token = pEntry->LciToken;
	pRptMode = (PMEASURE_REPORT_MODE)&LciRpt.ReportMode;
	pRptMode->field.Late = 0;
	pRptMode->field.Incapable = 0;
	pRptMode->field.Refused = 0;
	LciRpt.Type = MSMT_LCI_RPT;

	/* 0. LCI */
	LciRpt.Length += (sizeof(MSMT_RPT_SUBELEMENT) + pFtm->LciHdr.Length);

	/* 4. Z element */
	if (pAd->pFtmCtrl->bSetZRpt) {
		LciRpt.Length += (pFtm->LciZ.Length + 2);
		pFtm->LciZ.Floor.field.ExpectedToMove = 0;
	}

	/* 6. Usage Rules/Policy */
	pFtm->LciUsage.SubElement = LCI_RPTID_USAGE_RULES;
	pFtm->LciUsage.RulesAndPolicy.field.RetransAllowed = 1;
	pFtm->LciUsage.RulesAndPolicy.field.RetExpiresPresent = 0;
	pFtm->LciUsage.RulesAndPolicy.field.LocationPolicy = 0;
	pFtm->LciUsage.Length = pFtm->LciUsage.RulesAndPolicy.field.RetExpiresPresent ? \
							(sizeof(USAGE_SUBELEMENT) - 2):
							(sizeof(USAGE_SUBELEMENT) - 2 \
							- sizeof(pFtm->LciUsage.RetExpires));
	LciRpt.Length += (pFtm->LciUsage.Length + 2);


	MakeOutgoingFrame(pOutBuffer, &FrameLen,
				  sizeof(MEASUREMENT_REPORT), &LciRpt,
				  sizeof(MSMT_RPT_SUBELEMENT), &pFtm->LciHdr,
				  pFtm->LciHdr.Length, &pFtm->LciField,
				  END_OF_ARGS);

	if (pAd->pFtmCtrl->bSetZRpt) {
		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
				  sizeof(Z_ELEMENT), &pFtm->LciZ,
				  END_OF_ARGS);
		FrameLen += TmpLen;
	}

	MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
				  (pFtm->LciUsage.Length + 2), &pFtm->LciUsage,
				  END_OF_ARGS);
	FrameLen += TmpLen;

	return  FrameLen;
}


ULONG FtmApendCivicReport(
    PRTMP_ADAPTER		pAd,
	FTM_PEER_ENTRY     *pEntry,
	UINT8              *pOutBuffer)
{
	PFTM_CTRL			pFtm = pAd->pFtmCtrl;
	MEASUREMENT_REPORT CivicRpt;
	UINT8 CivicLocationType;
	ULONG				FrameLen = 0;
	ULONG				TmpLen = 0;
	PMEASURE_REPORT_MODE pRptMode = NULL;

	NdisZeroMemory(&CivicRpt, sizeof(MEASUREMENT_REPORT));
	pEntry->bCivicMsmtReport = TRUE;

	CivicRpt.ID = IE_MEASUREMENT_REPORT;
	CivicRpt.Length = sizeof(MEASUREMENT_REPORT) - 2 ;
	CivicRpt.Token = pEntry->CivicToken;
	pRptMode = (PMEASURE_REPORT_MODE)&CivicRpt.ReportMode;
	pRptMode->field.Late = 0;
	pRptMode->field.Incapable = 0;
	pRptMode->field.Refused = 0;
	CivicRpt.Type = MSMT_LOCATION_CIVIC_RPT;

	/* Location Civic Report */
	CivicLocationType = CIVIC_TYPE_IETF_RFC4776_2006;
	CivicRpt.Length += sizeof(CivicLocationType);
	CivicRpt.Length += (sizeof(MSMT_RPT_SUBELEMENT) + pFtm->CivicHdr.Length);

	MakeOutgoingFrame(pOutBuffer, &FrameLen,
				  sizeof(MEASUREMENT_REPORT), &CivicRpt,
				  sizeof(CivicLocationType), &CivicLocationType,
				  sizeof(MSMT_RPT_SUBELEMENT), &pFtm->CivicHdr,
				  END_OF_ARGS);

	if (pFtm->CivicHdr.Length) {
		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
					  sizeof(LOCATION_CIVIC), &pFtm->Civic,
					  pFtm->Civic.CA_Length, pFtm->CA_Value,
					  END_OF_ARGS);

		FrameLen += TmpLen;
	}

	return FrameLen;
}


ULONG FtmApendFtmParam(
    PRTMP_ADAPTER		pAd,
	FTM_PEER_ENTRY     *pEntry,
	UINT8              *pOutBuffer)
{
	ULONG			TmpLen = 0;
	FTM_PARM_IE		FtmParm;

	NdisZeroMemory(&FtmParm, sizeof(FTM_PARM_IE));

	FtmParm.ID = IE_FTM_PARM;
	FtmParm.Length = sizeof(FTM_PARAMETER);
	NdisCopyMemory(&FtmParm.p, &pEntry->VerdictParm, sizeof(FTM_PARAMETER));

	MakeOutgoingFrame(pOutBuffer, &TmpLen,
	                  sizeof(FTM_PARM_IE), &FtmParm,
	                  END_OF_ARGS);
	
	return TmpLen;
}


VOID FtmFrameKickOut(
		PRTMP_ADAPTER   pAd,
		FTM_PEER_ENTRY  *pEntry,
		UINT8           *pOutBuffer,
		ULONG			FrameLen)
{
	/* Packet send out */
	//pEntry->bNeedTmr = TRUE;
	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pOutBuffer);

	/* Update Dialog Token (this is also the follow-up token of next pair) */
	pEntry->DialogToken = pAd->pFtmCtrl->DialogToken;

	/* 802.11mc D3.0: 10.24.6.4 (p.1721): 
	Dialog Tokens field values of consecutive Fine Timing Measurement frames
	shall, excluding retries, be consecutive, except when the value wraps around to 1.
	*/
	INCREASE_IDX(pAd->pFtmCtrl->DialogToken, 1, 256);

}

/*
========================================================================
Routine Description:
	Tx a FTM packet to start/continue a FTM procedure

Arguments:
	pAd				- WLAN control block pointer
	pEntry			- FTM entry pointer
	Reason			- FTMTX_START or FTMTX_ONGOING
	bReportLci 		- BOOLEAN, report LCI Location IE or not
	bReportCivic		- BOOLEAN, report Civic Location IE or not

Return Value:
	None

========================================================================
*/
VOID 
FtmTxForResponse(
	IN PRTMP_ADAPTER    pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN CHAR				Reason,
	IN BOOLEAN			bReportLci,
	IN BOOLEAN			bReportCivic
	)
{
	PUINT8				pOutBuffer = NULL;
	ULONG				FrameLen = 0;
	HEADER_802_11		FtmHdr;
	NDIS_STATUS			NStatus;

	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
	if (NStatus != NDIS_STATUS_SUCCESS)
	    return;

	NdisZeroMemory(&FtmHdr, sizeof(HEADER_802_11));

	/* Construct 802.11 header */
	FtmHdr.FC.FrDs = 1;
	MgtMacHeaderInit(pAd, &FtmHdr, SUBTYPE_ACTION, 0, pEntry->Addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.if_addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.bssid);

	MakeOutgoingFrame(pOutBuffer, &FrameLen,
	                  sizeof(HEADER_802_11), &FtmHdr,
	                  END_OF_ARGS);
	
	switch (Reason)
	{
		case FTMTX_START:
		{
			/* case 1: START and ASAP */
			/* case 2: START and non-ASAP */
			pEntry->bLciMsmtReport = FALSE;
			pEntry->bCivicMsmtReport = FALSE;
			
			/* Construct FTM frame */
			FrameLen += FtmApendFtmHeader(pAd,
							  pEntry,
							  (pOutBuffer+FrameLen),
							  pAd->pFtmCtrl->DialogToken,
							  0,
							  FALSE);

			/* Construct LCIMsmtReport */
			if (pEntry->bLciMsmtReq && bReportLci) {
				FrameLen += FtmApendLciReport(pAd,
								pEntry,
								(pOutBuffer+FrameLen));
			}

			/* Construct CivicRpt */
			if (pEntry->bCivicMsmtReq && bReportCivic) {
				FrameLen += FtmApendCivicReport(pAd,
								pEntry,
								(pOutBuffer+FrameLen));
			}
			
			/* Construct FTM Parameter */
			FrameLen += FtmApendFtmParam(pAd,
								pEntry,
								(pOutBuffer+FrameLen));

			pEntry->bNeedTmr = TRUE;
			FtmFrameKickOut(pAd, pEntry, pOutBuffer, FrameLen);

			break;
		}

		case FTMTX_ONGOING:
		{
			/* case 3: ONGOING and 1st FTM */
			/* case 4: ONGOING and not 1st FTM */
			if (pEntry->FtmCntDown == pEntry->VerdictParm.ftms_per_burst) {
				/* Construct FTM frame */
				FrameLen += FtmApendFtmHeader(pAd,
							  pEntry,
							  (pOutBuffer+FrameLen),
							  pAd->pFtmCtrl->DialogToken,
							  0,
							  FALSE);				
			} else {
				/* Construct FTM frame */
				FrameLen += FtmApendFtmHeader(pAd,
							  pEntry,
							  (pOutBuffer+FrameLen),
							  pAd->pFtmCtrl->DialogToken,
							  pEntry->FollowUpToken,
							  TRUE);
			}

			pEntry->bNeedTmr = TRUE;
			FtmFrameKickOut(pAd, pEntry, pOutBuffer, FrameLen);
			break;
		}

		case FTMTX_RETRY:
		{
			/* case 5: FTM RETRY */
			/* Construct FTM frame */
			FrameLen += FtmApendFtmHeader(pAd,
						  pEntry,
						  (pOutBuffer+FrameLen),
						  pAd->pFtmCtrl->DialogToken,
						  pEntry->FollowUpToken,
						  FALSE);				

			pEntry->bNeedTmr = TRUE;
			FtmFrameKickOut(pAd, pEntry, pOutBuffer, FrameLen);
			pEntry->TransAndRetrans++;
			/* Schedule next FTM Tx */
			RTMPSetTimer(&pEntry->FtmTimer, FtmConvertMinDelta(pEntry->VerdictParm.min_delta_ftm));
			break;
		}

		default:
			break;
	}	
}


/*
========================================================================
Routine Description:
	Tx a FTM packet to stop a FTM procedure

Arguments:
	pAd				- WLAN control block pointer
	pEntry			- FTM entry pointer

Return Value:
	None

========================================================================
*/
VOID 
FtmTxForStop(
	IN PRTMP_ADAPTER    pAd,
	IN PFTM_PEER_ENTRY	pEntry
	)
{
    PUINT8          pOutBuffer = NULL;
    ULONG           FrameLen = 0;
    HEADER_802_11   FtmHdr;
	FTM_FRAME		FtmFr;
    NDIS_STATUS     NStatus;

	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
	if (NStatus != NDIS_STATUS_SUCCESS)
	    return;

	NdisZeroMemory(&FtmHdr, sizeof(HEADER_802_11));
	NdisZeroMemory(&FtmFr, sizeof(FTM_FRAME));


	/* Construct 802.11 header */
	FtmHdr.FC.FrDs = 1;
	MgtMacHeaderInit(pAd, &FtmHdr, SUBTYPE_ACTION, 0, pEntry->Addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.if_addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.bssid);

	/* Construct FTM frame */
	FtmFr.Category = CATEGORY_PUBLIC;
	FtmFr.Action = ACTION_FTM;
	FtmFr.DialogToken = 0;
	FtmFilloutToaTod(pAd, pEntry, &FtmFr);

	MakeOutgoingFrame(pOutBuffer, &FrameLen,
	                  sizeof(HEADER_802_11), &FtmHdr,
	                  sizeof(FTM_FRAME),  &FtmFr,
	                  END_OF_ARGS);

	/* Packet send out */
	pEntry->bNeedTmr = TRUE;
	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pOutBuffer);
}


/*
========================================================================
Routine Description:
	Tx a FTM packet

Arguments:
	pAd				- WLAN control block pointer
	Addr				- MAC address of Target FTM peer
	Reason			- FTMTX_ONGOING or FTMTX_STOP

Return Value:
	None

========================================================================
*/
VOID 
SendFTM(
    IN PRTMP_ADAPTER    pAd,
    IN UINT8			*Addr,
    IN CHAR				Reason
	)
{
	PFTM_PEER_ENTRY		pEntry;

	if (!Addr || !pAd)
		return;

	pEntry = FtmEntrySearch(pAd, Addr);
	if (!pEntry)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, 
			("%s()#%d: FTM_peer_entry search failed !\n", __FUNCTION__, __LINE__));
		return;
	}

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
			("Send FTM frame to %02x:%02x:%02x:%02x:%02x:%02x, Reason=%d\n",
			PRINT_MAC(Addr), Reason));

	switch (Reason)
	{
		case FTMTX_START:
			FtmTxForResponse(pAd, pEntry, Reason, pAd->pFtmCtrl->bSetLciRpt, pAd->pFtmCtrl->bSetCivicRpt);
			break;

		case FTMTX_ONGOING:
		case FTMTX_RETRY:
			FtmTxForResponse(pAd, pEntry, Reason, FALSE, FALSE);
			break;

		case FTMTX_STOP:
			FtmTxForStop(pAd, pEntry);
			break;
	}
}


/*
========================================================================
Routine Description:
	Parse the received FTM request frame and response it.

Arguments:
	pAd				- WLAN control block pointer
	Elem				- received packet

Return Value:
	None

========================================================================
*/
VOID 
ReceiveFTMReq(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem
	)
{
	FTM_PEER_ENTRY *pEntry;
	FTM_REQUEST_FRAME *pFtmReq;
	PFRAME_802_11 Fr;

	if (!Elem)
		return;

	if (Elem->MsgLen < (LENGTH_802_11 + sizeof(FTM_REQUEST_FRAME)) )
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("ACTION - FTM Request Frame length=%ld is too short!\n", Elem->MsgLen));
		return;
	}

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("ACTION - Rx FTM Request Frame\n"));
	FtmHexDump("FTMRequestFrame", Elem->Msg, Elem->MsgLen, DBG_LVL_INFO);

	Fr = (PFRAME_802_11)Elem->Msg;
	pEntry = FtmEntryGet(pAd, Fr->Hdr.Addr2);
	if (!pEntry)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("ACTION - FTM Request Frame, no available entry to handle\n"));
		return;
	}

	/* Check if TOAE Calibration is on-going */
	if (VALIDATE_FTM_ENTRY_IDX(pAd->pFtmCtrl->CalPeerIdx))
	{
		if (pAd->pFtmCtrl->CalPeerIdx != GET_FTM_PEER_IDX(pAd, pEntry))
		{
			/* Not the original target peer: reject it ! */
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,("T3\n"));
			FtmEntryTerminate(pAd, pEntry, TRUE, FALSE);
		}
	}

	pFtmReq = (FTM_REQUEST_FRAME *) &Elem->Msg[LENGTH_802_11];

	switch (pFtmReq->Trigger)
	{
		case FTM_TRIGGER_START_OR_CONTI:
			FtmRequestHandler(pAd, pEntry, pFtmReq, Elem->MsgLen - LENGTH_802_11);
			break;

		case FTM_TRIGGER_STOP:
			MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,("T4\n"));
			FtmEntryTerminate(pAd, pEntry, FALSE, FALSE);
			break;
	}
}


/*
========================================================================
Routine Description:
	Parse the received FTM frame.

Arguments:
	pAd				- WLAN control block pointer
	Elem				- received packet

Return Value:
	None

========================================================================
*/
VOID 
ReceiveFTM(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	FTM_FRAME *pFtm;
	MEASUREMENT_REPORT *pLCIMsmtReport = NULL;
	MEASUREMENT_REPORT *pLocationCivicMsmtRport = NULL;
	FTM_PARAMETER *parm = NULL;
	FTM_PEER_ENTRY *pEntry = NULL;
	PFRAME_802_11   Fr = NULL;
	UINT8 *pBuf;
	UINT16 SN;
#ifdef FTM_INITIATOR
	PFTM_MAPPING pCurNode, pMappingNode;
	ULONG IrqFlags = 0;
#endif /* FTM_INITIATOR */

	if (!Elem)
		return;

	if (Elem->MsgLen < (LENGTH_802_11 + sizeof(FTM_FRAME)) )
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, 
			("ACTION - FTM Frame length=%ld is too short! (sizeof(FTM_FRAME)=%d)(sum=%d)\n", 
			Elem->MsgLen, sizeof(FTM_FRAME), (LENGTH_802_11 + sizeof(FTM_FRAME))));
		return;
	}

	NdisCopyMemory((UINT8 *)&SN, &Elem->Msg[22], 2);
	Fr = (PFRAME_802_11)Elem->Msg;
	pEntry = FtmEntryGet(pAd, Fr->Hdr.Addr2);
	if (!pEntry)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("ACTION - FTM Frame, no available entry to handle\n"));
		return;
	}

	pFtm = (FTM_FRAME *) &Elem->Msg[LENGTH_802_11];
	pBuf = (UINT8 *)(pFtm->Variable);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("ACTION - Rx FTM Frame\n"));
	FtmHexDump("FTMFrame", Elem->Msg, Elem->MsgLen, DBG_LVL_INFO);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("FTM Content\n"));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  Category=%d\n", pFtm->Category));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  Action=%d\n", pFtm->Action));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("   Token:0x%02X FollowUp:0x%02X SN:0x%04X(%d)\n", pFtm->DialogToken, pFtm->FollowUpDialogToken, SN, Fr->Hdr.Sequence));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  TOD: %02X %02X %02X %02X %02X %02X\n", pFtm->TOD[0], pFtm->TOD[1], pFtm->TOD[2], pFtm->TOD[3], pFtm->TOD[4], pFtm->TOD[5]));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  TOA: %02X %02X %02X %02X %02X %02X\n", pFtm->TOA[0], pFtm->TOA[1], pFtm->TOA[2], pFtm->TOA[3], pFtm->TOA[4], pFtm->TOA[5]));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  TODError: NotConti=%d, MaxError=%d\n", pFtm->TODError.NotConti, pFtm->TODError.MaxError));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("	  TOAError: NotConti=%d, MaxError=%d\n", pFtm->TOAError.NotConti, pFtm->TOAError.MaxError));

	NdisCopyMemory((UINT8 *)&pEntry->Tmr.tod, pFtm->TOD, 6);
	NdisCopyMemory((UINT8 *)&pEntry->Tmr.toa, pFtm->TOA, 6);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
		("   [Rx]  TOD:0x%012llX  TOA:0x%012llX\n", pEntry->Tmr.tod, pEntry->Tmr.toa)); 

#ifdef FTM_INITIATOR
	/* Record Rx information and get mapping node */
	pCurNode = FtmSearchRxMappingNodeBySN(pAd, pEntry, SN);
	if (pCurNode)
	{
		pCurNode->DialogToken = pFtm->DialogToken;
		pCurNode->FollowUpToken = pFtm->FollowUpDialogToken;
		pCurNode->PeerTOA = pEntry->Tmr.toa;
		pCurNode->PeerTOD = pEntry->Tmr.tod;

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
			("    sn:0x%04X DialogToken:0x%02X FollowUpToken:0x%02X (Next:0x%p Prev:0x%p)\n", 
			pCurNode->sn, pCurNode->DialogToken, pCurNode->FollowUpToken, pCurNode->list.Next, pCurNode->list.Prev));

		if (pFtm->FollowUpDialogToken)
		{
			pMappingNode = FtmGetRxMappingNodeByToken(pAd, pEntry, pFtm->FollowUpDialogToken);
			if (pMappingNode)
			{
				INT32 RTT;

				/* Calculate RTT */
				RTT = FtmRTTCalculate(
					&pCurNode->PeerTOD, 	/* T1: TOD of FTM Responder */
					&pMappingNode->tmr.toa,	/* T2: TOA of FTM Initiator */ 
					&pMappingNode->tmr.tod,	/* T3: TOD of FTM Initiator */
					&pCurNode->PeerTOA);	/* T4: TOA of FTM Reaponder */

				MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, 
					("   RTT: %d ns	(0x%02X 0x%04X)\n", RTT, pFtm->FollowUpDialogToken, pMappingNode->sn));

				/* Dequeue pMappingNode */
				{
					PFTM_MAPPING pNode, pPrev;

					RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);

					for (pNode = DlListEntry(&pMappingNode->list, FTM_MAPPING, list), 
						pPrev = DlListEntry(pNode->list.Prev, FTM_MAPPING, list); 
						&pNode->list != (&pEntry->RxTmrQ); 
						pNode = pPrev, pPrev = DlListEntry(pPrev->list.Prev, FTM_MAPPING, list))
					{
						DlListDel(&pNode->list);
						os_free_mem(pNode);
					}

					RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);
				}
			}
		}

		if (!pCurNode->DialogToken)
		{
			/* Dequeue pCurNode */
			RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
			DlListDel(&pCurNode->list);
			os_free_mem(pCurNode);
			RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);
		}
	}
#endif /* FTM_INITIATOR */

	if (*pBuf == IE_MEASUREMENT_REPORT)
	{
		pLCIMsmtReport = (MEASUREMENT_REPORT *)pBuf;
		pBuf += (pLCIMsmtReport->Length + 2);

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
			("IE_MEASUREMENT_REPORT[%d] Length:%d\n", 
			IE_MEASUREMENT_REPORT, pLCIMsmtReport->Length));
	}

	if (*pBuf == IE_MEASUREMENT_REPORT)
	{
		pLocationCivicMsmtRport = (MEASUREMENT_REPORT *)pBuf;
		pBuf += (pLocationCivicMsmtRport->Length + 2);

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, 
			("IE_MEASUREMENT_REPORT[%d] Length:%d\n", 
			IE_MEASUREMENT_REPORT, pLocationCivicMsmtRport->Length));
	}

	if (*pBuf == IE_FTM_PARM)
	{
		parm = (FTM_PARAMETER *)(pBuf + 2);

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO, ("Dump FTM Parameters, FTM Rx\n"));
		FtmParmDump(pAd, parm, DBG_LVL_TRACE);
	}

	if (pAd->pFtmCtrl->WaitForNego)
	{
		pAd->pFtmCtrl->WaitForNego = FALSE;
		RTMPSetTimer(&pAd->pFtmCtrl->FtmReqTimer, 10);	/* partial tsf ? */
	}
}


/*
========================================================================
Routine Description:
	FTM Timer callnack function

Arguments:
	FunctionContext	- FTM Peer Entry pointer

Return Value:
	None

========================================================================
*/
VOID FtmTxTimerCallback(
	IN PVOID SystemSpecific1, 
	IN PVOID FunctionContext, 
	IN PVOID SystemSpecific2, 
	IN PVOID SystemSpecific3) 
{
	if (FunctionContext)
	{
		PFTM_PEER_ENTRY pEntry = (PFTM_PEER_ENTRY)FunctionContext;
		PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)pEntry->pAd;

		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			("%s()#%d: idx=%d, State=%d\n", __FUNCTION__, __LINE__, 
			GET_FTM_PEER_IDX(pAd, pEntry), pEntry->State));

		switch (pEntry->State)
		{
			case FTMPEER_MEASURING_WAIT_TRIGGER:
				MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,("T5\n"));
				FtmEntryTerminate(pAd, pEntry, TRUE, FALSE);
				break;

			case FTMPEER_MEASURING_IN_BURST:
				if ((pEntry->bGotTmr) || (pEntry->TransAndRetrans == FTM_MAX_NUM_OF_RETRY))
				{
					/* Case 1. Got at least one TMR report interrupt => Ready to Tx FTM. */
					/* Case 2. TMR waiting Timeout => Send invalid TOA/TOD FTM. */
					if (!pEntry->bGotTmr)
					{
						MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
							("%s()#%d: idx=%d, TMR waiting timeout, discard PendingPid 0x%02X !\n", 
							__FUNCTION__, __LINE__, GET_FTM_PEER_IDX(pAd, pEntry), pEntry->PendingPid));
					}

					if (FtmDeqPidPendingNode(pAd, pEntry) == NDIS_STATUS_SUCCESS) {
						SendFTM(pAd, pEntry->Addr, FTMTX_ONGOING);
					} 
				}
				else
				{
					MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
						("%s()#%d: idx=%d, State=%d, PendingPid=0x%02X, FTMTX_RETRY\n", __FUNCTION__, __LINE__, 
						GET_FTM_PEER_IDX(pAd, pEntry), pEntry->State, pEntry->PendingPid));
					if (FtmDeqPidPendingNode(pAd, pEntry) == NDIS_STATUS_SUCCESS) {
						SendFTM(pAd, pEntry->Addr, FTMTX_RETRY);
					} 
				}
				break;
		}
	}
}

VOID FtmSendCivicToDaemon(IN PRTMP_ADAPTER pAd)
{
	//construct anqp location event to daemon , using pAd->pFtmCtrl->Civic
	PUINT8				pOutBuffer = NULL;
	ULONG				FrameLen = 0;
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;		
	PNET_DEV NetDev = pAd->ApCfg.MBSSID[pObj->ioctl_if].wdev.if_dev;
	UINT8 CivicLocationType = CIVIC_TYPE_IETF_RFC4776_2006;
	INT NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
	if (NStatus != NDIS_STATUS_SUCCESS)
		return;

	MakeOutgoingFrame(pOutBuffer, &FrameLen,							  
					  sizeof(CivicLocationType), &CivicLocationType,
					  sizeof(MSMT_RPT_SUBELEMENT), &pAd->pFtmCtrl->CivicHdr,
					  sizeof(LOCATION_CIVIC), &pAd->pFtmCtrl->Civic,
					  pAd->pFtmCtrl->Civic.CA_Length, pAd->pFtmCtrl->CA_Value,
					  END_OF_ARGS); 			

	SendLocationElementEvent(NetDev,pOutBuffer,FrameLen,AP_CIVIC_LOCATION);
	MlmeFreeMemory(pOutBuffer);
}

VOID FtmSendLciToDaemon(IN PRTMP_ADAPTER pAd)
{
	//construct anqp location event to daemon , using pAd->pFtmCtrl->Civic
	PUINT8				pOutBuffer = NULL;
	ULONG				FrameLen = 0, TmpLen = 0;
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;		
	PNET_DEV NetDev = pAd->ApCfg.MBSSID[pObj->ioctl_if].wdev.if_dev;
	INT NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
	if (NStatus != NDIS_STATUS_SUCCESS)
		return;

	MakeOutgoingFrame(pOutBuffer, &FrameLen,				  
				  sizeof(MSMT_RPT_SUBELEMENT), &pAd->pFtmCtrl->LciHdr,
				  pAd->pFtmCtrl->LciHdr.Length, &pAd->pFtmCtrl->LciField,
				  END_OF_ARGS);

	if (pAd->pFtmCtrl->bSetZRpt) {
		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
				  sizeof(Z_ELEMENT), &pAd->pFtmCtrl->LciZ,
				  END_OF_ARGS);
		FrameLen += TmpLen;
	}

	MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
				  (pAd->pFtmCtrl->LciUsage.Length + 2), &pAd->pFtmCtrl->LciUsage,
				  END_OF_ARGS);
	FrameLen += TmpLen; 			

	SendLocationElementEvent(NetDev,pOutBuffer,FrameLen,AP_GEOSPATIAL_LOCATION);
	MlmeFreeMemory(pOutBuffer);
}


#ifdef FTM_INITIATOR
/*
========================================================================
Routine Description:
	Dubug purpose, dump RxTmrQ link list content of the FTM Peer Entry.

Arguments:
	pAd				- WLAN control block pointer
	pEntry			- FTM Peer Entry pointer
	DbgLvl			- Level of debug message

Return Value:
	None

========================================================================
*/
VOID 
FtmRxTmrQDump(
	IN PRTMP_ADAPTER	pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN UINT32			DbgLvl
	)
{
	PFTM_MAPPING pNode, pNext;
	ULONG IrqFlags = 0;
	int num = 0;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("FTM RxTmrQ Dump\n"));

	RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pEntry->RxTmrQ, FTM_MAPPING, list)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, 
			("    sn:0x%04X DialogToken:0x%02X FollowUpToken:0x%02X [TOA:0x%012llX TOD:0x%012llX] [TOD:0x%012llX TOA:0x%012llX]\n", 
			pNode->sn, pNode->DialogToken, pNode->FollowUpToken,
			pNode->tmr.toa, pNode->tmr.tod, 
			pNode->PeerTOD, pNode->PeerTOA
			));

		num ++;
	}
	RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DbgLvl, ("    Total %d nodes\n", num));
}


/*
========================================================================
Routine Description:
	Search a FTM_MAPPING node with specific Sequence Number (along with fragment number).

Arguments:
	pAd 			- WLAN control block pointer
	Addr			- MAC adress of target initiator
	sn			- 802.11 frame Sequence Number (along with fragment number, 2 bytes)

Return Value:
	FTM_MAPPING node pointer; NULL means search failed.

========================================================================
*/
PFTM_MAPPING 
FtmSearchRxMappingNodeBySN(
    IN PRTMP_ADAPTER    pAd,
	IN PFTM_PEER_ENTRY	pEntry,
    IN UINT16			sn
    )
{
	PFTM_MAPPING pNode, pNext;
	PFTM_MAPPING pTarget = NULL;
	ULONG IrqFlags = 0;

	if (!pAd || !pEntry)
		return NULL;

	RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pEntry->RxTmrQ, FTM_MAPPING, list)
	{
		if (pNode->sn == sn)
		{
			pTarget = pNode;
			break;
		}
	}
	RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);

	return pTarget;
}


/*
========================================================================
Routine Description:
	Search a FTM_MAPPING node with specific DialogToken.

Arguments:
	pAd 			- WLAN control block pointer
	Addr			- MAC adress of target initiator
	DialogToken	- DialogToken for FTM frame

Return Value:
	FTM_MAPPING node pointer; NULL means search failed.

========================================================================
*/
PFTM_MAPPING 
FtmGetRxMappingNodeByToken(
	IN PRTMP_ADAPTER    pAd,
	IN PFTM_PEER_ENTRY	pEntry,
	IN UINT8	DialogToken
	)
{
	PFTM_MAPPING pNode, pNext;
	PFTM_MAPPING pTarget = NULL;
	ULONG IrqFlags = 0;

	RTMP_IRQ_LOCK(&pEntry->RxTmrQLock, IrqFlags);
	DlListForEachSafe(pNode, pNext, &pEntry->RxTmrQ, FTM_MAPPING, list)
	{
		if (pNode->DialogToken == DialogToken)
		{
			pTarget = pNode;
			break;
		}
	}
	RTMP_IRQ_UNLOCK(&pEntry->RxTmrQLock, IrqFlags);

	return pTarget;
}


/*
========================================================================
Routine Description:
	FTM Delta Calculation. Returned type is decreased to 32 bits.

Arguments:
	pStart 		- content is the start time, unit: 0.1 ns
	pEnd		- content is the end time, unit: 0.1 ns
	pStr			- debug message print purpose
	pDelta		- [OUT] result: "end - start" and translate unit to 1 ns.

Return Value:
	Check if delta value is greater than 32 bits. (should be not happened)

========================================================================
*/
BOOLEAN
FtmDeltaCalculate(
	IN UINT64	*pStart,
	IN UINT64	*pEnd,
	IN UINT8		*pStr,
	OUT INT32		*pDelta
	)
{
	UINT64 Delta_LL = *pEnd - *pStart;
	
	if (Delta_LL >> 32)
	{
		MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("(X) %s, Delta_LL = 0x%016llX", pStr, Delta_LL));
		return FALSE;
	}
	else
	{
		*pDelta = (INT32)(Delta_LL & 0x00000000FFFFFFFF);
		*pDelta /= 10;	// unit: 0.1 ns -> 1 ns
	}

	return TRUE;
}


/*
========================================================================
Routine Description:
	FTM RTT Calculation.

Arguments:
	T1, T2, T3, T4: as spec defined

Return Value:
	RTT, unit: ns

========================================================================
*/
INT32 
FtmRTTCalculate(
	IN UINT64	*pT1,
	IN UINT64	*pT2,
	IN UINT64	*pT3,
	IN UINT64	*pT4
	)
{
	INT32 DeltaTx, DeltaRx;
	INT32 RTT = 0;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, 
		("   T1:%15lld(0x%012llX)  T4:%15lld(0x%012llX) (0.1ns)\n", 
		*pT1, *pT1, *pT4, *pT4));
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, 
		("   T2:%15lld(0x%012llX)  T3:%15lld(0x%012llX) (0.1ns)\n", 
		*pT2, *pT2, *pT3, *pT3));

	if (FtmDeltaCalculate(pT1, pT4, "DeltaTx", &DeltaTx) && 
		FtmDeltaCalculate(pT2, pT3, "DeltaRx", &DeltaRx) )
	{
		RTT = (DeltaTx - DeltaRx) /2;
	}

	return RTT;
}


/*
========================================================================
Routine Description:
	Debug purpose, to show the content of FtmRxTmrQDump.

Arguments:
	UINT8, FTM entry index

Return Value:
	Success or not

========================================================================
*/
INT 
Show_FtmRxTmrQ_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 EntryIdx = 0;

	if (arg)
		EntryIdx = simple_strtol(arg, 0, 10);

	FtmRxTmrQDump(pAd, &pAd->pFtmCtrl->FtmPeer[EntryIdx], DBG_LVL_OFF);
	return TRUE;
}
#endif /* FTM_INITIATOR */


/*
========================================================================
Routine Description:
	FTM Request Timer callnack function

Arguments:
	FunctionContext	- WLAN control block pointer

Return Value:
	None

========================================================================
*/
VOID FtmReqTxTimerCallback(
	IN PVOID SystemSpecific1, 
	IN PVOID FunctionContext, 
	IN PVOID SystemSpecific2, 
	IN PVOID SystemSpecific3) 
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)FunctionContext;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, ("%s()#%d\n", __FUNCTION__, __LINE__));

	if (!FunctionContext)
		return;

	SendFTMReq_TestCmd(pAd, pAd->pFtmCtrl->Responder, FALSE, FALSE, FALSE);

	CNT_DOWN_DECREASE(pAd->pFtmCtrl->BurstCntDown);	
	if (pAd->pFtmCtrl->BurstCntDown)
	{
		RTMPSetTimer(&pAd->pFtmCtrl->FtmReqTimer, 100*pAd->pFtmCtrl->burst_period);
	}
}

/*
========================================================================
Routine Description:
	Send a FTM Request frame for test purpose

Arguments:
	pAd 			- WLAN control block pointer
	Addr			- Target Address (A1)
	bSetLci		- BOOLEAN, with LCI Location IE or not
	bSetCivic 	- BOOLEAN, with Civic Location IE or not
	bSetParm	- BOOLEAN, with FTM Parameter IE or not

Return Value:
	None

========================================================================
*/
VOID 
SendFTMReq_TestCmd(
    IN PRTMP_ADAPTER    pAd,
    IN UINT8			*Addr,
    IN BOOLEAN			bSetLci,
    IN BOOLEAN			bSetCivic,
    IN BOOLEAN			bSetParm
    )
{
	PFTM_CTRL			pFtm;
	PUINT8				pOutBuffer = NULL;
	ULONG				FrameLen = 0;
	ULONG				TmpLen = 0;
	PMEASURE_REQ_MODE	pReqMode = NULL;
	HEADER_802_11   	FtmHdr;
	FTM_REQUEST_FRAME	FtmReq;
	FTM_PARM_IE			FtmParm;
	NDIS_STATUS     	NStatus;

	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
	if (NStatus != NDIS_STATUS_SUCCESS)
	    return;

	pFtm = pAd->pFtmCtrl;
	NdisZeroMemory(&FtmHdr, sizeof(HEADER_802_11));
	NdisZeroMemory(&FtmReq, sizeof(FTM_REQUEST_FRAME));
	NdisZeroMemory(&FtmParm, sizeof(FTM_PARM_IE));

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE,
			("Send FTM Request frame to %02x:%02x:%02x:%02x:%02x:%02x\n",
			PRINT_MAC(Addr)));

	/* 802.11 Header */
	FtmHdr.FC.FrDs = 0;
	MgtMacHeaderInit(pAd, &FtmHdr, SUBTYPE_ACTION, 0, Addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.if_addr,
					pAd->ApCfg.MBSSID[BSS0].wdev.bssid);

	/* Construct FTM Request frame */
	FtmReq.Category = CATEGORY_PUBLIC;
	FtmReq.Action = ACTION_FTM_REQUEST;
	FtmReq.Trigger = 1;

	MakeOutgoingFrame(pOutBuffer, &FrameLen,
	                  sizeof(HEADER_802_11), &FtmHdr,
	                  sizeof(FTM_REQUEST_FRAME), &FtmReq,
	                  END_OF_ARGS);

	/* Construct LCIMsmtReport */
	if (bSetLci)
	{
		MEASUREMENT_REQ 	LciReq;
		MSMT_REQ_LCI		Lci;

		NdisZeroMemory(&LciReq, sizeof(MEASUREMENT_REQ));
		NdisZeroMemory(&Lci, sizeof(MSMT_REQ_LCI)); 

		LciReq.ID = IE_MEASUREMENT_REQUEST;
		LciReq.Length = sizeof(MEASUREMENT_REQ) - 2 + sizeof(MSMT_REQ_LCI);
		LciReq.Token = pFtm->TokenLCI;
		pFtm->TokenLCI ++;

		pReqMode = (PMEASURE_REQ_MODE)&LciReq.RequestMode;
		pReqMode->field.Enable = 0;	/* 802.11mc D3.0 p.1122 */
		LciReq.Type = MSMT_LCI_REQ;
		Lci.LocationSubj = LSUBJ_REMOTE;

		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
		                  sizeof(MEASUREMENT_REQ), &LciReq,
		                  sizeof(MSMT_REQ_LCI), &Lci,
		                  END_OF_ARGS);
		FrameLen += TmpLen;
	}

	/* Construct CivicReq */
	if (bSetCivic)
	{
		MEASUREMENT_REQ 	CivicReq;
		MSMT_REQ_CIVIC		Civic;

		NdisZeroMemory(&CivicReq, sizeof(MEASUREMENT_REQ));
		NdisZeroMemory(&Civic, sizeof(MSMT_REQ_CIVIC));

		CivicReq.ID = IE_MEASUREMENT_REQUEST;
		CivicReq.Length = sizeof(MEASUREMENT_REQ) - 2 + sizeof(MSMT_REQ_CIVIC);
		CivicReq.Token = pFtm->TokenCivic;
		pFtm->TokenCivic ++;

		pReqMode = (PMEASURE_REQ_MODE)&CivicReq.RequestMode;
		pReqMode->field.Enable = 0;	/* 802.11mc D3.0 p.1122 */
		CivicReq.Type = MSMT_LOCATION_CIVIC_REQ;
		Civic.LocationSubj = LSUBJ_REMOTE;
		Civic.CivicLocType = CIVIC_TYPE_IETF_RFC4776_2006;
		Civic.ServiceIntvUnits = CIVIC_UNIT_SECOND;
		Civic.ServiceInterval = 0;	/* 802.11mc D3.0 p.1122 */

		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
		                  sizeof(MEASUREMENT_REQ), &CivicReq,
		                  sizeof(MSMT_REQ_CIVIC), &Civic,
		                  END_OF_ARGS);
		FrameLen += TmpLen;
	}

	if (bSetParm)
	{
		/* Construct FTM Parameter */
		FtmParm.ID = IE_FTM_PARM;
		FtmParm.Length = sizeof(FTM_PARAMETER);
		FtmParm.p.num_burst_exponent = pFtm->num_burst_exponent;
		FtmParm.p.min_delta_ftm = pFtm->min_delta_ftm;
		FtmParm.p.partial_tsf_timer = 0;
		FtmParm.p.asap_capable = 1;
		FtmParm.p.asap = pFtm->asap;
		FtmParm.p.ftms_per_burst = pFtm->ftms_per_burst;
		FtmParm.p.ftm_format_and_bw = FTM_BW_HT_BW20;
		FtmParm.p.burst_duration = pFtm->burst_duration;
		FtmParm.p.burst_period = pFtm->burst_period;

		MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
		                  sizeof(FTM_PARM_IE), &FtmParm,
		                  END_OF_ARGS);
		FrameLen += TmpLen;
	}

	/* Packet send out */
	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pOutBuffer);

	FtmEntryGet(pAd, Addr);
}

INT Set_FtmCalFlag_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  RTMP_STRING *arg)
{
	UINT8 flag = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_TRACE, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->bCal, flag));

	pAd->pFtmCtrl->bCal = flag;
	return TRUE;
}

INT Set_FtmReqTx_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  RTMP_STRING *arg
    )
{
	RTMP_STRING *value;
	UINT8 macAddr[MAC_ADDR_LEN];
	INT i;
	PFTM_CTRL pFtm = pAd->pFtmCtrl;

	if(strlen(arg) != 17)  /*Mac address acceptable format 01:02:03:04:05:06 length 17 */
		return FALSE;

	for (i=0, value = rstrtok(arg,":"); value; value = rstrtok(NULL,":"))
	{
		if((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))) )
			return FALSE;  /*Invalid */

		AtoH(value, (UINT8 *)&macAddr[i++], 1);
	}

	NdisCopyMemory(pFtm->Responder, macAddr, MAC_ADDR_LEN);
	SendFTMReq_TestCmd(pAd, macAddr, pFtm->bSetLciReq, pFtm->bSetCivicReq, TRUE);

	pFtm->BurstCntDown = (1 << pFtm->num_burst_exponent);

	if (pFtm->asap) 
		CNT_DOWN_DECREASE(pFtm->BurstCntDown);

	if (pFtm->BurstCntDown)
	{
		if (!pFtm->asap)
			pFtm->WaitForNego = TRUE;
		else
			RTMPSetTimer(&pFtm->FtmReqTimer, 10 + 100*pFtm->burst_period);
	}

	return TRUE;
}


INT Set_FtmAsapReq_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 asap = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->asap, asap));

	pAd->pFtmCtrl->asap = asap;
	return TRUE;
}
INT Set_FtmMinDelta_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 MinDelta = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->min_delta_ftm, MinDelta));

	pAd->pFtmCtrl->min_delta_ftm = MinDelta;
	return TRUE;
}

INT Set_FtmNum_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 FtmNum = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->ftms_per_burst, FtmNum));

	pAd->pFtmCtrl->ftms_per_burst = FtmNum;
	return TRUE;
}

INT Set_FtmBurstExp_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 BurstExp = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->num_burst_exponent, BurstExp));

	pAd->pFtmCtrl->num_burst_exponent = BurstExp;
	return TRUE;
}


INT Set_FtmLciValue_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	PLCI_FIELD pLci = &pAd->pFtmCtrl->LciField;

	if (strlen(arg) != (sizeof(LCI_FIELD) * 2))
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("(X) illegal size: LCI should be %d hex bytes\n", sizeof(LCI_FIELD)));
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("example:\n"));
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("iwpriv ra0 set FtmLciValue=52834d12efd2b08b9b4bf1cc2c000041\n"));
		return TRUE;
	}

	/* update */
	AtoH(arg, pLci->byte, sizeof(LCI_FIELD));

	/* dump all */
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN, ("Update LciField\n"));
	FtmLciValueDump(pAd, pLci, DBG_LVL_WARN);

	return TRUE;
}


INT Set_FtmLciLat_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	PLCI_FIELD pLci = &pAd->pFtmCtrl->LciField;
	int arg_len;
	UINT8 high = 0;
	UINT32 low = 0;

	if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
		arg += 2;

	while (*arg == '0')
		arg ++;

	arg_len = strlen(arg);

	if (arg_len > 9)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("(X) illegal size of Latitude: exceed 34 bits\n"));
		return TRUE;
	}

	if (arg_len == 9)
	{
		AtoH(arg, &high, 1);
		high = (high >> 4) & 3;	/* take bit 33 and 32 */
		arg ++;
	}

	low = simple_strtol(arg, 0, 16);
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("Updated Latitude: 0x%02X %08X\n\n", high, low));

	/* update */
	pLci->field.Latitude_b0_b1 = low & 0x3;
	pLci->field.Latitude_b2_b33 = ((UINT32)high << 30) | (low >> 2);

	/* dump all */
	FtmLciValueDump(pAd, pLci, DBG_LVL_WARN);

	return TRUE;
}


INT Set_FtmLciLng_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	PLCI_FIELD pLci = &pAd->pFtmCtrl->LciField;
	int arg_len;
	UINT8 high = 0;
	UINT32 low = 0;

	if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
		arg += 2;

	while (*arg == '0')
		arg ++;

	arg_len = strlen(arg);

	if (arg_len > 9)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("(X) illegal size of Longitude: exceed 34 bits\n"));
		return TRUE;
	}

	if (arg_len == 9)
	{
		AtoH(arg, &high, 1);
		high = (high >> 4) & 3;	/* take bit 33 and 32 */
		arg ++;
	}

	low = simple_strtol(arg, 0, 16);
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("Updated Longitude: 0x%02X %08X\n\n", high, low));

	/* update */
	pLci->field.Longitude_b0_b1 = low & 0x3;
	pLci->field.Longitude_b2_b33 = ((UINT32)high << 30) | (low >> 2);

	/* dump all */
	FtmLciValueDump(pAd, pLci, DBG_LVL_WARN);

	return TRUE;
}


INT Set_FtmLciAlt_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	PLCI_FIELD pLci = &pAd->pFtmCtrl->LciField;
	UINT32 value;

	if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
		arg += 2;

	while (*arg == '0')
		arg ++;

	if (strlen(arg) > 8)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("(X) illegal size of Altitude: exceed 30 bits\n"));
		return TRUE;
	}

	value = simple_strtol(arg, 0, 16);
	value &= 0x3FFFFFFF;	/* bit31 and bit30 are invalid, truncate them */
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_WARN, ("Updated Altitude: 0x%08X\n\n", value));

	/* update */
	pLci->field.Altitude_b0_b21 = value & 0x3FFFFF;	/* mask bit0 ~ bit21 */
	pLci->field.Altitude_b22_b29 = value >> 22;

	/* dump all */
	FtmLciValueDump(pAd, pLci, DBG_LVL_WARN);

	return TRUE;
}


INT Set_FtmLciKnown_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);

	if (flag)
	{
		pAd->pFtmCtrl->LciHdr.Length = sizeof(LCI_FIELD);
		FtmSendLciToDaemon(pAd);
	}
	else
		pAd->pFtmCtrl->LciHdr.Length = 0;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d => LciHdr.Length=%d\n", __FUNCTION__, flag, pAd->pFtmCtrl->LciHdr.Length));

	return TRUE;
}


INT Set_FtmLciFlag_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): Request:%d->%d, Report:%d->%d\n", 
		__FUNCTION__, pAd->pFtmCtrl->bSetLciReq, flag, 
		pAd->pFtmCtrl->bSetLciRpt, flag));

	pAd->pFtmCtrl->bSetLciReq = flag;
	pAd->pFtmCtrl->bSetLciRpt = flag;
	return TRUE;
}


INT Set_FtmCivicKnown_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);

	if (flag)
	{		
		pAd->pFtmCtrl->CivicHdr.Length = sizeof(LOCATION_CIVIC) + pAd->pFtmCtrl->Civic.CA_Length;
		FtmSendCivicToDaemon(pAd);		
	}
	else
		pAd->pFtmCtrl->CivicHdr.Length = 0;

	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d => CivicHdr.Length=%d\n", __FUNCTION__, flag, pAd->pFtmCtrl->CivicHdr.Length));

	return TRUE;
}


INT Set_FtmCivicFlag_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): Request:%d->%d, Report:%d->%d\n", 
		__FUNCTION__, pAd->pFtmCtrl->bSetCivicReq, flag, 
		pAd->pFtmCtrl->bSetCivicRpt, flag));

	pAd->pFtmCtrl->bSetCivicReq = flag;
	pAd->pFtmCtrl->bSetCivicRpt = flag;
	return TRUE;
}


INT Set_FtmZFlag_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->bSetZRpt, flag));

	pAd->pFtmCtrl->bSetZRpt = flag;
	return TRUE;
}


INT Set_FtmBurstDuration_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 value = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->burst_duration, value));

	pAd->pFtmCtrl->burst_duration = value;
	return TRUE;
}


INT Set_FtmBurstPeriod_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 value = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->burst_period, value));

	pAd->pFtmCtrl->burst_period = value;
	return TRUE;
}


INT Set_FtmDisableFtmDurTimeout_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 flag = simple_strtol(arg, 0, 10);
	MTWF_LOG(DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_OFF, 
		("%s(): %d -> %d\n", __FUNCTION__, pAd->pFtmCtrl->bNoDurTimeout, flag));

	pAd->pFtmCtrl->bNoDurTimeout = flag;
	return TRUE;
}


INT Show_FtmEntry_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	UINT8 EntryIdx = 0;

	if (arg)
		EntryIdx = simple_strtol(arg, 0, 10);
	else
		EntryIdx = pAd->pFtmCtrl->LatestJoinPeer;

	FtmEntryDump(pAd, &pAd->pFtmCtrl->FtmPeer[EntryIdx], DBG_LVL_OFF);
	return TRUE;
}


INT Show_FtmPidList_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	FtmPidPendingQDump(pAd, DBG_LVL_OFF);
	return TRUE;
}


INT Show_FtmLciValue_Proc(
	IN RTMP_ADAPTER *pAd, 
	IN RTMP_STRING *arg
	)
{
	FtmLciValueDump(pAd, &pAd->pFtmCtrl->LciField, DBG_LVL_OFF);
	return TRUE;
}


struct anqp_frame {
	u16 info_id;
	u16 length;
	char variable[0];
} __attribute__ ((packed));

INT Send_ANQP_Req_For_Test(
	IN PRTMP_ADAPTER pAd,
	IN RTMP_STRING *arg)
{
	UCHAR *Buf,*buf_anqp, *Pos;	
	//RTMP_STRING *value;
	GAS_FRAME *GASFrame;
	UINT32 FrameLen = 0;
	UINT16 tmpLen = 0,tmp = 0;//,i;
	static UINT32 Token = 0;
	UINT32 anqp_req_len = 0;
	struct anqp_frame *anqp_req;
	UCHAR PeerMACAddr[MAC_ADDR_LEN]={0x00,0x0C,0x43,0xE1,0x76,0x28};
	ULONG choice = simple_strtol(arg, 0, 10);
	printk("%s  choice  %ld\n", __FUNCTION__,choice);

	//NdisZeroMemory(PeerMACAddr, MAC_ADDR_LEN);
	//query_ap_geospatial_location  , query_ap_civic_location ,query_ap_location_public_uri
	if(choice==1)
	{
		os_alloc_mem(NULL, (UCHAR **)&buf_anqp, sizeof(*anqp_req) + 2);
		anqp_req = (struct anqp_frame *)buf_anqp;

		anqp_req->info_id = cpu2le16(ANQP_QUERY_LIST);
		anqp_req_len += 2;

		anqp_req->length = cpu2le16(2);
		anqp_req_len += 2;

		Pos = anqp_req->variable;


		tmp = cpu2le16(ROAMING_CONSORTIUM_LIST);
		//tmp = cpu2le16(AP_GEOSPATIAL_LOCATION);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;
		/*
		tmp = cpu2le16(AP_CIVIC_LOCATION);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;	

		tmp = cpu2le16(AP_LOCATION_PUBLIC_IDENTIFIER_URI);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;

		anqp_req_len += 6;
		*/
		anqp_req_len += 2;
	}
	else
	{
		os_alloc_mem(NULL, (UCHAR **)&buf_anqp, sizeof(*anqp_req) + 6);
		anqp_req = (struct anqp_frame *)buf_anqp;

		anqp_req->info_id = cpu2le16(ANQP_QUERY_LIST);
		anqp_req_len += 2;

		anqp_req->length = cpu2le16(6);
		anqp_req_len += 2;

		Pos = anqp_req->variable;
		
		tmp = cpu2le16(AP_GEOSPATIAL_LOCATION);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;
		
		tmp = cpu2le16(AP_CIVIC_LOCATION);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;	

		tmp = cpu2le16(AP_LOCATION_PUBLIC_IDENTIFIER_URI);
		NdisMoveMemory(Pos, &tmp, 2);
		Pos += 2;

		anqp_req_len += 6;
		
	}	

	os_alloc_mem(NULL, (UCHAR **)&Buf, sizeof(*GASFrame) + anqp_req_len);

	if (!Buf)
	goto error0;

	NdisZeroMemory(Buf, sizeof(*GASFrame) + anqp_req_len);

	GASFrame = (GAS_FRAME *)Buf;		

	ActHeaderInit(pAd, &GASFrame->Hdr, PeerMACAddr ,pAd->CurrentAddress,
					   PeerMACAddr);

	FrameLen += sizeof(HEADER_802_11);

	GASFrame->Category = CATEGORY_PUBLIC;
	GASFrame->u.GAS_INIT_REQ.Action = ACTION_GAS_INIT_REQ;
	GASFrame->u.GAS_INIT_REQ.DialogToken = Token++; //Event->u.GAS_REQ_DATA.DialogToken;
	FrameLen += 3;

	Pos = GASFrame->u.GAS_INIT_REQ.Variable;
	*Pos++ = IE_ADVERTISEMENT_PROTO;
	*Pos++ = 2; /* Length field */
	*Pos++ = 0; /* Query response info field */
	*Pos++ = ACCESS_NETWORK_QUERY_PROTOCOL; /* Advertisement Protocol ID field */

	tmpLen = cpu2le16(anqp_req_len);
	NdisMoveMemory(Pos, &tmpLen, 2);
	Pos += 2;
	FrameLen += 6;

	NdisMoveMemory(Pos, buf_anqp, anqp_req_len);
	FrameLen += anqp_req_len; 

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("Location Anqp Req to %02x:%02x:%02x:%02x:%02x:%02x\n",
	    		PRINT_MAC(PeerMACAddr)));
	MiniportMMRequest(pAd, 0, Buf, FrameLen);


	os_free_mem(Buf);
	os_free_mem(buf_anqp);

	return TRUE;


	os_free_mem(Buf);
error0:
	return FALSE;
}

INT Send_NeighborReq_For_Test(
	IN PRTMP_ADAPTER pAd,
	IN RTMP_STRING *arg)
{
	HEADER_802_11 ActHdr;
	PUCHAR pOutBuffer = NULL;
	NDIS_STATUS NStatus;
	ULONG FrameLen;
	UINT8 DialogToken = RandomByte(pAd);
	UINT8 MeasurementToken = RandomByte(pAd);
	UCHAR PeerMACAddr[MAC_ADDR_LEN]={0x00,0x0C,0x43,0xE1,0x76,0x28};
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(). \n", __FUNCTION__));

	NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  /*Get an unused nonpaged memory */
	if(NStatus != NDIS_STATUS_SUCCESS)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s() allocate memory failed \n", __FUNCTION__));
		return FALSE;
	}

	/* build action frame header. */
	MgtMacHeaderInit(pAd, &ActHdr, SUBTYPE_ACTION, 0, PeerMACAddr,pAd->ApCfg.MBSSID[0].wdev.if_addr, PeerMACAddr);
						

	NdisMoveMemory(pOutBuffer, (PCHAR)&ActHdr, sizeof(HEADER_802_11));
	FrameLen = sizeof(HEADER_802_11);

	InsertActField(pAd, (pOutBuffer + FrameLen), &FrameLen,
			CATEGORY_RM, RRM_NEIGHTBOR_REQ);
	/* fill Dialog Token */
	InsertDialogToken(pAd, (pOutBuffer + FrameLen), &FrameLen, DialogToken);

	{
		ULONG TempLen;
		UCHAR ElemetnID=38,measurementType=MSMT_LCI_RPT,length=5,MeasurementMode=0;
		MakeOutgoingFrame((pOutBuffer + FrameLen),	&TempLen,
					1,				&ElemetnID,
					1,				&length,
					1,				&MeasurementToken,
					1,				&MeasurementMode,
					1,				&measurementType,
					END_OF_ARGS);

		FrameLen = FrameLen + TempLen;
	}

	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);

	if (pOutBuffer)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s() sent out Neighbor Req. \n", __FUNCTION__));
		MlmeFreeMemory(pOutBuffer);
	}

	return TRUE;
}

#endif /* FTM_SUPPORT */
