/*======================================================================*/
/*		NuSYS 							*/
/*		nusys.h							*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*									*/
/*----------------------------------------------------------------------*/
/* Ver 1.0	97/10/09	Created  by Kensaku Ohki(SLANP)		*/
/* Ver 1.2	98/07/12	Modified by Kensaku Ohki(SLANP)		*/
/* Ver 2.0	90/01/23	Modified by Kensaku Ohki(SLANP)		*/
/*----------------------------------------------------------------------*/
/* $Id: nusys.h,v 1.21 1999/01/26 02:33:26 ohki Exp $		*/
/*======================================================================*/
#ifndef _NUSYS_H_
#define _NUSYS_H_

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifndef F3DEX_GBI
#define	F3DEX_GBI
#endif	/* F3DEX_GBI */

#include <ultra64.h>
#include <PR/gs2dex.h>
#endif

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif

/*----------------------------------------------------------------------*/    
/*----------------------------------------------------------------------*/
/*	DEFINE								*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
#define NU_VERSION		"1.0"

/*--------------------------------------*/
/* NuSystem spec define			*/
/*--------------------------------------*/
#define NU_SPEC_BOOT_STACK_SIZE	0x2000
#define	NU_SPEC_BOOT_ADDR	0x80025c00
#define NU_SPEC_BOOT_STACK	nuMainStack + NU_SPEC_BOOT_STACK_SIZE

/*--------------------------------------*/
/* Thread ID for DEBUG（一応のため）	*/
/*--------------------------------------*/
#define NU_IDLE_THREAD_ID	1
#define NU_RMON_THERAD_ID	2	/* no use */
#define NU_MAIN_THREAD_ID	3

									
/*--------------------------------------*/
/* NuSYS Thread Priority		*/
/*--------------------------------------*/
#define NU_IDLE_THREAD_PRI	OS_PRIORITY_IDLE	/* Idle thread	*/
#define NU_RMON_THREAD_PRI	OS_PRIORITY_RMON	/* no use	*/
#define NU_MAIN_THREAD_PRI	10			/* main thread	*/
    
/*--------------------------------------*/
/* NUSYS STACK SIZE			*/
/*--------------------------------------*/
#define NU_IDLE_STACK_SIZE	0x2000			/* Idle thread */
#define NU_MAIN_STACK_SIZE	NU_SPEC_BOOT_STACK_SIZE	/* Main thread */




/*----------------------------------------------------------------------*/
/* SCHEDULER DEFINE							*/
/*----------------------------------------------------------------------*/
#define NU_SC_STACK_SIZE	0x2000		/* スレッドスタックサイズ */
#define NU_SC_NOSWAPBUFFER	0x0000		/* 切り替えなし		*/
#define NU_SC_SWAPBUFFER	0x0001		/* フレームバッファの切り替え*/
#define NU_SC_NORDP		0x0002		/* RDP終了待ちしない	*/
#define	NU_SC_UCODE_XBUS	0x0004		/* XBUS Ucode		*/
#define	NU_SC_TASK_YIELDED	(OS_TASK_YIELEDE<<16)
#define	NU_SC_TASK_DP_WAIT	(OS_TASK_DP_WAIT<<16)	/* RDP WAIT	*/
#define	NU_SC_TASK_LODABLE	(OS_TASK_LOADBLE<<16)	/* LOADABLE	*/     
#define	NU_SC_TASK_SP_ONLY	(OS_TASK_SP_ONLY<<16)	/* SP ONLY	*/

							   
#define NU_SC_RETRACE_MSG	0x0001		/* リトレースメッセージ */
#define NU_SC_PRENMI_MSG	0x0002		/* ＮＭＩメッセージ */
#define NU_SC_SWAPBUFFER_MSG	0x0004		/* フレームバッファ切り替え */
						/* メッセージ*/ 
#define NU_SC_GTASKEND_MSG	0x0008		/* タスク終了メッセージ */
#define NU_SC_MAX_MESGS		8		/* メッセージバッファサイズ */

#define NU_SC_HANDLER_PRI	120	/* EVENT HANDLER THREAD PRORITY */
#define NU_SC_AUDIO_PRI		110	/* AUDIO DISPATCHER THREAD PRORITY */
#define NU_SC_GRAPHICS_PRI	100	/* GFX DISPATCHER THREAD PRORITY */
#define	NU_SC_PRENMI_YET	0	/* PRE NMIが発生していない*/
#define	NU_SC_PRENMI_GET	1	/* PRE NMIを受けたフラグ */
#define	NU_SC_BEFORE_RESET 	2	/* RESET前フレーム前のフラグ */
#define	NU_SC_BEFORE_RESET_FRAME 2	/* BEFORE_RESETフラグを立てるRESET前の
					   フレーム数*/
/*----------------------------------------------------------------------*/
/* GRAFIC THREAD DEFINE							*/
/*----------------------------------------------------------------------*/
#define NU_GFX_THREAD_ID	4	
#define NU_GFX_TASKMGR_THREAD_ID 5
#define NU_GFX_STACK_SIZE	0x2000		/* スレッドスタックサイズ */
#define	NU_GFX_MESGS		8		/* GFXメッセージキュー*/
#define NU_GFX_TASKMGR_STACK_SIZE 0x2000	/* スタックサイズ */
#define	NU_GFX_TASKMGR_MESGS	8		/*タスクマネージャのキュー*/
#define NU_GFX_THREAD_PRI	50	/* GFXスレッドのプライオリティ	*/
#define NU_GFX_TASKMGR_THREAD_PRI 60	/* タスクマネージャのプライオリティ */

/*----------------------------------------------------------------------*/
/* GRAFIC MANEGER DEFINE						*/
/*----------------------------------------------------------------------*/
#ifdef F3DEX_GBI_2
#define	NU_GFX_UCODE_F3DEX2	0		/* F3DEXマイクロコード  */
#define	NU_GFX_UCODE_F3DEX2_NON	1		/* F3DEX.NoNマイクロコード  */
#define	NU_GFX_UCODE_F3DEX2_REJ	2		/* F3DEX.NoNマイクロコード  */
#define	NU_GFX_UCODE_F3DLX2_REJ	3		/* F3DEX.ReJマイクロコード  */
#define	NU_GFX_UCODE_L3DEX2	4		/* L3DEXマイクロコード  */
#define	NU_GFX_UCODE_S2DEX2	5		/* S2DEXマイクロコード  */
    
#define	NU_GFX_UCODE_F3DEX	0		/* 互換性のため		*/
#define	NU_GFX_UCODE_F3DEX_NON	1		/* 互換性のため		*/
#define	NU_GFX_UCODE_F3DLX	0		/* 互換性のため		*/
#define	NU_GFX_UCODE_F3DLX_NON	1		/* 互換性のため		*/
#define	NU_GFX_UCODE_F3DLX_REJ	4		/* 互換性のため		*/
#define	NU_GFX_UCODE_F3DLP_REJ	3		/* 互換性のため		*/
#define	NU_GFX_UCODE_L3DEX	4		/* 互換性のため		*/
#define	NU_GFX_UCODE_S2DEX	5		/* 互換性のため		*/
#define	NU_GFX_UCODE_S2DEX_D	5		/* 互換性のため		*/

