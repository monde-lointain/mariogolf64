#
# 	NuSystem Makefile for NINTENDO64
#
#	Copyright (C) 1997-1999 NINTENDO Co.,Ltd.
#

N64KITDIR     = c:\nintendo\n64kit
NUSYSINCDIR  = $(N64KITDIR)\nusys\include
NUSYSLIBDIR  = $(N64KITDIR)\nusys\lib

LIB = $(ROOT)/usr/lib
LPR = $(LIB)/PR
INC = $(ROOT)/usr/include
CC  = gcc
LD  = ld
MAKE		=	make
RM		=	del
CP              =       copy
AR		=	ar
ARFLAGS		=	cr
RANLIB          =       ranlib
MAKEROM = mild

.c.o:
	$(CC) $(OPTIMIZER) $(MOPTDEF) -DUSE_EPI -G 0 -c -I.. -I$(INC)/PR -I$(INC) -D_LANGUAGE_C -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 $<

#
# NUSYS LIBRARY
#
#

NUBOOTSRC	=	nuboot.c
NUBOOTOBJ	=	$(NUBOOTSRC:.c=.o)
NUBOOTLIB	= 	nusys.o
ROMLIB		=	libnusys.a
DEBUGLIB	=	libnusys_d.a
ROMOBJDIR       =       libnusys
DEBUGOBJDIR     =       libnusys_d
HEADER          =       nusys.h

LIBSYSSRC	= 	nusched.c 			\
			nuprenmifuncset.c		\
			nurdpoutput.c 			\
			nuyieldbuf.c 			\
			nudramstack.c 			\
			nuidlefuncset.c 		\
			nugfxthread.c 			\
			nugfxtaskmgr.c 			\
			nugfxinit.c  			\
			nugfxfuncset.c 			\
			nugfxprenmifuncset.c 		\
			nugfxtaskendfuncset.c 		\
			nugfxswapcfbfuncset.c		\
			nugfxswapcfb.c			\
			nugfxtaskallendwait.c		\
			nugfxretracewait.c		\
			nugfxsetcfb.c			\
			nugfxdisplayoff.c 		\
			nugfxdisplayon.c 		\
			nudebtaskperfbar0.c		\
			nudebtaskperfbar1.c		\
			nudebload.c			\
			nudebconsole.c			\
			nudebconputc.c			\
			nudebconcputs.c			\
			nudebconwindowset.c		\
			nudebconwindowpos.c		\
			nudebconwindowsize.c		\
			nudebconwindowshow.c		\
			nudebcontextcolor.c		\
			nudebcontextattr.c		\
			nudebcontextpos.c		\
			nudebconscroll.c		\
			nudebconclear.c			\
			nudebconprintf.c		\
			nusimgr.c			\
			nusicallbackadd.c		\
			nusicallbackremove.c		\
			nusisendmesgnw.c		\
			nupiinit.c			\
			nupireadrom.c			\
			nupiinitsram.c			\
			nupiinitddrom.c			\
			nupireadwritesram.c		\
			nudebconbuf.c			\
			nudebcondisp.c			\
			nudebcondisp_ex2.c		\
			nugfxinit_ex2.c			\
			nudebtaskperfbar0_ex2.c		\
			nudebtaskperfbar1_ex2.c		\
			nucontmgr.c 			\
			nucontinit.c 			\
			nucontdatalock.c		\
			nucontdataread.c		\
			nucontdatareadwait.c 		\
			nucontdatareadstart.c 		\
			nucontdataget.c			\
			nucontdatagetall.c		\
			nucontdatagetex.c		\
			nucontdatagetexall.c		\
			nucontqueryread.c		\
			nucontreadfuncset.c		\
			nucontpakmgr.c			\
			nucontpakjiston64.c		\
			nucontpakopen.c			\
			nucontpakgetfree.c		\
			nucontpakrepairid.c		\
			nucontpakfileopen.c		\
			nucontpakfileopenjis.c		\
			nucontpakcodeset.c		\
			nucontpakfilereadwrite.c	\
			nucontpakfiledelete.c		\
			nucontpakfiledeletejis.c	\
			nucontpakfilestate.c		\
			nucontpakfilenum.c		\
			nucontrmbmgr.c			\
			nucontrmbstart.c		\
			nucontrmbstop.c			\
			nucontrmbcheck.c		\
			nucontrmbmodeset.c		\
			nucontrmbforcestop.c		\
			nucontrmbforcestopend.c		\
			nucontrmbsearchtimeset.c	\
			nueeprommgr.c			\
			nucontgbpakmgr.c		\
			nucontgbpakopen.c		\
			nucontgbpakgetstatus.c		\
			nucontgbpakpower.c		\
			nucontgbpakreadid.c		\
			nucontgbpakreadwrite.c		\
			nucontgbpakcheck.c		\
			nucontgbpakfread.c		\
			nucontgbpakfwrite.c		\
			nucontgbpakregwrite.c		\
			nucontpakfilefread.c		\
			nucontpakfilefwrite.c		\
			nuvrsmgr.c			\
			nuvrsopen.c			\
			nuvrscleardictionary.c		\
			nuvrssetword.c			\
			nuvrsmaskdictionary.c		\
			nuvrsstartreaddata.c		\
			nuvrsgetreaddata.c		\
			nuvrsstopreaddata.c		\
			nuvrscontrolgain.c		\
			numark.c			\
			nudebperfmarkset.c		\
			nudebtaskperfintervalset.c	\
			nufont.c			\
			nupireadromoverlay.c

LIBSYSOBJ	=	$(LIBSYSSRC:.c=.o)

VPATH           = ..

#
# compile
#
default: $(NUBOOTLIB) nusys.h
	-mkdir $(ROMOBJDIR)
	$(MAKE) -C $(ROMOBJDIR) -f ..\Makefile OPTIMIZER=-O3 $(ROMLIB)
	-mkdir $(DEBUGOBJDIR)
	$(MAKE) -C $(DEBUGOBJDIR) -f ..\Makefile OPTIMIZER=-g MOPTDEF=-DNU_DEBUG $(DEBUGLIB)

$(NUBOOTLIB):   $(NUBOOTOBJ) nusys.h
	$(LD) -o $(NUBOOTLIB) -r $(NUBOOTOBJ)

$(ROMLIB): $(LIBSYSOBJ) ../nusys.h
        $(AR) $(ARFLAGS) ../$(ROMLIB) $(LIBSYSOBJ)
        $(RANLIB) ../$(ROMLIB)

$(DEBUGLIB):    $(LIBSYSOBJ) ../nusys.h
        $(AR) $(ARFLAGS) ../$(DEBUGLIB) $(LIBSYSOBJ)
        $(RANLIB) ../$(DEBUGLIB)

install:
        $(CP) $(ROMLIB) $(NUSYSLIBDIR)
        $(CP) $(DEBUGLIB) $(NUSYSLIBDIR)
	$(CP) $(NUBOOTLIB)  $(NUSYSLIBDIR)
	$(CP) $(HEADER) $(NUSYSINCDIR)

clean:
	$(RM) $(ROMOBJDIR)\*.o
	$(RM) $(DEBUGOBJDIR)\*.o
	$(RM) $(NUBOOTLIB)
	$(RM) $(NUBOOTOBJ)
        $(RM) *.a
