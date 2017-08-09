include ~/.xmk_header

#--------------add the files need to compile------------------------------------
#if some source file are not in current directory, set them in VPATH (directory names are separated by colons(:) or blanks. 
VPATH = ../common

SOURCE_FILES =         \
StorageAccessExe.cpp   \


#-------------change target type------------------------------------------------ 	
#TARGET_TYPE can be:  dll , lib , exe
TARGET_TYPE = exe

#-------------change target name------------------------------------------------	
#for lib, the name begin with lib and end with .a such as "libgmhelper.a"
#for dll, the name begin with lib and end with .so such as "libcommon.so"
#for exe, the name desn't need prefix and postfix, such as "weichat" is ok
TARGET_NAME=storage

#-----------change to your last inc path(reative to INC_DPD_ROOT and INC_OUT_ROOT 	
#if you have no head file to output
#INC_LAST_PATH=XTools

#----------libs need linking together-------------------------------------------
#if the target doesn't depend any thing, just comment the line
DEPENDED_LIBS += -lstorage_access3.1 -lxtools -llogwriter -lgmhelper  -ltbb 
#-laio_basic -lxtools -llogwriter -lgmhelper -lrt -lpthread

LDFLAGS += -Wl,-rpath,/opt/redcdn/lib

#----------extra include pathes---------------------------------------------------
#pathes besides dpd/inc, relative to the path of this makefile
EXTRA_INC = -I../common

#-------------------------------------------------------------------------------

include ~/.xmk_footer