#else    
#define	NU_GFX_UCODE_F3DEX	0		/* F3DEXマイクロコード  */
#define	NU_GFX_UCODE_F3DEX_NON	1		/* F3DEX.NoNマイクロコード  */
#define	NU_GFX_UCODE_F3DLX	2		/* F3DLXマイクロコード  */
#define	NU_GFX_UCODE_F3DLX_NON	3		/* F3DLX.NoNマイクロコード  */
#define	NU_GFX_UCODE_F3DLX_REJ	4		/* F3DEX.ReJマイクロコード  */
#define	NU_GFX_UCODE_F3DLP_REJ	5		/* F3DLP.Rejマイクロコード  */
#define	NU_GFX_UCODE_L3DEX	6		/* L3DEXマイクロコード  */
#define	NU_GFX_UCODE_S2DEX	7		/* S2DEXマイクロコード  */
#define	NU_GFX_UCODE_S2DEX_D	8		/* S2DEXマイクロコード  */
#endif /* F3DEX_GBI_2 */

    
/*--------------------------------------*/
/* グラフィックタスク構造体の数は	*/
/* スケジューラのメッセージバッファサイズ*/
/* 以上にしてください。			*/
/* そうしないと,使用中のタスク構造体を  */
/* 使う可能性があります			*/
/*--------------------------------------*/
#define NU_GFX_TASK_NUM		10	/* グラフィックタスク構造体数 	*/
#define	NU_GFX_RDP_OUTPUTBUFF_SIZE	0x20000	/* fifoバッファサイズ	*/

/*--------------------------------------*/
/* デフォルトのフレームバッファ		*/
/* デフォルトではフレームバッファは	*/
/* RDRAMの最上位に配置			*/
/* 320x240 16bitトリプルバッファに設定	*/
/*--------------------------------------*/
#define NU_GFX_FRAMEBUFFER_NUM		3
#define NU_GFX_FRAMEBUFFER_ADDR		(0x80400000-320*240*2*3)
#define NU_GFX_FRAMEBUFFER0_ADDR	(NU_GFX_FRAMEBUFFER_ADDR)
#define NU_GFX_FRAMEBUFFER1_ADDR	(NU_GFX_FRAMEBUFFER0_ADDR+320*240*2)
#define NU_GFX_FRAMEBUFFER2_ADDR	(NU_GFX_FRAMEBUFFER1_ADDR+320*240*2)

#define NU_GFX_INIT_SCREEN_WD		320
#define NU_GFX_INIT_SCREEN_HT		240

/*--------------------------------------*/
/* デフォルトではZバッファは		*/
/*   メモリの最下位に配置		*/
/*--------------------------------------*/    
#define NU_GFX_ZBUFFER_ADDR		0x80000400
					   
#define	NU_GFX_DISPLAY_OFF		0	/* ディスプレイの非表示 */    
#define	NU_GFX_DISPLAY_ON		1	/* ディスプレイの表示	*/
#define	NU_GFX_DISPLAY_ON_TRIGGER	0x80	/* トリガー		*/

					   
/*----------------------------------------------------------------------*/
/* SI MANAGER DEFINE							*/
/*----------------------------------------------------------------------*/
#define	NU_SI_STACK_SIZE		NU_CONT_STACK_SIZE
#define	NU_SI_THREAD_ID			NU_CONT_THREAD_ID
#define NU_SI_THREAD_PRI		NU_CONT_THREAD_PRI
#define NU_SI_MESG_MAX			NU_CONT_MESG_MAX
#define NU_SI_CALLBACK_END		1
#define NU_SI_CALLBACK_CONTINUE		0
#define	NU_SI_SC_MAJOR_NO		0x0000
#define	NU_SI_MSG_MAJOR_NO_MASK		0xff00
#define	NU_SI_MSG_MINOR_NO_MASK		0x00ff

#define	NU_SI_MAJOR_NO_CONT		0x0100
#define	NU_SI_MAJOR_NO_PAK		0x0200
#define	NU_SI_MAJOR_NO_RMB		0x0300
#define	NU_SI_MAJOR_NO_EEPROM		0x0400
#define	NU_SI_MAJOR_NO_GBPAK		0x0500
#define	NU_SI_MAJOR_NO_VRS		0x0600
#define	NU_SI_STOP_MGR_MSG		0x7F00

/*----------------------------------------------------------------------*/
/* CONTROLLER MANEGER DEFINE						*/
/*----------------------------------------------------------------------*/
#define NU_CONT_MAXCONTROLLERS		MAXCONTROLLERS
#define	NU_CONT_STACK_SIZE		0x2000
#define	NU_CONT_MESG_MAX		8
#define NU_CONT_THREAD_ID		5
#define NU_CONT_THREAD_PRI		115
#define	NU_CONT_DATA_UNLOCK		0
#define	NU_CONT_DATA_LOCK		1


/*----------------------------------------------------------------------*/
/* コントローラマネージャへのメッセージ定義 				*/
/*----------------------------------------------------------------------*/
#define	NU_CONT_MSG_BASE		NU_SI_MAJOR_NO_CONT
#define	NU_CONT_RETRACE_MSG		(NU_CONT_MSG_BASE+0)
#define NU_CONT_READ_MSG		(NU_CONT_MSG_BASE+1)
#define NU_CONT_READ_NW_MSG		(NU_CONT_MSG_BASE+2)
#define NU_CONT_QUERY_MSG		(NU_CONT_MSG_BASE+3)


#define	NU_CONT_PAK_MSG_BASE		NU_SI_MAJOR_NO_PAK
#define	NU_CONT_PAK_RETRACE_MSG		(NU_CONT_PAK_MSG_BASE+0)
#define NU_CONT_PAK_OPEN_MSG		(NU_CONT_PAK_MSG_BASE+1)
#define NU_CONT_PAK_FREE_MSG		(NU_CONT_PAK_MSG_BASE+2)
#define NU_CONT_PAK_FOPEN_MSG		(NU_CONT_PAK_MSG_BASE+3)
#define NU_CONT_PAK_READWRITE_MSG	(NU_CONT_PAK_MSG_BASE+4)
#define NU_CONT_PAK_FILEDELETE_MSG	(NU_CONT_PAK_MSG_BASE+5)
#define NU_CONT_PAK_FILESTATE_MSG	(NU_CONT_PAK_MSG_BASE+6)
#define NU_CONT_PAK_FILENUM_MSG		(NU_CONT_PAK_MSG_BASE+7)
#define NU_CONT_PAK_REPAIRID_MSG	(NU_CONT_PAK_MSG_BASE+8)

