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
	meminfo_list.h
*/
#ifndef __MEMINFO_LIST_H__
#define __MEMINFO_LIST_H__

#ifdef MEM_ALLOC_INFO_SUPPORT

#include "rtmp_comm.h"

#define POOL_ENTRY_NUMBER 32
#define HASH_SIZE 32

typedef struct _MEM_INFO_LIST_ENTRY
{
	struct _MEM_INFO_LIST_ENTRY *pNext;
	UINT32 EntryId;
	UINT32 MemSize;
	VOID *pMemAddr;
	VOID *pCaller;
} MEM_INFO_LIST_ENTRY;
typedef struct _MEM_INFO_LIST_POOL
{
	MEM_INFO_LIST_ENTRY Entry[POOL_ENTRY_NUMBER];
	struct  _MEM_INFO_LIST_POOL *pNext;
	UINT32 BitTbl;
	UINT32 Pid;
} MEM_INFO_LIST_POOL;
typedef struct _MEM_INFO_LIST
{
	MEM_INFO_LIST_ENTRY *pHead[HASH_SIZE];
	NDIS_SPIN_LOCK Lock;
	UINT32 EntryNumber;
	UINT32 PoolEntryNumber; /*entry number in Pool*/
	UINT32 CurAlcSize;
	UINT32 MaxAlcSize;
	MEM_INFO_LIST_POOL *Pool;
} MEM_INFO_LIST;

static inline VOID FreePool(MEM_INFO_LIST *MIList)
{
	MEM_INFO_LIST_POOL *temp, *Pool = MIList->Pool;

	while(Pool->pNext) /* free first Pool in MIListexit*/
	{
		if(Pool->pNext->BitTbl == 0xffffffff)
		{
			temp = Pool->pNext;
			Pool->pNext = temp->pNext;
			kfree(temp);
			MIList->PoolEntryNumber -= POOL_ENTRY_NUMBER;
		}
		else
		{
			Pool = Pool->pNext;
		}
	}
}
static inline MEM_INFO_LIST_ENTRY* GetEntryFromPool(MEM_INFO_LIST *MIList)
{
	UINT32 Index = 0, Pid = 0;
	MEM_INFO_LIST_POOL *Pool = MIList->Pool;
	if(!Pool)
	{
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: pool is null!!!\n", __FUNCTION__));
		return NULL;
	}
	Pid = Pool->Pid;
	while(Pool->BitTbl == 0)
	{
		if(Pid + 1 == Pool->Pid) Pid = Pool->Pid;
		if(!Pool->pNext)
		{
			UINT32 i;
			MEM_INFO_LIST_POOL *temp;

			temp = kmalloc(sizeof(MEM_INFO_LIST_POOL), GFP_ATOMIC);
			if(!temp)
			{
				MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("allocate new Pool fail!!!!!!!!\n"));
				return NULL;
			}
			temp->Pid = Pid + 1;
			for(i = 0;i < POOL_ENTRY_NUMBER;i++)
			{
				temp->Entry[i].EntryId = temp->Pid * POOL_ENTRY_NUMBER + i + 1;
				temp->Entry[i].pNext = NULL;
			}
			temp->BitTbl = 0xffffffff;
			Pool = MIList->Pool;
			while(Pool->Pid != Pid)
				Pool = Pool->pNext;
			temp->pNext = Pool->pNext;
			Pool->pNext = temp;
			MIList->PoolEntryNumber += POOL_ENTRY_NUMBER;
			Pool = temp;
			break;
		}
		else
			Pool = Pool->pNext;
	}
	while((Pool->BitTbl & (0x1 << Index)) == 0)
		Index++;
	if(Index > 31)
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s:fix me, err Index = %u",__FUNCTION__,Index));
	Pool->BitTbl ^= (0x1 << Index);
	return &Pool->Entry[Index];
}
static inline VOID ReleaseEntry(MEM_INFO_LIST *MIList, UINT32 EntryId)
{
	UINT32 Index;
	MEM_INFO_LIST_POOL *Pool = MIList->Pool;
	while(((Pool->Pid + 1) * POOL_ENTRY_NUMBER) < EntryId)
		Pool = Pool->pNext;
	Index = EntryId - Pool->Pid * POOL_ENTRY_NUMBER - 1;
	if(Index > 31)
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s:fix me, err Index = %u",__FUNCTION__,Index));
	Pool->BitTbl |= (0x1 << Index);
	if((Pool->BitTbl == 0xffffffff)
			&& (MIList->PoolEntryNumber > 2 * MIList->EntryNumber)
			&& (MIList->PoolEntryNumber > POOL_ENTRY_NUMBER))
		FreePool(MIList);
}
static inline UINT32 HashF(VOID *pMemAddr)
{
	LONG addr = (LONG)pMemAddr;
	UINT32 a = (addr & 0xF0) >> 4;
	UINT32 b = (addr & 0xF000) >> 12;
	UINT32 c = (addr & 0xF00000) >> 20;
	UINT32 d = (addr & 0xF0000000) >> 28;
	return (a + b + c + d) % HASH_SIZE;
}
static inline VOID MIListInit(MEM_INFO_LIST *MIList)
{
	UINT32 i;

	for(i = 0;i < HASH_SIZE;i++)
		MIList->pHead[i] = NULL;
	NdisAllocateSpinLock(NULL,&MIList->Lock);
	MIList->EntryNumber = 0;
	MIList->CurAlcSize = 0;
	MIList->MaxAlcSize = 0;
	MIList->PoolEntryNumber = POOL_ENTRY_NUMBER;
	MIList->Pool = kmalloc(sizeof(MEM_INFO_LIST_POOL), GFP_ATOMIC);
	for(i = 0;i < POOL_ENTRY_NUMBER;i++)
	{
		MIList->Pool->Entry[i].EntryId = i + 1;
		MIList->Pool->Entry[i].pNext = NULL;
	}
	MIList->Pool->Pid = 0;
	MIList->Pool->pNext = NULL;
	MIList->Pool->BitTbl = 0xffffffff;
}
static inline VOID MIListExit(MEM_INFO_LIST *MIList)
{
	UINT32 i = 0;
	ULONG IrqFlags = 0;
	MEM_INFO_LIST_ENTRY *ptr;
	MEM_INFO_LIST_POOL *temp;
	OS_INT_LOCK(&MIList->Lock, IrqFlags);
	for(i = 0;i < HASH_SIZE;i++)
	{
		while(MIList->pHead[i])
		{
			ptr = MIList->pHead[i];
			MIList->pHead[i] = ptr->pNext;
			kfree(ptr->pMemAddr);
		}
	}
	while(MIList->Pool)
	{
		temp = MIList->Pool;
		MIList->Pool = MIList->Pool->pNext;
		kfree(temp);
	}
	OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
	NdisFreeSpinLock(&MIList->Lock);
	MIList->EntryNumber = 0;
}

