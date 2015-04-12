#!/bin/sh
gdb --batch --quiet -ex "thread apply all bt full" -ex "quit" src/g7ctrl $1 