#define NU_CONT_PAK_MODE_NOCREATE	0
#define NU_CONT_PAK_MODE_CREATE		1
#define	NU_CONT_PAK_TYPE_NONE		0
#define	NU_CONT_PAK_TYPE_PAK		1	/* コントローラパック	*/
#define	NU_CONT_PAK_TYPE_RUMBLE		2	/* 振動パック 		*/
#define	NU_CONT_PAK_TYPE_GBPAK		3	/* 64GBパック		*/
#define NU_CONT_PAK_READ		PFS_READ
#define	NU_CONT_PAK_WRITE		PFS_WRITE

/*----------------------------------------------------------------------*/
/* 振動パック制御 							*/
/*----------------------------------------------------------------------*/
#define NU_CONT_RMB_STATE_STOP		0x00
#define NU_CONT_RMB_STATE_STOPPING	0x01
#define NU_CONT_RMB_STATE_STOPPED	0x02
#define NU_CONT_RMB_STATE_RUN		0x03
#define NU_CONT_RMB_STATE_FORCESTOP	0x04
#define NU_CONT_RMB_MODE_DISABLE	0x00
#define	NU_CONT_RMB_MODE_ENABLE		0x01
#define NU_CONT_RMB_MODE_AUTORUN	0x02
#define NU_CONT_RMB_MODE_PAUSE		0x80
#define NU_CONT_RMB_AUTO_SEARCH		0x00
#define	NU_CONT_RMB_AUTO_FIND		0x01
#define NU_CONT_RMB_AUTO_SEARCHTIME	(60*5)
#define NU_CONT_RMB_MSG_BASE		NU_SI_MAJOR_NO_RMB
#define NU_CONT_RMB_RETRACE_MSG		(NU_CONT_RMB_MSG_BASE+0)
#define NU_CONT_RMB_CHECK_MSG		(NU_CONT_RMB_MSG_BASE+1)
#define NU_CONT_RMB_START_MSG		(NU_CONT_RMB_MSG_BASE+2)
#define NU_CONT_RMB_STOP_MSG		(NU_CONT_RMB_MSG_BASE+3)	   
#define	NU_CONT_RMB_FORCESTOP_MSG	(NU_CONT_RMB_MSG_BASE+4)
#define	NU_CONT_RMB_FORCESTOPEND_MSG	(NU_CONT_RMB_MSG_BASE+5)

/*----------------------------------------------------------------------*/
/* EEPROM								*/
/*----------------------------------------------------------------------*/
#define NU_EEPROM_MSG_BASE		NU_SI_MAJOR_NO_EEPROM
#define NU_EEPROM_RETRACE_MSG		(NU_EEPROM_MSG_BASE+0)
#define NU_EEPROM_CHECK_MSG		(NU_EEPROM_MSG_BASE+1)
#define	NU_EEPROM_READ_MSG		(NU_EEPROM_MSG_BASE+2)
#define NU_EEPROM_WRITE_MSG		(NU_EEPROM_MSG_BASE+3)


/*----------------------------------------------------------------------*/
/* 64GB Pack Manager							*/
/*----------------------------------------------------------------------*/
#define NU_CONT_GBPAK_MSG_BASE		NU_SI_MAJOR_NO_GBPAK
#define NU_CONT_GBPAK_RETRACE_MSG	(NU_CONT_GBPAK_MSG_BASE+0)
#define NU_CONT_GBPAK_OPEN_MSG		(NU_CONT_GBPAK_MSG_BASE+1)
#define NU_CONT_GBPAK_STATUS_MSG	(NU_CONT_GBPAK_MSG_BASE+2)
#define NU_CONT_GBPAK_POWER_MSG		(NU_CONT_GBPAK_MSG_BASE+3)
#define NU_CONT_GBPAK_READID_MSG	(NU_CONT_GBPAK_MSG_BASE+4)
#define NU_CONT_GBPAK_READWRITE_MSG	(NU_CONT_GBPAK_MSG_BASE+5)
#define NU_CONT_GBPAK_CHECKCONNECTOR_MSG (NU_CONT_GBPAK_MSG_BASE+6)
    
#define NU_CONT_GBPAK_POWER_OFF		OS_GBPAK_POWER_OFF
#define NU_CONT_GBPAK_POWER_ON		OS_GBPAK_POWER_ON
#define	NU_CONT_GBPAK_MBC_RAM_ENABLE_CODE	0x0a
#define	NU_CONT_GBPAK_MBC_RAM_DISABLE_CODE	0x00
#define	NU_CONT_GBPAK_MBC_RAM_REG0_ADDR		0x0000

#define NU_CONT_GBPAK_MBC_REG0_ADDR		0x0000	/* Register 0	*/
#define NU_CONT_GBPAK_MBC_REG1_ADDR		0x2100	/* Register 1	*/
#define NU_CONT_GBPAK_MBC_REG2_ADDR		0x4000	/* Register 2	*/
#define NU_CONT_GBPAK_MBC_REG3_ADDR		0x6000	/* Register 3	*/

/*----------------------------------------------------------------------*/
/* 音声認識システム Manager						*/
/*----------------------------------------------------------------------*/
#define NU_VRS_MSG_BASE			NU_SI_MAJOR_NO_VRS
#define	NU_VRS_RETRACE_MSG		(NU_VRS_MSG_BASE+0)
#define	NU_VRS_OPEN_MSG			(NU_VRS_MSG_BASE+1)
#define	NU_VRS_CLEAR_DICTIONARY_MSG	(NU_VRS_MSG_BASE+2)
#define	NU_VRS_SET_WORD_MSG		(NU_VRS_MSG_BASE+3)
#define	NU_VRS_MASK_DICTIONARY_MSG	(NU_VRS_MSG_BASE+4)
#define	NU_VRS_START_READ_DATA_MSG	(NU_VRS_MSG_BASE+5)
#define	NU_VRS_GET_READ_DATA_MSG	(NU_VRS_MSG_BASE+6)
#define	NU_VRS_STOP_READ_DATA_MSG	(NU_VRS_MSG_BASE+7)
#define	NU_VRS_CONTROL_GAIN_MSG		(NU_VRS_MSG_BASE+8)
					   
/*----------------------------------------------------------------------*/
/* PI DEFINE								*/
/*----------------------------------------------------------------------*/
#define	NU_PI_MESG_NUM			50	/* PI Message buffer size */
#define	NU_PI_CART_BLOCK_READ_SIZE	0x4000	/* cart read block size	*/

