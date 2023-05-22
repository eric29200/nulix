LIBUTILS	= libutils
BIN		= bin
SBIN		= sbin
NJOBS		= $(shell nproc)

all:
	make -j$(NJOBS) -C $(LIBUTILS)
	make -j$(NJOBS) -C $(SBIN)
	make -j$(NJOBS) -C $(BIN)

clean:
	make clean -C $(BIN)
	make clean -C $(SBIN)
	make clean -C $(LIBUTILS)
