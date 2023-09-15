#!smake -k
#
# 	NuSystem Makefile for NINTENDO64
#
#	Copyright (C) 1998, NINTENDO Co,Ltd.
#
# include $(ROOT)/usr/include/make/PRdefs

#
# NuSystem Install Directory
#
N64KITDIR	= 	C:\nintendo\n64kit
NUSYSDIR	=	$(N64KITDIR)\nusys
NUSYSLIBDIR	=	$(NUSYSDIR)\lib
NUSYSINCDIR	=	$(NUSYSDIR)\include
ROMOBJDIR	=	libnualstl
DEBUGOBJDIR	=	libnualstl_d
NROMOBJDIR	=	libnualstl_n
NDEBUGOBJDIR	=	libnualstl_n_d

#
# Environment
#
TARGETS		=	all
MAKE		=	make
OPTIMIZER	=	$(OPTMZ)
RM		=	del
CP		=	copy
AR		=	ar
ARFLAGS		=	cr

#
# NUSYS LIBRARY
#
#
LIBALSTLSRC	=	nuaustlmgr.c			\
			nuaustlinit.c			\
			nuaustlptrbankinit.c		\
			nuaustlptrbankset.c		\
			nuaustlseqplayerinit.c		\
			nuaustlseqplayerdataset.c	\
			nuaustlseqplayerplay.c		\
			nuaustlsndplayerinit.c		\
			nuaustlsndplayerdataset.c	\
			nuaustlsndplayerplay.c		\
			nuauprenmifuncset.c		\
			nuauprenmiproc.c		\
			nuaustlheap.c

ROMLIB		=	libnualstl.a
DEBUGLIB	=	libnualstl_d.a
NROMLIB		=	libnualstl_n.a
NDEBUGLIB	=	libnualstl_n_d.a


LIBALSTLOBJ	=	$(LIBALSTLSRC:.c=.o)


LCDEFS		=	$(MOPTDEF) -DNU_SYSTEM  -DUSE_EPI $(MUCODE)
LCINCS 		= 	-I.. -I$(NUSYSINCDIR) -I$(ROOT)/usr/include -I$(ROOT)/usr/include/PR

LCOPTS		=	-D_MIPS_SZLONG=32 -D_MIPS_SZINT=32

LDIRT	=	$(APP)

.SUFFICS: .c .o

.c.o:
	$(CC) $(OPTIMIZER) $(DEFINES) $(LCDEFS) -G 0 -c $(LCINCS) $(LCOPTS) $(LDFLAGS) $<


.PATH:	..


#
#	compile 
#

default:	$(TARGETS)

all:		makedir nualstl nualstl_n

makedir:
	-mkdir $(ROMOBJDIR)
	-mkdir $(DEBUGOBJDIR)
	-mkdir $(NROMOBJDIR)
	-mkdir $(NDEBUGOBJDIR)

nualstl:
        $(MAKE) -C $(ROMOBJDIR) -f ../Makefile.dos OPTMZ=-O3 $(ROMLIB)  VPATH='..'
	$(MAKE) -C $(DEBUGOBJDIR) -f ../Makefile.dos OPTMZ=-g MOPTDEF=-DNU_DEBUG $(DEBUGLIB) VPATH='..'

nualstl_n:
        $(MAKE) -C $(NROMOBJDIR) -f ../Makefile.dos OPTMZ=-O3 MUCODE=-DN_AUDIO $(NROMLIB)  VPATH='..'
	$(MAKE) -C $(NDEBUGOBJDIR) -f ../Makefile.dos OPTMZ=-g MOPTDEF=-DNU_DEBUG MUCODE=-DN_AUDIO $(NDEBUGLIB) VPATH='..'

$(ROMLIB):	$(LIBALSTLOBJ) nualstl.h
	$(AR) $(ARFLAGS) ../$(ROMLIB) $(LIBALSTLOBJ)

$(DEBUGLIB):	$(LIBALSTLOBJ) nualstl.h
	$(AR) $(ARFLAGS) ../$(DEBUGLIB) $(LIBALSTLOBJ)

$(NROMLIB):	$(LIBALSTLOBJ) nualstl.h
	$(AR) $(ARFLAGS) ../$(NROMLIB) $(LIBALSTLOBJ)

$(NDEBUGLIB):	$(LIBALSTLOBJ) nualstl.h
	$(AR) $(ARFLAGS) ../$(NDEBUGLIB) $(LIBALSTLOBJ)

install:
	$(CP) nualstl.h $(NUSYSINCDIR)
	$(CP) nualstl_n.h $(NUSYSINCDIR)
	$(CP) $(ROMLIB) $(NUSYSLIBDIR)
	$(CP) $(DEBUGLIB) $(NUSYSLIBDIR)
	$(CP) $(NROMLIB) $(NUSYSLIBDIR)
	$(CP) $(NDEBUGLIB) $(NUSYSLIBDIR)


clean:
	$(RM) $(ROMOBJDIR)\*.o
	$(RM) $(DEBUGOBJDIR)\*.o
	$(RM) $(NROMOBJDIR)\*.o
	$(RM) $(NDEBUGOBJDIR)\*.o
	$(RM) $(NROMLIB)
	$(RM) $(NDEBUGLIB)
	$(RM) $(ROMLIB)
	$(RM) $(DEBUGLIB)
