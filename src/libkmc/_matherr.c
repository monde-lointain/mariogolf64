/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    double __matherr(int type , char *name ,
        double a1 , double a2 ,double retval)
*/


#include "_kmclib.h"
#include <math.h>

double __matherr(type,name,a1,a2,retval)
int type;
char *name;
double a1,a2,retval;
{
    struct _mexception ex;

    ex.type=type;
    ex.name=name;
    ex.arg1=a1;
    ex.arg2=a2;
    ex.retval=retval;
    if (_matherr(&ex)==0) {
    errno = ((type==_OVERFLOW) || (type==_UNDERFLOW)) ? ERANGE : EDOM;
    }
    return ex.retval;
}
