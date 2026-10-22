/*======================================================================*/
/*		NuSYS							*/
/*		nugfxsetcfb.c						*/
/*									*/
/*		Copyright (C) 1997, NINTENDO Co,Ltd.			*/
/*									*/
/*----------------------------------------------------------------------*/    
/* Ver 1.0	97/10/9		Created by Kensaku Ohki(SLANP)		*/
/*======================================================================*/  
#include <nusys.h>

/* Mario Golf replaces this file. With two frame buffers, the memory of
   the third one holds the depth buffer; with three, the depth buffer
   moves to a fixed address below them. The buffers in use form a ring
   through g_NextFramebuffer, and the unused one is kept aside. */

/* Depth buffer address used while three frame buffers are in use */
#define TRIPLE_BUFFERING_DEPTH_BUFFER ((u16*)0x802EA000)

/* Number of frame buffers with which the depth buffer moves */
#define TRIPLE_BUFFERING 3

/* sSpareFramebuffer value while every frame buffer is in use */
#define NO_SPARE_FRAMEBUFFER 0xFF

u16*	g_DepthBuffer = (u16*)0x803DA800;
u8	g_NextFramebuffer[3] = { 1, 0, 0 };
static u8	sSpareFramebuffer = NO_SPARE_FRAMEBUFFER;

/*----------------------------------------------------------------------*/
/*	Framebuffer Settings						*/
/*	IN:	**framebuf 	Pointer to an array of three frame buffers */
/*		framebufnum 	Number of frame buffers to use (2 or 3)	*/
/*----------------------------------------------------------------------*/
void nuGfxSetCfb(u16** framebuf, u32 framebufnum)
{
    nuGfxCfb	= framebuf;
    nuGfxCfbNum	= framebufnum;
    nuGfxCfbCounter = 0;
    nuGfxCfb_ptr = framebuf[0];

    if(framebufnum == TRIPLE_BUFFERING){
	g_DepthBuffer = TRIPLE_BUFFERING_DEPTH_BUFFER;
	g_NextFramebuffer[0] = 1;
	g_NextFramebuffer[1] = 2;
	g_NextFramebuffer[2] = 0;
	sSpareFramebuffer = NO_SPARE_FRAMEBUFFER;
    } else {
	g_DepthBuffer = framebuf[2];
	g_NextFramebuffer[0] = 1;
	g_NextFramebuffer[2] = NO_SPARE_FRAMEBUFFER;
	g_NextFramebuffer[1] = 0;
	sSpareFramebuffer = 2;
    }
}

/*----------------------------------------------------------------------*/
/*	Switches between double and triple buffering without disturbing	*/
/*	the frame buffer being rendered					*/
/*	IN:	framebufnum 	Number of frame buffers to use (2 or 3)	*/
/*----------------------------------------------------------------------*/
void SetFramebufferCount(u32 framebufnum)
{
    if(nuGfxCfbNum == framebufnum){
	return;
    }
    if(framebufnum == TRIPLE_BUFFERING){
	/* Put the spare buffer in the ring after the rendered one. */
	nuGfxCfbNum = framebufnum;
	g_NextFramebuffer[sSpareFramebuffer] = nuGfxCfbCounter;
	g_NextFramebuffer[g_NextFramebuffer[nuGfxCfbCounter]] = sSpareFramebuffer;
	g_DepthBuffer = TRIPLE_BUFFERING_DEPTH_BUFFER;
	nuGfxCfbCounter = sSpareFramebuffer;
	nuGfxCfb_ptr = nuGfxCfb[sSpareFramebuffer];
	sSpareFramebuffer = NO_SPARE_FRAMEBUFFER;
    } else {
	/* Take the rendered buffer out of the ring to hold depth. */
	nuGfxCfbNum = framebufnum;
	sSpareFramebuffer = nuGfxCfbCounter;
	nuGfxCfbCounter = g_NextFramebuffer[nuGfxCfbCounter];
	g_NextFramebuffer[g_NextFramebuffer[nuGfxCfbCounter]] = nuGfxCfbCounter;
	nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
	g_DepthBuffer = nuGfxCfb[sSpareFramebuffer];
    }
}
