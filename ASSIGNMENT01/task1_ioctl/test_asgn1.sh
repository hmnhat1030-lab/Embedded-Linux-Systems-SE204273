#!/bin/sh
echo "=== ASSIGNMENT-01 TASK 1: ioctl Driver Test ==="
rmmod asgn1_driver 2>/dev/null
insmod asgn1_driver.ko
mknod /dev/asgn1 c 245 0 2>/dev/null
./ioctl_helper version
echo -n "Hello Echo Mode" > /dev/asgn1
echo "  Echo Mode Read: [$(cat /dev/asgn1)]"
./ioctl_helper mode 0
echo -n "Hello Kernel Mode" > /dev/asgn1
echo "  Kernel Mode Read: [$(cat /dev/asgn1)]"
./ioctl_helper stats
./ioctl_helper reset
echo "  After Reset Read: [$(cat /dev/asgn1)]"
./ioctl_helper stats
rmmod asgn1_driver
dmesg | grep asgn1 | tail -12
echo "=== TASK 1 TEST PASSED ==="
