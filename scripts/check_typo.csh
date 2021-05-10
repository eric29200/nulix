#!/bin/csh

find kernel/ -name '*.c' -exec grep -lP '\t' {} \;
find kernel/ -name '*.h' -exec grep -lP '\t' {} \;
find usr/ -name '*.c' -exec grep -lP '\t' {} \;
find usr/ -name '*.h' -exec grep -lP '\t' {} \;
