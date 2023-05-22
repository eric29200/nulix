LIBUTILS	= libutils
BIN		= bin
SBIN		= sbin
SH		= sh
NJOBS		= $(shell nproc)

all:
	make -j$(NJOBS) -C $(LIBUTILS)
	make -j$(NJOBS) -C $(SBIN)
	make -j$(NJOBS) -C $(BIN)
	make -j$(NJOBS) -C $(SH)

clean:
	make clean -C $(SH)
	make clean -C $(BIN)
	make clean -C $(SBIN)
	make clean -C $(LIBUTILS)
