#!/bin/sh
#Upgrade script to install a new vesion of the g7ctrl daemon
sudo /etc/init.d/g7ctrl stop
sleep 1
sudo mv /usr/share/g7ctrl/g7ctrl.log /usr/share/g7ctrl/g7ctrl.log.OLD
make -j8
sudo make install
sudo /etc/init.d/g7ctrl start

