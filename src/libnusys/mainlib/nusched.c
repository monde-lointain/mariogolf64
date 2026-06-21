/*======================================================================*/
/*		NuSYS							*/
/*		nusched.c						*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*======================================================================*/
/* MG64 nusched: a game-customized scheduler based on nusys-2.07 (minus  */
/* the nuVersion[] marker), compiled with NU_DEBUG. nuScExecuteAudio is	*/
/* the stock NU_DEBUG body; nuScCreateScheduler / nuScEventHandler /	*/
/* nuScExecuteGraphics carry MG64-specific edits and stay as INCLUDE_ASM	*/
/* until decompiled; the rest are stock-2.07 bodies.			*/
/*======================================================================*/
#define NU_DEBUG
#include <nusys.h>
#include "include_asm.h"

#define TASK_YIELD		1	/* GFX TASK YILED FLAG */
#define TASK_YIELDED		2	/* GFX TASK YILEDED FLAG */

#define VIDEO_MSG		666	/* Retrace message */
#define RSP_DONE_MSG		667	/* RSP task finished */
#define RDP_DONE_MSG		668	/* RDP rendering finished*/
#define PRE_NMI_MSG    	 	669	/* NMI message */

extern NUDebTaskPerf *debTaskPerfPtr;

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

void nuScExecuteAudio(void)
{

    NUScTask*	gfxTask;
    NUScTask*	audioTask;
    OSMesg 	msg;
    u32		yieldFlag;
#ifdef NU_DEBUG
    OSIntMask	mask;
#endif /* NU_DEBUG */

    while(1) {
	/* Wait for request for audio task execution. */
	osRecvMesg(&nusched.audioRequestMQ, (OSMesg *)&audioTask, OS_MESG_BLOCK);
	/* Do not create task after 0.5 sec - 2 frames. */
	/* Do notify of end of task, however. */
	if(nuScPreNMIFlag & NU_SC_BEFORE_RESET){
	    /* Notify the thread that started the audio task that the task has finished. */
	    osSendMesg(audioTask->msgQ, audioTask->msg, OS_MESG_BLOCK);
	    continue;
	}

	osWritebackDCacheAll();	/* Flash the cache. */

	/* Check current RSP status. */
	yieldFlag = 0;
	gfxTask = nusched.curGraphicsTask;

	/* If a graphics task is being executed, have it yield. */
	if( gfxTask ) {

	    /* Wait for completion (yield) of the graphics task. */
	    osSpTaskYield();		/* Task yield */
	    osRecvMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);

	    /* Check whether the task actually has yielded.*/
	    if (osSpTaskYielded(&gfxTask->list)){

		/* Yielded */
		yieldFlag = TASK_YIELD;
	    } else {

		/* Task finishes with yield. */
		yieldFlag = TASK_YIELDED;
	    }
	}
#ifdef NU_DEBUG
	mask = osSetIntMask(OS_IM_NONE);
	if(debTaskPerfPtr->auTaskCnt < NU_DEB_PERF_AUTASK_CNT){
	    debTaskPerfPtr->auTaskTime[debTaskPerfPtr->auTaskCnt].rspStart =
		OS_CYCLES_TO_USEC(osGetTime());
	}
	osSetIntMask(mask);
#endif /* NU_DEBUG */

	/* Execute audio task. */
	nusched.curAudioTask = audioTask;
	osSpTaskStart(&audioTask->list);

	/* Wait for end of the RSP task. */
	osRecvMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
	nusched.curAudioTask = (NUScTask *)NULL;

#ifdef NU_DEBUG
	mask = osSetIntMask(OS_IM_NONE);
	if(debTaskPerfPtr->auTaskCnt < NU_DEB_PERF_AUTASK_CNT){
	   debTaskPerfPtr->auTaskTime[debTaskPerfPtr->auTaskCnt].rspEnd =
	       OS_CYCLES_TO_USEC(osGetTime());
	   debTaskPerfPtr->auTaskCnt++;
	}
	osSetIntMask(mask);
#endif /* NU_DEBUG */

	/* Check whether a graphics task is waiting to be executed.	*/
	/* If so, send message.			*/
	if( nusched.graphicsTaskSuspended )
	    osSendMesg( &nusched.waitMQ, &msg, OS_MESG_BLOCK );

	/* Resume the yielding graphics task. */
	if( yieldFlag == TASK_YIELD ) {
	    osSpTaskStart(&gfxTask->list);
	} else if ( yieldFlag == TASK_YIELDED ) {
	    /* The graphics task finishes at the same time as the yield,	*/
	    /*	so the RSPTask completion message is the same as that for the yield.  */
	    /* Send the message to the graphics task thread. */
	    osSendMesg(&nusched.rspMQ, &msg, OS_MESG_BLOCK);
	}

	/* Notify the thread that started the audio task that the task has finished. */
	osSendMesg(audioTask->msgQ, audioTask->msg, OS_MESG_BLOCK);
    }
}

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
