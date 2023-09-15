#
# 	NuSYS Makefile for NINTENDO64
#
#	Copyright (C) 1997, NINTENDO Co,Ltd.
#

MAKE		=	make
CP		=	copy

default:
	$(MAKE) -C nusys      -f Makefile.dos
	$(MAKE) -C nualsgi    -f Makefile.dos
	$(MAKE) -C nualstl3    -f Makefile.dos

install:
	$(MAKE) -C nusys      -f Makefile.dos install 
	$(MAKE) -C nualsgi    -f Makefile.dos install 
	$(MAKE) -C nualstl3    -f Makefile.dos install 


clean:
	$(MAKE) -C nusys      -f Makefile.dos clean 
	$(MAKE) -C nualsgi    -f Makefile.dos clean
	$(MAKE) -C nualstl3    -f Makefile.dos clean

dosmake:
	$(CP)  nusys\Makefile.dos     nusys\Makefile
	$(CP)  nualsgi\Makefile.dos   nualsgi\Makefile
	$(CP)  nualstl3\Makefile.dos   nualstl3\Makefile 
