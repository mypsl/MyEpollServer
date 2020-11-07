CC = g++     #c++ ±‡“Î∆˜
# CC = cc         #c±‡“Î∆˜
 
CFLAGS = -c -Werror -D_DEBUG -g -O3
#MYSQL_HOME=/usr/local/mysql
MYSQL_LIBS = `mysql_config --libs`
MYSQL_INCL = `mysql_config --cflags`
 
LIBS = -lmariadbclient -lz -ldl -lpthread -lssl -lcrypto -lhiredis
#INCPATH = -I./ -I$(MYSQL_HOME)/include $(MYSQL_INCL)
INCPATH = -I./ $(MYSQL_INCL)
 
# global file
 
OBJS_PATH = ./
 
C_OBJS = myserver.o
 
TARGET = myserver
 
# for link
$(TARGET):$(C_OBJS)
    $(CC) -o $@ $(C_OBJS) $(LIBS)
    @rm -rf $(C_OBJ)
 
$(OBJS_PATH)/%.o : ./%.cpp
    $(CC) $(CFLAGS) $(INCPATH) $< -o $@
 
$(OBJS_PATH)/%.o : ./%.c
    $(CC) $(CFLAGS) $(INCPATH) $< -o $@
 
clean:
    rm -rf $(OBJS_PATH)*.o $(TARGET)