#!/bin/bash

case "$1" in
  Hardware)
    retval=`cat /proc/cpuinfo | grep $1 | awk '{print $3}' | head -1`
    echo "HW_${retval:=UNKNOWN}"
    ;;
  Revision)
    retval=`cat /proc/cpuinfo | grep $1 | awk '{print $3}' | sed 's/^1000//' | head -1`
    echo "REV_${retval:=UNKNOWN}"
    ;;
  *)
    echo "$1_UNKNOWN"
    ;;
esac
