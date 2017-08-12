/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	andes_mt.h

	Abstract:

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifndef __ANDES_MT_H__
#define __ANDES_MT_H__

#include "mcu.h"
#include "mcu/mt_cmd.h"

#ifdef LINUX
#ifndef WORKQUEUE_BH
#include <linux/interrupt.h>
#endif
#endif /* LINUX */

#define GET_EVENT_FW_RXD_LENGTH(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_0.field.length)
#define GET_EVENT_FW_RXD_PKT_TYPE_ID(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_0.field.pkt_type_id)
#define GET_EVENT_FW_RXD_EID(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_1.field.eid)

#if (defined(MT7615) || defined(MT7622)) && !defined(UNIFY_FW_CMD)
#define GET_EVENT_FW_RXD_SEQ_NUM(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_1.field1.seq_num)
#else       
#define GET_EVENT_FW_RXD_SEQ_NUM(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_1.field.seq_num)      
#endif

#define GET_EVENT_FW_RXD_EXT_EID(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_2.field.ext_eid)

#define IS_IGNORE_RSP_PAYLOAD_LEN_CHECK(m) \
    (((struct cmd_msg *)(m))->attr.ctrl.expect_size == MT_IGNORE_PAYLOAD_LEN_CHECK) \
    ? TRUE : FALSE
#define GET_EVENT_HDR_ADDR(net_pkt) \
    (GET_OS_PKT_DATAPTR(net_pkt) + sizeof(EVENT_RXD))


#define GET_EVENT_HDR_ADD_PAYLOAD_TOTAL_LEN(event_rxd) \
    (((EVENT_RXD *)(event_rxd))->fw_rxd_0.field.length - sizeof(EVENT_RXD))


struct _RTMP_ADAPTER;
struct cmd_msg;


VOID AndesMTFillCmdHeader(struct cmd_msg *msg, PNDIS_PACKET net_pkt);
VOID AndesMTRxEventHandler(struct _RTMP_ADAPTER *pAd, UCHAR *data);
INT32 AndesMTLoadFw(struct _RTMP_ADAPTER *pAd);
INT32 AndesMTEraseFw(struct _RTMP_ADAPTER *pAd);

#if defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT)
INT32 AndesMTPciKickOutCmdMsg(struct _RTMP_ADAPTER *pAd, struct cmd_msg *msg);
#if defined(MT7615) || defined(MT7622)
INT32 AndesMTPciKickOutCmdMsgFwDlRing(struct _RTMP_ADAPTER *pAd, struct cmd_msg *msg);
INT32 AndesRestartCheck(struct _RTMP_ADAPTER *pAd);
#endif /* defined(MT7615) || defined(MT7622) */
VOID AndesMTPciFwInit(struct _RTMP_ADAPTER *pAd);
VOID AndesMTPciFwExit(struct _RTMP_ADAPTER *pAd);
#endif /* defined(RTMP_PCI_SUPPORT) || defined(RTMP_RBUS_SUPPORT) */



#ifdef TXBF_SUPPORT
VOID ExtEventBfStatusRead(struct _RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length);
#endif

#ifdef LED_CONTROL_SUPPORT
#ifdef MT7615
INT AndesLedEnhanceOP(
	struct _RTMP_ADAPTER *pAd,
	UCHAR led_idx,
	UCHAR tx_over_blink,
	UCHAR reverse_polarity,
	UCHAR band,
	UCHAR blink_mode,
	UCHAR off_time,
	UCHAR on_time,
	UCHAR led_control_mode
	);
#else 
INT AndesLedEnhanceOP(
	struct _RTMP_ADAPTER *pAd,
	UCHAR led_idx,
	UCHAR tx_over_blink,
	UCHAR reverse_polarity,
	UCHAR blink_mode,
	UCHAR off_time,
	UCHAR on_time,
	UCHAR led_control_mode
	);
#endif
#endif

INT32 AndesMTLoadRomPatch(struct _RTMP_ADAPTER *ad);
INT32 AndesMTEraseRomPatch(struct _RTMP_ADAPTER *ad);

#ifdef INTERNAL_CAPTURE_SUPPORT
VOID ExtEventWifiSpectrumRawDataDumpHandler(struct _RTMP_ADAPTER *pAd, UINT8 *Data, UINT32 Length);
NTSTATUS WifiSpectrumRawDataDumpHandler(struct _RTMP_ADAPTER *pAd, PCmdQElmt CMDQelmt);
#endif/*INTERNAL_CAPTURE_SUPPORT*/

#endif /* __ANDES_MT_H__ */

