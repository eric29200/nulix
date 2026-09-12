#!/bin/bash

echo "********** <tab>$ **********"
find kernel/ -name '*.c' -exec grep -lP '\t$' {} \;
find kernel/ -name '*.h' -exec grep -lP '\t$' {} \;

echo "********** <space>$ **********"
find kernel/ -name '*.c' -exec grep -lP ' $' {} \;
find kernel/ -name '*.h' -exec grep -lP ' $' {} \;

echo "********** if( **********"
find kernel/ -name '*.c' -exec grep -lP '\tif\(' {} \;
find kernel/ -name '*.h' -exec grep -lP '\tif\(' {} \;

echo "********** for( **********"
find kernel/ -name '*.c' -exec grep -lP 'for\(' {} \;
find kernel/ -name '*.h' -exec grep -lP 'for\(' {} \;

echo "********** while( **********"
find kernel/ -name '*.c' -exec grep -lP 'while\(' {} \;
find kernel/ -name '*.h' -exec grep -lP 'while\(' {} \;