static inline MEM_INFO_LIST_ENTRY* MIListRemove(MEM_INFO_LIST *MIList, VOID* pMemAddr)
{
	UINT32 Index = HashF(pMemAddr);
	ULONG IrqFlags = 0;
	MEM_INFO_LIST_ENTRY *ptr;
	if(MIList->Pool == NULL)
	{
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: MIList has not initialed!\n", __FUNCTION__));
		return NULL;
	}

	OS_INT_LOCK(&MIList->Lock, IrqFlags);
	ptr = MIList->pHead[Index];
	if(!ptr)
	{
		OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
		return NULL;
	}
	else if(ptr->pMemAddr == pMemAddr)
	{
		MIList->pHead[Index] = ptr->pNext;
		ptr->pNext = NULL;
		ReleaseEntry(MIList, ptr->EntryId);
		MIList->EntryNumber--;
		MIList->CurAlcSize -= ptr->MemSize;
		OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
		return ptr;
	}
	while(ptr->pNext)
	{
		if(ptr->pNext->pMemAddr == pMemAddr)
		{
			MEM_INFO_LIST_ENTRY *temp;
			temp = ptr->pNext;
			ptr->pNext = temp->pNext;
			temp->pNext = NULL;
			ReleaseEntry(MIList, temp->EntryId);
			MIList->EntryNumber--;
			MIList->CurAlcSize -= temp->MemSize;
			OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
			return temp;
		}
		ptr = ptr->pNext;
	}
	OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
	return NULL;
}


static inline VOID MIListAddHead(MEM_INFO_LIST *MIList, UINT32 Size, VOID* pMemAddr, VOID* pCaller)
{
	UINT32 Index = HashF(pMemAddr);
	ULONG IrqFlags = 0;
	MEM_INFO_LIST_ENTRY *newEntry;
	if(MIList->Pool == NULL)
	{
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: MIList has not initialed!\n", __FUNCTION__));
		return;
	}

	OS_INT_LOCK(&MIList->Lock, IrqFlags);
	newEntry = GetEntryFromPool(MIList);
	if(!newEntry)
	{
		OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: newEntry is null!!!\n", __FUNCTION__));
		return;
	}
	if(newEntry->pNext)
		MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s:fix me, newEntry is in use\n",__FUNCTION__));
	newEntry->MemSize = Size;
	newEntry->pMemAddr = pMemAddr;
	newEntry->pCaller = pCaller;
	newEntry->pNext = MIList->pHead[Index];
	MIList->pHead[Index] = newEntry;
	MIList->EntryNumber++;
	MIList->CurAlcSize += newEntry->MemSize;
	if(MIList->CurAlcSize > MIList->MaxAlcSize)
		MIList->MaxAlcSize = MIList->CurAlcSize;
	OS_INT_UNLOCK(&MIList->Lock, IrqFlags);
}

static inline VOID ShowMIList(MEM_INFO_LIST *MIList)
{
	UINT32 i, total = 0;
	MEM_INFO_LIST_ENTRY *ptr;

	for(i = 0;i < HASH_SIZE;i++)
	{
		ptr = MIList->pHead[i];
		while(ptr)
		{
			if(ptr->MemSize == 0)
				MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_OFF,
					("%u: addr = %p, caller is %pS\n",++total ,ptr->pMemAddr, ptr->pCaller));
			else
				MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_OFF,
					("%u: addr = %p, size = %u, caller is %pS\n",++total ,ptr->pMemAddr, ptr->MemSize, ptr->pCaller));
			ptr = ptr->pNext;
		}
	}
	MTWF_LOG(DBG_CAT_INIT, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("the number of allocated memory = %u\n", MIList->EntryNumber));
}

#endif /* MEM_ALLOC_INFO_SUPPORT */
#endif

