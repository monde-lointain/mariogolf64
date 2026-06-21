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

/* MG64-specific scheduler state (BSS); volatiles per the dead-reload/recompute tells. */
extern vu32	D_801B68E0;	/* second retrace counter (++ w/ dead reload) */
extern vs32	D_800B678C;	/* perf-sample-armed flag */
extern vu32	D_8012F4D4;	/* swap-gate retrace base */
extern void	*D_800B6784;	/* current gfx task ptr (fb @ +0xc) */
extern s32	D_800B6788;	/* perf buffer index */
extern s8	D_800B67C0;	/* display-off guard */
extern u32	D_800B6790;	/* video TV-format code (MPAL=1 PAL=2 NTSC=3 other=4) */

/* Scheduler thread stacks: upstream file-statics relocated to main BSS (extern, placed). */
extern u64	nuScStack[NU_SC_STACK_SIZE / sizeof(u64)];
extern u64	nuScAudioStack[NU_SC_STACK_SIZE / sizeof(u64)];
extern u64	nuScGraphicsStack[NU_SC_STACK_SIZE / sizeof(u64)];

/* The scheduler TU treats the retrace counter as volatile (dead-reload-after-store on
   the ++, recompute-not-CSE of (counter - base)); nugfxtaskmgr/nucontrmbmgr keep the
   non-volatile nusys.h extern, so the volatility is local to this TU. */
extern volatile u32 nuScRetraceCounter;

/* Thread entry points referenced by nuScCreateScheduler (defined later / INCLUDE_ASM). */
void nuScEventHandler(void);
void nuScExecuteAudio(void);
void nuScExecuteGraphics(void);

void nuScCreateScheduler(u8 videoMode, u8 numFields)
{
    /* Initialize variables */
    nusched.retraceMsg            = NU_SC_RETRACE_MSG;
    nusched.prenmiMsg             = NU_SC_PRENMI_MSG;
    nusched.retraceCount          = numFields;
    nusched.curGraphicsTask       = NULL;
    nusched.curAudioTask          = NULL;
    nusched.graphicsTaskSuspended = NULL;
    nusched.clientList            = NULL;
    nusched.frameBufferNum        = 2;

    /* MG64: only NTSC/MPAL are supported; hang otherwise. */
    if(osTvType != OS_TV_NTSC && osTvType != OS_TV_MPAL){
	while(1)
	    ;
    }

    /* MG64: pick the video mode and TV-format code from the TV type. */
    if(osTvType == OS_TV_MPAL){
	videoMode = OS_VI_MPAL_LAN1;
    }
    switch(osTvType){
    case OS_TV_MPAL:
	D_800B6790 = 1;
	break;
    case OS_TV_PAL:
	D_800B6790 = 2;
	break;
    case OS_TV_NTSC:
	D_800B6790 = 3;
	break;
    default:
	D_800B6790 = 4;
	break;
    }

    /* Distinguish between NTSC and PAL and set frame rate accordingly. */
    if(osTvType == OS_TV_PAL){
	nusched.frameRate = 50;
    } else {
	nusched.frameRate = 60;
    }
    nuScPreNMIFlag = NU_SC_PRENMI_YET;

#ifdef NU_DEBUG
    debTaskPerfPtr = &nuDebTaskPerf[0];
    debTaskPerfPtr->retraceTime = 0;
    debTaskPerfPtr->auTaskCnt = 0;
    debTaskPerfPtr->gfxTaskCnt = 0;
    nuDebTaskPerfPtr = debTaskPerfPtr;
    osDpSetStatus(DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR | DPC_CLR_CMD_CTR | DPC_CLR_CLOCK_CTR);
#endif /* NU_DEBUG */

    /* Create the message queue. */
    osCreateMesgQueue(&nusched.retraceMQ, nusched.retraceMsgBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.rspMQ, nusched.rspMsgBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.rdpMQ, nusched.rdpMsgBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.graphicsRequestMQ, nusched.graphicsRequestBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.audioRequestMQ, nusched.audioRequestBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.waitMQ, nusched.waitMsgBuf, NU_SC_MAX_MESGS);

    /* Set the video mode. */
    osCreateViManager(OS_PRIORITY_VIMGR);
    osViSetMode(&osViModeTable[videoMode]);
    osViBlack(TRUE);

    /* Register the event handler. */
    osViSetEvent(&nusched.retraceMQ, (OSMesg)VIDEO_MSG, numFields);
    osSetEventMesg(OS_EVENT_SP,     &nusched.rspMQ,     (OSMesg)RSP_DONE_MSG);
    osSetEventMesg(OS_EVENT_DP,     &nusched.rdpMQ,     (OSMesg)RDP_DONE_MSG);
    osSetEventMesg(OS_EVENT_PRENMI, &nusched.retraceMQ, (OSMesg)PRE_NMI_MSG);

    /* Start the Scheduler thread. */
    osCreateThread(&nusched.schedulerThread, 19, (void (*)(void *))nuScEventHandler,
		   (void *)&nusched, nuScStack + NU_SC_STACK_SIZE / sizeof(u64), NU_SC_HANDLER_PRI);
    osStartThread(&nusched.schedulerThread);

    osCreateThread(&nusched.audioThread, 18, (void (*)(void *))nuScExecuteAudio,
		   (void *)&nusched, nuScAudioStack + NU_SC_STACK_SIZE / sizeof(u64), NU_SC_AUDIO_PRI);
    osStartThread(&nusched.audioThread);

    osCreateThread(&nusched.graphicsThread, 17, (void (*)(void *))nuScExecuteGraphics,
		   (void *)&nusched, nuScGraphicsStack + NU_SC_STACK_SIZE / sizeof(u64), NU_SC_GRAPHICS_PRI);
    osStartThread(&nusched.graphicsThread);
}

