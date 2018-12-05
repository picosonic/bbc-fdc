#!/bin/bash

# Read and interpret revision code
#   https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md

# Get revision code
revision=$(cat /proc/cpuinfo | grep "Revision" | sed 's/.*: //')

# Convert to decimal
revision=$((16#$revision))

case "$1" in
  Hardware)
    retval=$(cat /proc/cpuinfo | grep $1 | awk '{print $3}' | head -1)
    echo "HW_${retval:=UNKNOWN}"
    ;;
  Revision)
    retval=$(cat /proc/cpuinfo | grep $1 | awk '{print $3}' | sed 's/^1000//' | head -1)
    echo "REV_${retval:=UNKNOWN}"
    ;;
  RamSize)
    ramsize=$(($(($revision >> 20)) & 0x7))
    case $ramsize in
      0)
        ramsize="256"
        ;;
      1)
        ramsize="512"
        ;;
      2)
        ramsize="1024"
        ;;
      *)
        ramsize="UNKNOWN"
        ;;
    esac
    echo "RAM_$ramsize"
    ;;
  Manufacturer)
    manufacturer=$(($(($revision >> 16)) & 0xf))
    case $manufacturer in
      0)
        manufacturer="SONY_UK"
        ;;
      1)
        manufacturer="EGOMAN"
        ;;
      2)
        manufacturer="EMBEST"
        ;;
      3)
        manufacturer="SONY_JAPAN"
        ;;
      4)
        manufacturer="EMBEST"
        ;;
      5)
        manufacturer="STADIUM"
        ;;
      *)
        manufacturer="UNKNOWN"
        ;;
    esac
    echo "MAN_$manufacturer"
    ;;
  CPU)
    processor=$(($(($revision >> 12)) & 0xf))
    case $processor in
      0)
        processor="BCM2835"
        ;;
      1)
        processor="BCM2836"
        ;;
      2)
        processor="BCM2837"
        ;;
      *)
        processor="UNKNOWN"
        ;;
    esac
    echo "CPU_${processor}"
    ;;
  BoardType)
    boardtype=$(($(($revision >> 4)) & 0xff))
    case $boardtype in
      0)
        boardtype="A"
        ;;
      1)
        boardtype="B"
        ;;
      2)
        boardtype="A_PLUS"
        ;;
      3)
        boardtype="B_PLUS"
        ;;
      4)
        boardtype="2B"
        ;;
      5)
        boardtype="ALPHA"
        ;;
      6)
        boardtype="CM1"
        ;;
      8)
        boardtype="3B"
        ;;
      9)
        boardtype="ZERO"
        ;;
      10)
        boardtype="CM3"
        ;;
      12)
        boardtype="ZERO_W"
        ;;
      13)
        boardtype="3B_PLUS"
        ;;
      14)
        boardtype="3A_PLUS"
        ;;
      *)
        boardtype="UNKNOWN"
        ;;
    esac
    echo "BOARD_${boardtype}"
    ;;
  BoardRev)
    boardrev=$(($revision & 0xf))
    echo "BOARDREV_${boardrev}"
    ;;
  *)
    echo "$1_UNKNOWN"
    ;;
esac