/*----------------------------------------------------------------------*/
/* DEBUG 								*/
/*----------------------------------------------------------------------*/
#define	NU_DEB_PERF_GFXTASK_CNT		8	/* グラフィックタスク計測数 */
#define	NU_DEB_PERF_AUTASK_CNT		4	/* オーディオタスク計測数 */
#define NU_DEB_DP_CLOCK_CTR		0	/* RDP内部カウンタ	*/
#define	NU_DEB_DP_CMD_CTR		1	/* CMDカウンタ		*/
#define NU_DEB_DP_PIPE_CTR		2	/* PIPEカウンタ		*/
#define	NU_DEB_DP_TMEM_CTR		3	/* TMEMカウンタ		*/
#define NU_DEB_BAR_FRAME1		0	/* パフォーマンスバーの */
#define NU_DEB_BAR_FRAME2		1	/* 表示最大数		*/
#define NU_DEB_BAR_FRAME3		2
#define NU_DEB_BAR_FRAME4		3
#define NU_DEB_BAR_FRAME5		4
#define NU_DEB_BAR_FRAME6		5
#define NU_DEB_BAR_FRAME7		6
#define	NU_DEB_CON_ROW_MAX		40	/* コンソールの列数 */
#define NU_DEB_CON_COLUMN_MAX		30 	/* コンソールの行数 */
#define NU_DEB_CON_TEXT_SIZE		(NU_DEB_CON_ROW_MAX*NU_DEB_CON_COLUMN_MAX)
#define NU_DEB_CON_WIDTH		320
#define NU_DEB_CON_WINDOW_NUM		4
#define NU_DEB_CON_WINDOW0		0
#define NU_DEB_CON_WINDOW1		1
#define NU_DEB_CON_WINDOW2		2
#define NU_DEB_CON_WINDOW3		3
#define	NU_DEB_CON_TEXT_BLACK		0	/* 黒 */
#define NU_DEB_CON_TEXT_BLUE		1	/* 青 */
#define NU_DEB_CON_TEXT_GREEN		2	/* 緑 */
#define NU_DEB_CON_TEXT_CYAN		3	/* シアン */
#define NU_DEB_CON_TEXT_RED		4	/* 赤 */
#define NU_DEB_CON_TEXT_MAGENTA		5	/* マゼンタ */
#define NU_DEB_CON_TEXT_YELLOW		6	/* 黄 */
#define NU_DEB_CON_TEXT_WHITE		7	/* 白 */
#define	NU_DEB_CON_TEXT_LIGHTBLACK	8	/* 灰 */
#define NU_DEB_CON_TEXT_LIGHTBLUE	9	/* 明るい青 */
#define NU_DEB_CON_TEXT_LIGTHBLUE	9	/* 明るい青 for spell miss*/
#define NU_DEB_CON_TEXT_LIGHTGREEN	10	/* 明るい緑 */
#define NU_DEB_CON_TEXT_LIGHTCYAN	11	/* 明るいシアン */
#define NU_DEB_CON_TEXT_LIGHTRED	12	/* 明るい赤 */
#define NU_DEB_CON_TEXT_LIGHTMAGENTA	13	/* 明るいマゼンタ */
#define NU_DEB_CON_TEXT_LIGHTYELLOW	14	/* 明るい黄 */
#define NU_DEB_CON_TEXT_LIGHTWHITE	15	/* 白 */
#define NU_DEB_CON_ATTR_NORMAL		0x0	/* アトリビュート無し */
#define NU_DEB_CON_ATTR_BLINK		0x1	/* 点滅		*/
#define	NU_DEB_CON_ATTR_REVERSE		0x2	/* 反転文字	*/
#define NU_DEB_CON_SCROLL_ON		1
#define NU_DEB_CON_SCROLL_OFF		0
#define NU_DEB_CON_WINDOW_OFF		0
#define NU_DEB_CON_WINDOW_ON		1    
    
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/* structer define 							*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/*--------------------------------------*/
/* Scheduler sturcter			*/
/*--------------------------------------*/
typedef short	NUScMsg;		/* スケジューラメッセージタイプ */

typedef struct	st_SCClient {		/* クライアントリスト構造体 */
    struct st_SCClient*	next;		/* 次のクライアントへのポインタ */
    OSMesgQueue*	msgQ;		/* クライアントに送信するメッセージ */
    NUScMsg		msgType;	/* メッセージの種類		*/
					/* NU_SC_RETRACE_MSG または	*/
    					/* NU_SC_PRENMI_MSG		*/
} NUScClient;

typedef struct st_SCTask {		/* タスク構造体 */
    struct st_SCTask	*next;
    u32		state;
    u32		flags;
    void	*framebuffer;		/* グラフィックスタスク用 */
    OSTask	list;
    OSMesgQueue	*msgQ;
    OSMesg	msg;
} NUScTask;

typedef struct st_Sched { /* スケジューラ構造体の定義 */

    /* メッセージ */
    NUScMsg	retraceMsg;
    NUScMsg	prenmiMsg;

    /* タスクリクエストキューの定義 */
    OSMesgQueue	audioRequestMQ;
    OSMesg	audioRequestBuf[NU_SC_MAX_MESGS];
    OSMesgQueue graphicsRequestMQ;
    OSMesg	graphicsRequestBuf[NU_SC_MAX_MESGS];

    /* メッセージキューの定義 */
    OSMesgQueue	retraceMQ;
    OSMesg	retraceMsgBuf[NU_SC_MAX_MESGS];
    OSMesgQueue	rspMQ;
    OSMesg      rspMsgBuf[NU_SC_MAX_MESGS];
    OSMesgQueue rdpMQ;
    OSMesg	rdpMsgBuf[NU_SC_MAX_MESGS];

    /* 次のリトレース信号を待つために使用 */
    OSMesgQueue	waitMQ;
    OSMesg	waitMsgBuf[NU_SC_MAX_MESGS];

    /* スレッドの定義 */
    OSThread	schedulerThread;	/* メインスレッド */
    OSThread	audioThread;		/* オーディオ */
    OSThread	graphicsThread;		/* グラフィックス */

    /* クライアントリスト  */
    NUScClient	*clientList;

    /* 実行中のグラフィックスタスク  */
    NUScTask	*curGraphicsTask;
    NUScTask 	*curAudioTask;
    NUScTask	*graphicsTaskSuspended;

    u32		retraceCount;
    u8		frameRate;
    u8		frameBufferNum;		/* フレームバッファ数 */


} NUSched;


/*--------------------------------------*/
/* ucode sturcter			*/
/*--------------------------------------*/
typedef struct  st_Ucode {
    u64*	ucode;	
    u64*	ucode_data;
} NUUcode;

/*--------------------------------------*/
/* CALL BACK Function	typedef		*/
/*--------------------------------------*/
typedef void (*NUIdleFunc)(void);	/* アイドルコールバック関数ポインタ*/
typedef void (*NUScPreNMIFunc)(void);	/* PRENMIコールバック関数ポインタ */
typedef void (*NUGfxFunc)(u32);		/* GFX リトレースコールバック関数ポインタ */
typedef void (*NUGfxPreNMIFunc)(void);	/* GFX PRENMIコールバック関数ポインタ */

