/*======================================================================*/
/*		NuSYS							*/
/*		nugfxtaskmgr.c						*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*======================================================================*/
/* MG64 nugfxtaskmgr: a game-customized graphics task manager based on	*/
/* nusys-2.07. nuGfxTaskMgr adds a retrace-pacing wait (nuGfxRetraceWait,*/
/* scheduler frame counter D_80104E68 vs last-seen D_800D8980) and	*/
/* reorders the spool/callback. nuGfxTaskMgrInit uncomments the		*/
/* output_buff FIFO setup (nuRDPOutputBuf) and re-stores msgQ at the	*/
/* loop tail; nuGfxTaskStart swaps the frame buffer via the MG64	*/
/* sequence table D_800B67A4. The file-scope data lives in the extracted*/
/* asm (main_data / ADD30.bss), referenced here as externs (the		*/
/* nugfxinit.c drop-def pattern).					*/
/*======================================================================*/
#include <nusys.h>

/* file-scope statics / MG64 globals (defined in the extracted asm data) */
extern NUScTask		D_801B5D08[NU_GFX_TASK_NUM];	/* nuGfxTask	*/
extern NUScTask*	D_800D8984;	/* nuGfxTask_ptr		*/
extern NUScMsg		D_800D8988;	/* taskDoneMsg			*/
extern NUScMsg		D_800D898A;	/* swapBufMsg			*/
extern OSThread		D_800D8990;	/* GfxTaskMgrThread		*/
extern OSMesg		D_800DAB40[];	/* nuGfxTaskMgrMesgBuf (== stack top) */
extern OSMesgQueue	D_801B93F8;	/* nuGfxTaskMgrMesgQ		*/
extern u32		D_800D8980;	/* retrace pacing: last-seen counter */
extern u32		D_80104E68;	/* retrace pacing: scheduler frame counter */
extern u8		D_800B3F20[];	/* rspbootTextEnd		*/
extern u8		audio_sched_thread_entry[];	/* rspbootTextStart */
extern u8		D_800B67A4[];	/* MG64 frame-buffer swap sequence table */
extern u16		D_800B67BC;	/* beforeFlag			*/

/*----------------------------------------------------------------------*/
/*	nuGfxTaskMgr - Task Manager (MG64 retrace-paced variant)	*/
/*----------------------------------------------------------------------*/
void nuGfxTaskMgr(void *arg)
{
    NUScMsg*	mesg_type;
    NUScTask*	gfxTask;
    OSIntMask	mask;

    osCreateMesgQueue(&D_801B93F8, D_800DAB40, NU_GFX_MESGS);

    while(1){
	(void)osRecvMesg(&D_801B93F8, (OSMesg*)&gfxTask, OS_MESG_BLOCK);
	mesg_type = gfxTask->msg;

	switch(*mesg_type){
	case NU_SC_SWAPBUFFER_MSG:
	    while(1){
		mask = osSetIntMask(OS_IM_NONE);
		if((u32)(D_80104E68 - D_800D8980) >= 2){
		    break;
		}
		osSetIntMask(mask);
		nuGfxRetraceWait(1);
	    }
	    D_800D8980 = D_80104E68;
	    osSetIntMask(mask);

	    if(nuGfxSwapCfbFunc != NULL){
		(*nuGfxSwapCfbFunc)((void*)gfxTask);
	    }
	    mask = osSetIntMask(OS_IM_NONE);
	    nuGfxTaskSpool--;
	    osSetIntMask(mask);

	    if(nuGfxDisplay & NU_GFX_DISPLAY_ON_TRIGGER){
		osViBlack(FALSE);
		nuGfxDisplay = NU_GFX_DISPLAY_ON;
	    }
	    break;

	case NU_SC_GTASKEND_MSG:
	    mask = osSetIntMask(OS_IM_NONE);
	    nuGfxTaskSpool--;
	    osSetIntMask(mask);
	    if(nuGfxTaskEndFunc != NULL){
		(*nuGfxTaskEndFunc)((void*)gfxTask);
	    }
	    break;
	}
    }
}

