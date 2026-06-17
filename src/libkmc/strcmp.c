/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    int strcmp(char *s1, char *s2);

*/

#include "_kmclib.h"
#include <memory.h>


int strcmp(s1,s2)
REG4 char *s1;
REG3 char *s2;
{
    REG1 UDWORD c,c1;
    REG2 int d;


#if FAST_SPEED /* { */

    if (((char *)s1-(char *)s2)&BUS_ERR_ALLIGN) {
    for(;;) {
        c= *((unsigned char *)s1)++;
        d= c - *((unsigned char *)s2)++;
        if (d || c==0) return d;
        }
    return 0;
    }


    while ((int)s1&3) {            /* Allign 32bit */
    c= *((unsigned char *)s1)++;
        d= c - *((UBYTE *)s2)++;    /* ALLIGN 16bit */
    if (d || c==0) return d;
    }
    for(;;) {
    c= *((UDWORD *)s1)++;
        d= c - (c1=*((UDWORD *)s2)++);    /* 32bit cmp */
    if(d){
        if ((c & 0xff000000)==0){
        d= 0xff000000;
        }
        else if ((c&0xff0000)==0){
        d = 0xffff0000;
        }
        else if ((c&0xff00)==0){
        d= 0xffffff00;
        }
        else{
        return d;
        }
        return (int)((c & d)-(c1 & d));
    }
    if ((char)c==0 || (c&0xff00)==0 || (c&0xff0000)==0 || (c&0xff000000)==0){
        return d;
    }
    }
#else /* }{ */
    for(;;) {
    c= *((unsigned char *)s1)++;
    d= c - *((unsigned char *)s2)++;
    if (d || c==0) return d;
    }
#endif /* } */
}