typedef void (*NUGfxSwapCfbFunc)(void*);/* SWAPBUF コールバック関数ポインタ*/
typedef void (*NUGfxTaskEndFunc)(void*);/* TASKEND コールバック関数ポインタ*/
typedef void (*NUContReadFunc)(u32);	/* コントローラ読み込み終了の	*/
					/* コールバック関数ポインタ	*/
typedef void (*NUContPakFunc)(void*);	/* コントローラパック制御関数コールバック */
typedef void (*NUContRmbFunc)(void*);	/* 振動パック制御関数コールバック */
typedef s32 (*NUCallBackFunc)(void*);	/* コールバック関数 */


/*--------------------------------------*/
/* SI Common Message 			*/
/*--------------------------------------*/
typedef struct st_SiCommonMesg {
    NUScMsg		mesg;
    OSMesgQueue*	rtnMesgQ;
    s32			error;
    void*		dataPtr;
} NUSiCommonMesg;

/*--------------------------------------*/
/* controller typedef			*/
/*--------------------------------------*/
typedef struct st_ContRead {
    NUScMsg	mesg;			/* メッセージの種類 */
    OSMesgQueue* rtnMesgQ;
    OSContPad*	pad;
} NUContReadMesg;

typedef struct st_ContData {
    u16	button;			/* A,B,C,Z,L,R,START,PAD ,button 	*/
    s8	stick_x;		/* 実使用範囲 -61 <= stick X <= 61	*/
    s8	stick_y;		/* 実使用範囲 -63 <= stick Y <= 63	*/
    u8	errno;			/* エラー				*/
    u16	trigger;		/* button のトリガーデータ		*/
} NUContData;

typedef struct st_ContQuery {
    NUScMsg	mesg;
    OSMesgQueue* rtnMesgQ;
} NUContQueryMesg;


typedef struct st_ContPakFile {
    OSPfs	*pfs;    
    s32		file_no;
    s32		error;
    u32		type;
} NUContPakFile;


typedef struct st_ContPakFreeMesg {
    NUContPakFile*	file;
    s32			size;
} NUContPakFreeMesg;

typedef struct st_ContPakFileOpenMesg {
    NUContPakFile*	file;
    u8*			noteName;
    u8*			extName;
    s32			size;
    s32			mode;
} NUContPakFileOpenMesg;

typedef struct st_ContPakFileRWMesg {
    NUContPakFile*	file;
    s32			offset;
    s32			size;
    u8			mode;
    u8*			buf;
} NUContPakFileRWMesg;

typedef struct st_ContPakFileStateMesg {
    NUContPakFile*	file;
    OSPfsState*		state;
} NUContPakFileStateMesg;

typedef struct st_ContPakFileNumMesg {
    NUContPakFile*	file;
    s32*		max_files;
    s32*		used_files;
} NUContPakFileNumMesg;

typedef struct st_ContPakIsPlugMesg {
    NUScMsg		mesg;
    OSMesgQueue*	rtnMesgQ;
    s32			error;
    u8			bitPattern;
} NUContPakIsPlugMesg;

typedef struct st_ContRmbCtl {
    u16			freq;		/* 振動周波数 */
    u16			frame;
    u16			counter;
    u8			state;
    u8			mode;
    u8			autorun;
    u8			type;
} NUContRmbCtl;

typedef struct st_ContRmbMesg {
    u8			contNo;
    s32			error;
    void*		data;
} NUContRmbMesg;

typedef struct st_CallBackList {
    struct st_CallBackList* 	next;
    s32 			(**func)(NUSiCommonMesg*);
    u16				majorNo;
    u8    			funcNum;
} NUCallBackList;

typedef struct st_EepromMesg {
    void*		dataPtr;
    s32			error;
    u8			address;
    u16			dataLen;
} NUEepromMesg;

typedef struct st_ContGBPakMesg {
    NUContPakFile*	handle;
    s32			data[4];
} NUContGBPakMesg;

typedef OSVoiceHandle NUVrsHandle;

typedef OSVoiceData NUVrsData;

typedef struct stVrsMesg {
    NUVrsHandle*	vrsHandle;
    s32			data[2];
} NUVrsMesg;

/*--------------------------------------*/
/* Debug				*/
/*--------------------------------------*/
typedef struct st_GfxTaskTime {
    s64		rspStart;		/* RSPタスクスタート時間*/
    s64		rspEnd;			/* RSPタスク終了時間	*/
    s64		rdpEnd;			/* RDP終了時間		*/
    u32		dpCnt[4];		/* RDP内部カウンタ	*/
} NUDebTaskTime;

typedef struct st_DebTaskPerf {
    s64		retraceTime;		/* リトレースイベントタイム */
    u8		gfxTaskCnt;		/* グラフィックタスク数 */
    u8		auTaskCnt;		/* オーディオタスク数 */
    u8		gfxTaskStart;
    NUDebTaskTime	gfxTaskTime[NU_DEB_PERF_GFXTASK_CNT];
    NUDebTaskTime	auTaskTime[NU_DEB_PERF_AUTASK_CNT];
} NUDebTaskPerf;

/* コンソールウインドウ構造体 */
typedef struct st_DebConWindow {
    u8	windowFlag;	/* コンソールウインドウ表示/非表示フラグ*/
    u16	scroll;		/* スクロール値 			*/
    u8	scrollFlag;	/* スクロール許可/不許可 		*/
    u8	textColor;	/* 文字色 				*/
    u8	attribute;	/* アトリビュート			*/
    u16	posX;		/* 文字書き込み桁位置			*/
    u16	posY;		/* 文字書き込み行位置			*/
    u16	index;		/* 文字バッファ書き込み位置 		*/
    u16	winX;		/* コンソールウインドウの表示X座標 	*/
    u16	winY;		/* コンソールウインドウの表示Y座標 	*/
    u16	winW;		/* コンソール表示桁数 			*/
    u16	winH;		/* コンソール表示行数 			*/
    u16	text[NU_DEB_CON_TEXT_SIZE];	/*文字バッファ*/    
} NUDebConWindow;

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/* extern variables 							*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/    

extern u8	nuRDPOutputBuf[NU_GFX_RDP_OUTPUTBUFF_SIZE];
extern u8	nuDramStack[SP_DRAM_STACK_SIZE8];
extern u8	nuYieldBuf[OS_YIELD_DATA_SIZE];
extern NUSched	nusched;		/* スケジューラ構造体 */
extern OSMesgQueue nuGfxMesgQ;		/* graphic thread  queue */
extern u32	nuScRetraceCounter;    /* リトレースカウンター */
extern u8	nuScPreNMIFlag;
/*--------------------------------------*/
/*  graphics variables 			*/
/*--------------------------------------*/
extern NUUcode*		nuGfxUcode;	/* GFXマイクロコード構造体のポインタ*/
extern u16**		nuGfxCfb;/* フレームバッファのポインタ配列のポインタ */
extern u16*		nuGfxCfb_ptr;	/* フレームバッファのポインタ */
extern u32		nuGfxCfbNum;		/* フレームバッファ数 */
extern u16*		nuGfxZBuffer;		/* Zバッファのポインタ*/
extern volatile u32	nuGfxTaskSpool;		/* キューにあるタスク数 */
extern u32		nuGfxDisplay;		/* 画面の表示/非表示フラグ */
extern u32		nuGfxCfbCounter;	/* フレームバッファ切り替え用*/

