#! /bin/sh
# usually /dev/sd0e or sd4e on my machines. 
# sysctl hw.disknames
# gpt show sd0

sudo mount_msdos /dev/sd0e /mnt/usb
