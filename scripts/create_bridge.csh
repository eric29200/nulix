#!/bin/csh

brctl show br0
if ($? == 1) then
	sudo brctl addbr br0
endif