OSMesgQueue *nuScGetAudioMQ(void)
{
    return( &nusched.audioRequestMQ );
}

OSMesgQueue *nuScGetGfxMQ(void)
{
    return( &nusched.graphicsRequestMQ );
}

void nuScEventHandler(void)
{
    OSMesg	msg;
    s32		beforeResetFrame;

    nuScRetraceCounter = 0;
    D_801B68E0 = 0;
    while(1){
	osRecvMesg(&nusched.retraceMQ, &msg, OS_MESG_BLOCK);
	switch((int)msg){
	case VIDEO_MSG:
	    nuScRetraceCounter++;
	    D_801B68E0++;
	    if(D_800B678C != 0){
		debTaskPerfPtr->retraceTime = OS_CYCLES_TO_USEC(osGetTime());
		nuDebTaskPerf[D_800B6788].auTaskCnt = 0;
		D_800B678C = 0;
	    }
	    if((nuScRetraceCounter - D_8012F4D4) >= 0x1F){
		s32 frame = nuScRetraceCounter - D_8012F4D4;
		if(D_800B6784 != NULL){
		    if(frame == 0x20){
			osViSwapBuffer(*(void **)((u8 *)D_800B6784 + 0xc));
		    } else if(frame == 0x36 && D_800B67C0 == 0){
			nuGfxDisplayOff();
			D_800B6784 = NULL;
		    }
		}
	    }
	    nuScEventBroadcast(&nusched.retraceMsg);
	    if(nuScPreNMIFlag){
		if(beforeResetFrame){
		    beforeResetFrame--;
		} else {
		    nuScPreNMIFlag |= NU_SC_BEFORE_RESET;
		    osAfterPreNMI();
		    osViSetYScale(1.0);
		    osViBlack(TRUE);
		}
	    }
	    break;
	case PRE_NMI_MSG:
	    nuScPreNMIFlag = NU_SC_PRENMI_GET;
	    nuScEventBroadcast(&nusched.prenmiMsg);
	    if(nuScPreNMIFunc != NULL){
		(*nuScPreNMIFunc)();
	    }
	    beforeResetFrame = (nusched.frameRate / 2) / nusched.retraceCount - 3;
	    break;
	default:
	    break;
	}
    }
}

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
