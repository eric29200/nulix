#!/bin/bash

find kernel/ -name '*.c' -exec grep -lP '\t$' {} \;

find kernel/ -name '*.h' -exec grep -lP '\t$' {} \;
