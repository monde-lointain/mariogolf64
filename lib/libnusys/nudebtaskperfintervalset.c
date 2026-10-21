/*======================================================================*/
/*		NuSYS							*/
/*		nudebtaskperfinterval.c					*/
/*									*/
/*		Copyright (C) 1999, NINTENDO Co,Ltd.			*/
/*									*/
/*======================================================================*/
/* $Id: nudebtaskperfintervalset.c,v 1.1 1999/05/27 01:49:42 ohki Exp ohki $	*/
/*======================================================================*/
#ifdef NU_DEBUG
#include <nusys.h>

/*----------------------------------------------------------------------*/
/*  nuDebTaskPerfIntervalSet  Sets the performance measuring interval   */
/*									*/
/*     Sets the interval between measurements of performace.            */
/*  Performance is measured at set intervals when the frame buffer      */
/*  is swapped.  (The initial value is 1)				*/
/*	IN:     Sampling interval (in units of frame buffer rendering)  */
/*	RTN:	None							*/
/*----------------------------------------------------------------------*/
void nuDebTaskPerfIntervalSet(u32 interval)
{
    OSIntMask mask;
    mask = osSetIntMask(OS_IM_NONE);
    nuDebTaskPerfInterval = interval;
    osSetIntMask(mask);
}
#endif /* NU_DEBUG */
