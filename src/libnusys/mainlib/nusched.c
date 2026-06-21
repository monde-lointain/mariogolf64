/*======================================================================*/
/*		NuSYS							*/
/*		nusched.c						*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*======================================================================*/
/* MG64 nusched: a game-customized scheduler based on nusys-2.07 (minus  */
/* the nuVersion[] marker). nuScCreateScheduler / nuScEventHandler /	*/
/* nuScExecuteAudio / nuScExecuteGraphics carry MG64-specific edits and	*/
/* stay as INCLUDE_ASM until decompiled; the rest are stock-2.07 bodies.	*/
/*======================================================================*/
#include <nusys.h>
#include "include_asm.h"

#define VIDEO_MSG		666	/* Retrace message */
#define RSP_DONE_MSG		667	/* RSP task finished */
#define RDP_DONE_MSG		668	/* RDP rendering finished*/
#define PRE_NMI_MSG    	 	669	/* NMI message */

INCLUDE_ASM("asm/nonmatchings/libnusys/mainlib/nusched", nuScCreateScheduler);

OSMesgQueue *nuScGetAudioMQ(void)
{
    return( &nusched.audioRequestMQ );
}

OSMesgQueue *nuScGetGfxMQ(void)
{
    return( &nusched.graphicsRequestMQ );
}

INCLUDE_ASM("asm/nonmatchings/libnusys/mainlib/nusched", nuScEventHandler);

void nuScAddClient(NUScClient* client, OSMesgQueue* msgQ, NUScMsg msgType)
{
    OSIntMask	mask;

    mask = osSetIntMask(OS_IM_NONE);

    client->msgQ = msgQ;
    client->next = nusched.clientList;
    client->msgType = msgType;
    nusched.clientList = client;

    /* Dispatch a message if a PRENMI message has already been received. */
    if((msgType & NU_SC_PRENMI_MSG) && nuScPreNMIFlag){
	osSendMesg(msgQ, (OSMesg*)&nusched.prenmiMsg, OS_MESG_NOBLOCK);
    }

    osSetIntMask(mask);
}

void nuScResetClientMesgType(NUScClient* client, NUScMsg msgType)
{
    OSIntMask	mask;
    mask = osSetIntMask(OS_IM_NONE);
    client->msgType = msgType;

    osSetIntMask(mask);
}

void nuScRemoveClient(NUScClient *c)
{
    NUScClient*	client;
    NUScClient*	prev;
    OSIntMask	mask;

    mask = osSetIntMask(OS_IM_NONE);
    client = nusched.clientList;
    prev = 0;
    while(client != 0){
	if(client == c){
	    if(prev){
		prev->next = c->next;
	    } else {
		nusched.clientList = c->next;
	    }
	    break;
	}
	prev   = client;
	client = client->next;
    }
    osSetIntMask(mask);
}

void nuScEventBroadcast(NUScMsg *msg)
{
    NUScClient *client;

    /* Notify clients that require retrace messages. */
    for(client = nusched.clientList; client != 0; client = client->next){

	/* Check msgType for registered clients and notify. */
	if(client->msgType & *msg){
	    osSendMesg(client->msgQ, (OSMesg *)msg, OS_MESG_NOBLOCK);
	}
    }
}

INCLUDE_ASM("asm/nonmatchings/libnusys/mainlib/nusched", nuScExecuteAudio);

s32 func_80028D28(void)
{
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/libnusys/mainlib/nusched", nuScExecuteGraphics);

void nuScWaitTaskReady(NUScTask *task)
{
    NUScClient	client;
    void*	fb	= task->framebuffer;

    /* Wait unnecessary if the number of frame buffers is 1. */
    if(nusched.frameBufferNum == 1) return;

    nuScAddClient(&client, &nusched.waitMQ , NU_SC_RETRACE_MSG);

    /* Wait till next retrace if frame buffer is not free. */
    while( osViGetCurrentFramebuffer() == fb
	   || osViGetNextFramebuffer() == fb ){
	osRecvMesg( &nusched.waitMQ, NULL, OS_MESG_BLOCK );
    }

    nuScRemoveClient(&client );
}

void nuScSetFrameBufferNum(u8 frameBufferNum)
{
    nusched.frameBufferNum = frameBufferNum;
}

s32 nuScGetFrameRate(void)
{
    return nusched.frameRate;
}