/*--------------------------------------*/
/*  controller manager variables 	*/
/*--------------------------------------*/
extern OSContStatus	nuContStatus[NU_CONT_MAXCONTROLLERS];
extern OSContPad	nuContData[NU_CONT_MAXCONTROLLERS];
extern u32		nuContNum;	/* 接続されているコントローラの数 */
extern u32		nuContDataLockKey; /* コントローラデータのロック */
extern OSMesgQueue	nuContWaitMesgQ;/* コントローラリード待ち */
extern OSPfs		nuContPfs[4];
extern NUCallBackList	nuContCallBack;
extern u16		nuContPakCompanyCode;	/* 会社コード */
extern u32		nuContPakGameCode;	/* ゲームコード */
extern NUCallBackList	nuContPakCallBack;

/*--------------------------------------*/
/*  RUMBUL manager variables 		*/
/*--------------------------------------*/
extern NUContRmbCtl	nuContRmbCtl[4];
extern u32		nuContRmbSearchTime;
extern NUCallBackList	nuContRmbCallBack;


/*--------------------------------------*/
/*  EEPROM manager variables 		*/
/*--------------------------------------*/
extern NUCallBackList	nuEepromCallBack;

/*--------------------------------------*/
/*  64GBPack  manager variables 	*/
/*--------------------------------------*/
extern NUCallBackList	nuContGBPakCallBack;

/*--------------------------------------*/
/*  VRS  manager variables 		*/
/*--------------------------------------*/
extern NUCallBackList	nuVrsCallBack;
					   
/*--------------------------------------*/
/*  si variables 			*/
/*--------------------------------------*/
extern OSMesgQueue	nuSiMesgQ;	/* SIイベントメッセージキュー */
extern OSMesgQueue	nuSiMgrMesgQ;	/* SIマネージャのキュー */
extern NUCallBackList*	nuSiCallBackList;	/* コールバック関数リスト */

/*--------------------------------------*/
/*  pi variables 			*/
/*--------------------------------------*/
extern OSPiHandle*	nuPiCartHandle;
extern OSPiHandle*	nuPiSramHandle;
    
/*--------------------------------------*/
/* CALL BACK Function pointer 		*/
/*--------------------------------------*/
extern NUIdleFunc	nuIdleFunc;		/* callback func pointer */
extern NUScPreNMIFunc	nuScPreNMIFunc;		/* PRE NMI callback pointer  */
extern NUGfxSwapCfbFunc	nuGfxSwapCfbFunc; 	/* swapbuf callback pointer  */
extern NUGfxFunc	nuGfxFunc;		/* callback pointer 	*/
extern NUGfxPreNMIFunc	nuGfxPreNMIFunc;	/* callback pointer 	*/
extern NUGfxTaskEndFunc	nuGfxTaskEndFunc; 	/* task end callback  ptr */
extern NUContReadFunc	nuContReadFunc;		/* コントローラリード終了時の*/
						/* コールバック関数	*/
/*--------------------------------------*/
/* Debug 		 		*/
/*--------------------------------------*/
extern NUDebTaskPerf*	nuDebTaskPerfPtr;
extern NUDebConWindow	nuDebConWin[];

