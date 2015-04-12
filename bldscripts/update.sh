#!/bin/sh
make -s -j8
_RC=$?
if [ $_RC -eq 0 ]; then
  sudo systemctl stop g7ctrl.service
  #sudo rm -f /usr/share/g7ctrl/g7ctrl.log
  sudo make -s install
  sudo systemctl --system daemon-reload
  sudo systemctl start g7ctrl.service
fi
