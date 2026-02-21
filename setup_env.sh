#!/bin/bash

# Stop tty from rendering to screen
echo 0 > /sys/class/vtconsole/vtcon1/bind
# Stop GUI svc
# This change can be permanent with systemctl set-default multi-user.target
systemctl stop lightdm
