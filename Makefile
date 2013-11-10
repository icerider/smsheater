INSTALL = install -D -m 644
ECHO = echo                                                                       
DESTDIR =                                                                         
MKDIR = mkdir -p                                                                  
FIND = find 
CHMOD = chmod
COPYLINK = cp -P
LN = ln -sf

MODEL=atmega168
PORT=/dev/ttyACM0

EXES = all

all:
	ino build -m ${MODEL}

deploy:
	ino upload -m ${MODEL} -p ${PORT}

serial:
	ino serial -p ${PORT}
