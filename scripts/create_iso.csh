#!/bin/csh

set ISO			= nulix.iso

grub-mkrescue -o $ISO iso
