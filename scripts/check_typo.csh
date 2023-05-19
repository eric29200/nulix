#!/bin/bash

find kernel/ -name '*.c' -exec grep -lP '\t$' {} \;
find libc/ -name '*.c' -exec grep -lP '\t$' {} \;
find usr/ -name '*.c' -exec grep -lP '\t$' {} \;

find kernel/ -name '*.h' -exec grep -lP '\t$' {} \;
find libc/ -name '*.h' -exec grep -lP '\t$' {} \;
find usr/ -name '*.h' -exec grep -lP '\t$' {} \;
