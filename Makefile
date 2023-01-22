KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= 8
MEM_SIZE	= 512M
DISK1		= hda.img
QEMU		= kvm
args		= `arg="$(filter-out $@,$(MAKECMDGOALS))" && echo $${arg:-${1}}`

run:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	sudo $(QEMU)								\
		-m $(MEM_SIZE)							\
		-serial stdio 							\
		-cdrom $(ISO) 							\
		-drive format=raw,file=$(DISK1)					\
		-netdev tap,id=nulix_net					\
		-device rtl8139,netdev=nulix_net,id=nulix_nic			\
		-object filter-dump,id=f1,netdev=nulix_net,file=./traffic.pcap

%:
	@:

port:
	./ports/install.sh $(call args)
	./scripts/create_rootfs.sh

clean:
	make clean -C kernel
	rm -f $(DISK) $(ISO)