/*----------------------------------------------------------------------*/    
/*----------------------------------------------------------------------*/
/* Global Function 							*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/*--------------------------------------*/
/* Boot function			*/
/*--------------------------------------*/
extern void nuIdleFuncSet(void (*func)(void));
extern void nuPreNMIFuncSet(NUScPreNMIFunc func);
/*--------------------------------------*/
/* Scheduler function			*/
/*--------------------------------------*/
extern void nuScCreateScheduler(u8 videoMode, u8 numFields);
extern void nuScAddClient(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
extern void nuScRemoveClient(NUScClient *client);
extern void nuScResetClientMesgType(NUScClient* client, NUScMsg msgType);
extern void nuPreNMIFuncSet(NUScPreNMIFunc func);
extern OSMesgQueue* nuScGetGfxMQ(void);
extern OSMesgQueue* nuScGetAudioMQ(void);
extern void nuScSetFrameBufferNum(u8 frameBufferNum);
extern s32 nuScGetFrameRate(void);
					   
/*--------------------------------------*/
/* graphic(GFX) manager function	*/
/*--------------------------------------*/
extern void nuGfxInit(void);
extern void nuGfxInitEX2(void);
extern void nuGfxThreadStart(void);
extern void nuGfxTaskMgrInit(void);
extern void nuGfxTaskStart(Gfx *gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag);
extern void nuGfxFuncSet(NUGfxFunc func);
extern void nuGfxPreNMIFuncSet(NUGfxPreNMIFunc func);
extern void nuGfxSwapCfbFuncSet(NUGfxSwapCfbFunc func);
extern void nuGfxSetCfb(u16** framebuf, u32 framebufnum);
extern void nuGfxSwapCfb(void* framebuffer);

extern void nuGfxTaskEndFuncSet(NUGfxTaskEndFunc func);
extern void nuGfxTaskAllEndWait(void);
extern void nuGfxRetraceWait(u32 retrace_num);
extern void nuGfxDisplayOff(void);
extern void nuGfxDisplayOn(void);

#ifdef F3DEX_GBI_2
#define	nuGfxInit()	nuGfxInitEX2()
#endif /* F3DEX_GBI_2 */
/*--------------------------------------*/
/* controller manager function		*/
/*--------------------------------------*/
extern u8   nuContInit(void);
extern s32  nuContMgr(void* mesg);
extern u8   nuContMgrInit(void);
extern void nuContMgrRemove(void);
extern void nuContDataLock(void);
extern void nuContDataUnLock(void);
extern void nuContDataRead(OSContPad* pad);
extern void nuContDataReadWait(void);
extern void nuContDataReadStart(void);
extern void nuContDataGet(OSContPad* contdata, u32 padno);
extern void nuContDataGetAll(OSContPad* contdata);
extern void nuContDataGetEx(NUContData *contdata, u32 padno);
extern void nuContDataGetExAll(NUContData *contdata);
extern void nuContReadFuncSet(NUContReadFunc func);
extern void nuContQueryRead(void);
extern void nuContDataClose(void);
extern void nuContDataOpen(void);
    
/*--------------------------------------*/
/* controller pak function		*/
/*--------------------------------------*/
extern s32  nuContPakMgr(void* mesg_type);
extern void nuContPakMgrInit(void);
extern void nuContPakMgrRemove(void);
extern void nuContPakJisToN64(u8* src, u8* dest, u32 len);
extern void nuContPakN64ToJis(u8* src, u8* dest, u32 len);
extern s32  nuContPakOpen(NUContPakFile *file, u32 contNo);
extern s32  nuContPakGetFree(NUContPakFile *file);
extern void nuContPakCodeSet(u8* companyCode, u8* gameCode);
extern s32  nuContPakRepairId(NUContPakFile *file);
extern s32  nuContPakFileOpen(NUContPakFile *file,
			      u8* noteName,u8* extName, u32 mode, s32 size);
extern s32  nuContPakFileOpenJis(NUContPakFile *file,
				 u8* noteName,u8* extName, u32 mode, s32 size);
extern s32  nuContPakFileReadWrite(NUContPakFile *file,
			    s32 offset, s32 size,u8* buf, u32 mode);
extern s32  nuContPakFileDelete(NUContPakFile *file, u8* noteName, u8* extName);
extern s32  nuContPakFileDeleteJis(NUContPakFile *file, u8* noteName, u8* extName);
extern s32  nuContPakFileState(NUContPakFile *file, OSPfsState* state);
extern s32  nuContPakFileNum(NUContPakFile *file,
			     s32 *max_files, s32 *used_files);
extern s32 nuContPakFileFread(NUContPakFile *file, s32 offset, s32 size, u8* buf);

/*--------------------------------------*/
/* rumbler pak function			*/
/*--------------------------------------*/
extern void nuContRmbMgrInit(void);
extern s32  nuContRmbMgr(void* mesg);
extern void nuContRmbMgrRemove(void);
extern void nuContRmbStart(u32 contNo, u16 freq, u16 frame);
extern void nuContRmbStop(u32 contNo);
extern s32  nuContRmbCheck(u32 contNo);
extern void nuContRmbModeSet(u32 contNo, u8 mode);
extern void nuContRmbForceStop(void);
extern void nuContRmbForceStopEnd(void);
extern void nuContRmbSearchTimeSet(u32 frame);

/*--------------------------------------*/
/* 64GB Pack function			*/
/*--------------------------------------*/
extern s32 nuContGBPakMgr(void* mesg);
extern void nuContGBPakMgrInit(void);
extern void nuContGBPakMgrRemove(void);
extern s32 nuContGBPakOpen(NUContPakFile* handle, s32 contNo);
extern s32 nuContGBPakGetStatus(NUContPakFile* handle, u8* status);
extern s32 nuContGBPakReadID(NUContPakFile* handle, OSGbpakId* id, u8* status);
extern s32 nuContGBPakReadWrite(NUContPakFile* handle, u16 flag , u16 address, u8* buffer, u16 size);
extern s32 nuContGBPakPower(NUContPakFile* handle, s32 flag);
extern s32 nuContGBPakCheckConnector(NUContPakFile* handle, u8* status);
extern s32 nuContGBPakFread(NUContPakFile* handle, u16 address, u8* buffer, u16 size);

extern s32 nuContGBPakFwrite(NUContPakFile* handle, u16 address, u8* buffer, u16 size);
extern s32 nuContGBPakRegWrite(NUContPakFile* handle, u16 addr, u8 data);

/*--------------------------------------*/
/* eeprom function			*/
/*--------------------------------------*/
extern void nuEepromMgrInit(void);
extern s32  nuEepromMgr(void* mesg);
extern s32  nuEepromCheck(void);
extern void nuEepromMgrRemove(void);
extern s32 nuEepromRead(u8 address, u8* buffer,u16 len);
extern s32 nuEepromWrite(u8 address, u8* buffer,u16 len);

    
/*--------------------------------------*/
/* pi function				*/
/*--------------------------------------*/
extern void nuPiInit(void);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);
extern void nuPiInitSram(void);
extern void nuPiInitDDrom(void);
extern void nuPiReadWriteSram(u32 addr, void* buf_ptr, u32 size, s32 flag);

/*--------------------------------------*/
/* si function				*/
/*--------------------------------------*/
extern u8 nuSiMgrInit(void);
extern void nuSiCallBackAdd(NUCallBackList* list);
extern void nuSiCallBackRemove(NUCallBackList* list);
extern s32 nuSiSendMesg(NUScMsg mesg, void* dataPtr);
extern void nuSiSendMesgNW(NUScMsg mesg, void* dataPtr);
extern void nuSiMgrStop(void);
extern void nuSiMgrRestart(void);
/*--------------------------------------*/
/* si function				*/
/*--------------------------------------*/
extern s32 nuVrsMgr(void *mesg);
extern void nuVrsMgrInit(void);
extern s32 nuVrsOpen(NUVrsHandle* handle, s32 contNo);
extern s32 nuVrsClearDictionary(NUVrsHandle* handle, u8 words);
extern s32 nuVrsSetWord(NUVrsHandle* handle, u8* word);
extern s32 nuVrsStartReadData(NUVrsHandle* handle);
extern s32 nuVrsGetReadData(NUVrsHandle* handle, NUVrsData* data);
extern s32 nuVrsStopReadData(NUVrsHandle* handle);
extern s32 nuVrsControlGain(NUVrsHandle* handle, s32 analog, s32 digital);
extern s32 nuVrsMaskDictionary(NUVrsHandle* handle, u8* maskpattern, s32 size);

/*--------------------------------------*/
/* dubug function			*/
/*--------------------------------------*/
#ifdef NDEBUG
#define nuDebTaskPerfBar0(EX0 ,EX1 ,EX2)       ((void)0)
#define nuDebTaskPerfBar1(EX0 ,EX1 ,EX2)       ((void)0)
#define nuDebTaskPerfBar0EX2(EX0 ,EX1 ,EX2)       ((void)0)
#define nuDebTaskPerfBar1EX2(EX0 ,EX1 ,EX2)       ((void)0)
#else
extern void nuDebTaskPerfBar0(u32 frameNum, u32 y, u32 flag);
extern void nuDebTaskPerfBar1(u32 frameNum, u32 y, u32 flag);
extern void nuDebTaskPerfBar0EX2(u32 frameNum, u32 y, u32 flag);
extern void nuDebTaskPerfBar1EX2(u32 frameNum, u32 y, u32 flag);
#endif /* NDEBUG */

