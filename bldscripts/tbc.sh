#!/bin/sh
autoreconf && ./configure && make -s clean && make -s -j8
