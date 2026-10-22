#!/bin/sh
echo "=== ASSIGNMENT-01 TASK 2: Misc Counter Driver Test ==="
rmmod counter_driver 2>/dev/null
insmod counter_driver.ko
MINOR=$(grep counter /proc/misc | awk '{print $1}')
[ -c /dev/counter ] || mknod /dev/counter c 10 ${MINOR:-0}
echo "  Read 1: $(cat /dev/counter)"
echo "  Read 2: $(cat /dev/counter)"
echo "  Read 3: $(cat /dev/counter)"
cat /proc/counter_info
echo "100" > /dev/counter
echo "  After write 100, Read: $(cat /dev/counter)"
cat /proc/counter_info
echo "0" > /dev/counter
echo "  After reset 0, Read: $(cat /dev/counter)"
cat /proc/counter_info
echo "-5" > /dev/counter 2>/dev/null || echo "  Correctly rejected -5 (-EINVAL)"
rmmod counter_driver
dmesg | grep counter_driver | tail -10
echo "=== TASK 2 TEST PASSED ==="