extern void nuDebConDisp(u32 flag);
extern void nuDebConDispEX2(u32 flag);
extern void nuDebConCPuts(u32 wndNo, const char *s);
extern void nuDebConInc(NUDebConWindow* conWin);
extern void nuDebConRtn(NUDebConWindow* conWin);
extern void nuDebConEsc(NUDebConWindow* conWin, char esc);
extern void nuDebConWindowSet(u32 wndNo, u32 winX , u32 winY, u32 width, u32 height);
extern void nuDebConWindowSize(u32 wndNo,  u32 width, u32 height);
extern void nuDebConWindowPos(u32 wndNo, u32 winX , u32 winY);
extern void nuDebConTextColor(u32 wndNo, u32 color);
extern void nuDebConTextAttr(u32 wndNo, u32 attr);
extern void nuDebConTextPos(u32 wndNo, u32 posX, u32 posY);
extern void nuDebConScroll(u32 wndNo, u32 flag);
extern void nuDebConWindowShow(u32 wndNo, u32 flag);
extern void nuDebConClear(u32 wndNo);
extern void nuDebConPutc(u32 wndNo, u32  c);
extern void nuDebTaskPerfLoad(void);
extern void nuDebConPrintf(u32 wndNo, const char* fmt, ...);

#ifdef F3DEX_GBI_2
#define	nuDebConDisp(flag)		nuDebConDispEX2(flag)
#define nuDebTaskPerfBar0(a, b, c)	nuDebTaskPerfBar0EX2(a, b, c)
#define nuDebTaskPerfBar1(a, b, c)	nuDebTaskPerfBar1EX2(a, b, c)
#endif	/* F3DEX_GBI_2 */

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/* MACRO	 							*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*	グラフィックマイクロコードの登録				*/
/*	IN:	グラフィックスマイクロコードのNUUcode配列のポインタ	*/
/*----------------------------------------------------------------------*/
#define nuGfxSetUcode(ucode)						\
{									\
    nuGfxUcode = ucode;							\
}
/*----------------------------------------------------------------------*/
/*	Zバッファの設定							*/
/*	IN:	Zバッファのポインタ					*/
/*----------------------------------------------------------------------*/
#define nuGfxSetZBuffer(ZBuf_p)						\
{									\
    nuGfxZBuffer = ZBuf_p;						\
}
/*----------------------------------------------------------------------*/
/*	コールバック関数の削除						*/
/*	IN:	なし							*/
/*----------------------------------------------------------------------*/
#define	nuPreNmiFuncRemove()		nuPreNmiFuncSet(NULL)
#define nuGfxFuncRemove()		nuGfxFuncSet(NULL)
#define nuGfxPreNMIFuncRemove()		nuGfxPreNMIFuncSet(NULL)
#define	nuGfxSwapCfbFuncRemove()	nuGfxSwapCfbFuncSet(NULL)
#define	nuGfxTaskEndFuncRemove()	nuGfxTaskEndFuncSet(NULL)
#define nuContReadFuncRemove()		nuContReadFuncSet(NULL)

/*----------------------------------------------------------------------*/
/*	nuContDataGet - コントローラデータの取得			*/
/*	読み込まれたデータをコントローラマネージャから取得する		*/
/*									*/
/*	IN:	*contpad コントローラ構造体のポインタ			*/
/* 		cont_no  コントローラの番号				*/
/*----------------------------------------------------------------------*/
#if 0
#define	nuContDataGet(contpad, cont_no)					\
{									\
    bcopy(&nuContData[cont_no], contpad, sizeof(OSContPad));		\
}

/*----------------------------------------------------------------------*/
/*	nuContDataGetAll - コントローラデータの取得			*/
/*	読み込まれたデータをコントローラマネージャから取得する		*/
/*	４つ全部のバッファをコピーする					*/
/*	IN:	*contpad コントローラ構造体のポインタ			*/
/* 		cont_no  コントローラの番号				*/
/*----------------------------------------------------------------------*/
#define nuContDataGetAll(contpad)					\
{									\
    bcopy(&nuContData[0], contpad, sizeof(OSContPad)*MAXCONTROLLERS);\
}
#endif
/*----------------------------------------------------------------------*/
/*	nuContPakFileFind - ノードの検索				*/
/*----------------------------------------------------------------------*/
#define nuContPakFileFind(file, nodeName, extName)			\
    nuContPakFileOpen(file, nodeNname, extName, NU_CONT_PAK_NOCREAT, 0)\


/*----------------------------------------------------------------------*/
/*	nuContPakFileFindJis - ノートの検索				*/
/*----------------------------------------------------------------------*/
#define nuContPakFileFindJis(file, nodeName, extName)			\
    nuContPakFileOpenJis(file, nodeNname, extName, NU_CONT_PAK_NOCREAT, 0)\

/*----------------------------------------------------------------------*/
/*	nuContPakFileRead - ノートの読み込み				*/
/*----------------------------------------------------------------------*/
#define nuContPakFileRead(file, offset, size, buf)			\
    nuContPakFileReadWrite(file, offset, size, buf, PFS_READ)		\

/*----------------------------------------------------------------------*/
/*	nuContPakFileWrite - ノートの書き込み				*/
/*----------------------------------------------------------------------*/
#define nuContPakFileWrite(file, offset, size, buf)			\
    nuContPakFileReadWrite(file, offset, size, buf, PFS_WRITE)		\

/*----------------------------------------------------------------------*/
/*	nuDebConPuts - 文字列の出力(改行付)				*/
/*----------------------------------------------------------------------*/
#define  nuDebConPuts(wndNo, s)						\
{									\
    nuDebConCPuts(wndNo, s);						\
    nuDebConRtn(&nuDebConWin[wndNo]);					\
}

/*----------------------------------------------------------------------*/
/*	nuContGBPakRead - GBカートリッジの読み込み			*/
/*----------------------------------------------------------------------*/
#define nuContGBPakRead(file, address, buf, size)			\
    nuContGBPakReadWrite(file, OS_READ, address, buf, size)

/*----------------------------------------------------------------------*/
/*	nuContGBPakWrite - GBカートリッジの書き込み			*/
/*----------------------------------------------------------------------*/
#define nuContGBPakWrite(file, address, buf, size)			\
    nuContGBPakReadWrite(file, OS_WRITE, address, buf, size)

/*----------------------------------------------------------------------*/
/*	nuPiReadSram - SRAMからの読み込み				*/
/*----------------------------------------------------------------------*/
#define nuPiReadSram(addr, buf_ptr, size)				\
	nuPiReadWriteSram(addr, buf_ptr, size, OS_READ)
    
/*----------------------------------------------------------------------*/
/*	nuPiWriteSram - SRAMへの書き込み				*/
/*----------------------------------------------------------------------*/
#define nuPiWriteSram(addr, buf_ptr, size)				\
    nuPiReadWriteSram(addr, buf_ptr, size, OS_WRITE)
	
/*----------------------------------------------------------------------*/
/*	音声認識関係のマクロ定義					*/
/*----------------------------------------------------------------------*/
#define	nuVrsCheckWord		osVoiceCheckWord
#define	nuVrsCountSyllables	osVoiceCountSyllables

#endif  /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif
#endif /* _NUSYS_H_ */


