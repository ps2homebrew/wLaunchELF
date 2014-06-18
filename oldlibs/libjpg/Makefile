# myPS2 libjpg Makefile
#
# Author: ntba2
# Date: 12-22-2005

SYSDEPMEM	= jmemnobs.o

# library object files common to compression and decompression
COMOBJECTS	=	jcomapi.o	\
			jutils.o	\
			jerror.o	\
			jmemmgr.o	\
			$(SYSDEPMEM)

# compression library object files
CLIBOBJECTS	=	jcapimin.o	\
			jcapistd.o	\
			jctrans.o	\
			jcparam.o	\
			jdatadst.o	\
			jcinit.o	\
			jcmaster.o	\
			jcmarker.o	\
			jcmainct.o	\
			jcprepct.o	\
			jccoefct.o	\
			jccolor.o	\
			jcsample.o	\
			jchuff.o	\
			jcphuff.o	\
			jcdctmgr.o	\
			jfdctfst.o	\
			jfdctflt.o	\
			jfdctint.o

# decompression library object files
DLIBOBJECTS	=	jdapimin.o	\
			jdapistd.o	\
			jdatasrc.o	\
			jdtrans.o	\
			jdmaster.o	\
			jdinput.o	\
			jdmarker.o	\
			jdhuff.o	\
			jdphuff.o	\
			jdmainct.o	\
			jdcoefct.o	\
			jdpostct.o	\
			jddctmgr.o	\
			jidctfst.o	\
			jidctflt.o	\
			jidctint.o	\
			jidctred.o	\
			jdsample.o	\
			jdcolor.o	\
			jquant1.o	\
			jquant2.o	\
			jdmerge.o

# ps2 jpg library object files
PS2OBJECTS = libjpg.o

# These objectfiles are included in libjpeg.a
LIBOBJECTS = $(CLIBOBJECTS) $(DLIBOBJECTS) $(COMOBJECTS) $(PS2OBJECTS)

EE_INCS	= -I$(LIBJPG)/include -I.
EE_LIB	= libjpg.a
EE_OBJS	= $(LIBOBJECTS)

all:	$(EE_LIB)

clean:
	rm -f *.o *.a

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
