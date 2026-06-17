/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    double atan(double)
    double atan2(double , double)

*/

#include "_kmclib.h"
#include <math.h>
#include <cordic.h>

/*** for Turboc ***/

/**** for GCC ****/
#define NR 1.0e-7


extern XLONG _atbl[];            /* atan (2^-i) */


_xatan(u,v,atanp)
XLONG u,v;    /* 0<= u & v <= 1 ,   tan=v/u */
double *atanp;    /* atan */
{
    int i;
    XLONG x,xx,y,z;
/* calc u & v */

    x=u;
    y=v;
    z=0;
    for(i=0;i<CBIT;++i) {
        if (y>=0) {
            xx=x+(y>>i);
            y=y-(x>>i);
            z=z+_atbl[i];
        }
        else {
            xx=x-(y>>i);
            y=y+(x>>i);
            z=z-_atbl[i];
        }
        x=xx;
    }
    *atanp = (double)z/MBIT;
}

double atan(d)
double d;
{
    double a;
    XLONG u,v;
    int si;

    if (d<0) {
        d=-d;
        si=-1;
    }
    else si=1;
    if (d>NR) {
    if (d>=1.) {
            v=1*MBIT;
            u=(1./d)*MBIT;
    }
        else {
            u=1*MBIT;
            v=d*MBIT;
    }
    _xatan(u,v,&a);
    }
    else a=d;
    return (si<0 ? -a : a);
}


double atan2(y,x)
double y,x;
{
    double d;
       
    if(x==0 && y==0) {
    return __matherr(_DOMAIN,"atan2",x,y,0.0);
    }
    if (x==0) {
        if (y>0) return PI/2;
        else return -PI/2;
    }
    d=y/x;
    d=atan(d);
    if (x>0) return d;
    else {
    if (y==0) return PI;
    else if (y>0) return d+PI;
    else return d-PI;
    }
}





