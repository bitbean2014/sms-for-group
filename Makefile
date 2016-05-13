# C Compiler include & library path setting
CC          = ${CC_COMPILER} ${CC_CFLAGS} ${CC_IPATH}
LN          = ${CC_COMPILER}
LB          = ar -r
CP          = cp
RM          = rm -f
MV          = mv
RMDIR		= rm -rf

#-------------------------------------------------

.SUFFIXES:
.SUFFIXES: .o .c
.SUFFIXES: .o .cpp

.c.o:
	$(CC) -o $(<:.c=.o) -c $<
.cpp.o:
	$(CC) -o $(<:.cpp=.o) -c $<


#-------------------------------------------------


TARGET      = _template

AIM_PATH    = $(DIST_AIM_PATH)

SHARE_OBJS  =

NEW_OBJS    = DynaNodeManage.o ThreadMutex.o NodeThreadMutex.o


#-------------------------------------------------

all: $(TARGET)

$(TARGET): build
#	$(LN) $(NEW_OBJS) $(SHARE_OBJS) ${CC_LPATH} -o $@

build: $(NEW_OBJS)


setup: $(TARGET)
	@$(CP) $(SHARE_OBJS) $(NEW_OBJS) $(AIM_PATH)

clean:
	@$(RM) $(TARGET) $(NEW_OBJS)


#-------------------------------------------------