/*----------------------------------------------------------------------*/
/*	nuGfxTaskMgrInit - Initializes the Task Manager			*/
/*----------------------------------------------------------------------*/
void nuGfxTaskMgrInit(void)
{
    u32	cnt;

    D_800D8988 = NU_SC_GTASKEND_MSG;
    D_800D898A = NU_SC_SWAPBUFFER_MSG;
    nuGfxTaskSpool = 0;
    nuGfxDisplayOff();

    osCreateThread(&D_800D8990, NU_GFX_TASKMGR_THREAD_ID, nuGfxTaskMgr,
		   (void*)NULL,
		   (D_800DAB40),
		   NU_GFX_TASKMGR_THREAD_PRI );
    osStartThread(&D_800D8990);

    for(cnt = 0; cnt < NU_GFX_TASK_NUM; cnt++){
	D_801B5D08[cnt].next			= &D_801B5D08[cnt+1];
	D_801B5D08[cnt].msgQ			= &D_801B93F8;
	D_801B5D08[cnt].list.t.type		= M_GFXTASK;
	D_801B5D08[cnt].list.t.flags		= 0x00;
	D_801B5D08[cnt].list.t.ucode_boot	= (u64*)audio_sched_thread_entry;
	D_801B5D08[cnt].list.t.ucode_boot_size 	= ((s32) D_800B3F20
						    - (s32) audio_sched_thread_entry);
	D_801B5D08[cnt].list.t.ucode_size 	= SP_UCODE_SIZE;
	D_801B5D08[cnt].list.t.ucode_data_size 	= SP_UCODE_DATA_SIZE;
	D_801B5D08[cnt].list.t.dram_stack	= (u64*) nuDramStack;
	D_801B5D08[cnt].list.t.dram_stack_size 	= SP_DRAM_STACK_SIZE8;
	D_801B5D08[cnt].list.t.output_buff	= (u64*)&nuRDPOutputBuf[0];
	D_801B5D08[cnt].list.t.output_buff_size = (u64*)&nuRDPOutputBuf[0x20000];
	D_801B5D08[cnt].list.t.yield_data_ptr	= (u64 *) nuYieldBuf;
	D_801B5D08[cnt].list.t.yield_data_size	= OS_YIELD_DATA_SIZE;
	D_801B5D08[cnt].msgQ			= &D_801B93F8;
    }
    D_801B5D08[NU_GFX_TASK_NUM-1].next = &D_801B5D08[0];
    D_800D8984	= &D_801B5D08[0];
}

/*----------------------------------------------------------------------*/
/*	nuGfxTaskStart - Starts the graphics task			*/
/*----------------------------------------------------------------------*/
void nuGfxTaskStart(Gfx *gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag)
{
    OSIntMask	mask;

    D_800D8984->list.t.data_ptr		= (u64*)gfxList_ptr;
    D_800D8984->list.t.data_size	= gfxListSize;
    D_800D8984->list.t.flags		= flag >> 16;
    D_800D8984->list.t.ucode 		= nuGfxUcode[ucode].ucode;
    D_800D8984->list.t.ucode_data	= nuGfxUcode[ucode].ucode_data;
    D_800D8984->flags			= flag & 0x0000ffff;
    D_800D8984->framebuffer		= (u16*)nuGfxCfb_ptr;

    if(D_800B67BC & NU_SC_UCODE_XBUS){
	D_800D8984->list.t.flags |= OS_TASK_DP_WAIT;
	D_800B67BC ^= NU_SC_UCODE_XBUS;
    }
    D_800B67BC = flag;

    if(flag & NU_SC_SWAPBUFFER){
	D_800D8984->msg         	= (OSMesg)&D_800D898A;
	nuGfxCfbCounter = D_800B67A4[nuGfxCfbCounter];
	nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
    } else {
	D_800D8984->msg         = (OSMesg)&D_800D8988;
    }

    mask = osSetIntMask(OS_IM_NONE);
    nuGfxTaskSpool++;
    osSetIntMask(mask);

    osWritebackDCacheAll();
    osSendMesg(&nusched.graphicsRequestMQ,
	       (OSMesg*)D_800D8984, OS_MESG_BLOCK);

    D_800D8984 = D_800D8984->next;
}
