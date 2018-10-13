#!/bin/sh

if [ -d "/home/chef/Source/NetBSD" ]; then
cd /home/chef/Source/NetBSD && echo "[Updating] NetBSD ..." && git pull
fi
if [ -d "/home/chef/Source/core" ]; then
cd /home/chef/Source/core && echo "[Updating] wmutils core ..." && git pull 
fi
if [ -d "/home/chef/Source/opt" ]; then
cd /home/chef/Source/opt && echo "[Updating] wmutils opt ..." && git pull
fi
if [ -d "/home/chef/Source/libwm" ]; then
cd /home/chef/Source/libwm && echo "[Updating] wmutils lib ..." && git pull
fi
if [ -d "/home/chef/Source/bar" ]; then
cd /home/chef/Source/bar && echo "[Updating] lemonbar ..." && git pull 
fi
if [ -d "/home/chef/Source/sxhkd" ]; then
cd /home/chef/Source/sxhkd && echo "[Updating] shxkd ..." && git pull
fi 
echo "[All] githubs updated ..."
