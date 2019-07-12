
#!/bin/sh

#mount /dev/mmcblk0p3 /opt/submission -t ext4
#mount /dev/mmcblk0p4 /opt/routine -t ext4

ifconfig eth0 192.168.1.11 netmask 255.255.255.0

echo -----192.168.1.11-----

cd /opt

echo -----startup.sh-----

./CeleX5Server

#/etc/init.d/S50sshd start
