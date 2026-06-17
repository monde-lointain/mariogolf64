/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    double sin(double ),cos(double),tan(double)
*/

#include "_kmclib.h"
#include <math.h>
#include <cordic.h>

/*** for Turboc ***/
/**** for GCC ****/
#define NR 1.0e-8


extern XLONG _atbl[];            /* atan (2^-i) */


_xsincos(th,sinp,cosp)
double th;        /* -pi/2 <= th <= pi/2 */
double *sinp,*cosp;    /* sin , cos */
{
    int i;
    XLONG x,xx,y,z;
    
    x=(double)K*MBIT;
    y=0;
    z=th*MBIT;
    for(i=0;i<CBIT;++i) {
        if (z>=0) {
            xx=x-(y>>i);
            y=y+(x>>i);
            z=z-_atbl[i];
        }
        else {
            xx=x+(y>>i);
            y=y-(x>>i);
            z=z+_atbl[i];
        }
        x=xx;
    }
    *sinp = (double)y/MBIT;
    *cosp = (double)x/MBIT;
}

double sin(th)
double th;
{
    double s,c;
    int ti,sign;
    
    ti = (th / PI)+(th>=0 ? 0.5 : -0.5);
    th = th - ti*PI;
    
    if(th < NR && th >-NR) return th;

    _xsincos(th,&s,&c);
    return (ti&1 ? -s:s);
}

double cos(th)
double th;
{
    double s,c;
    int ti;
    
    ti = (th / PI)+(th>=0 ? 0.5 : -0.5);
    th = th - ti*PI;

    _xsincos(th,&s,&c);
    return (ti&1 ? -c : c);
}


double tan(th)
double th;
{
    double s,c;
    int ti;
    
    ti = (th / PI)+(th>=0 ? 0.5 : -0.5);
    th = th - ti*PI;

    if(th < NR && th >-NR) return th;

    _xsincos(th,&s,&c);
    return s/c;
}






