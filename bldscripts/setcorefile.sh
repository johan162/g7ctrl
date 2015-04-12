#!/bin/sh
sudo sysctl -w kernel.core_pattern='/tmp/core_%e-%t-%s.%p'



