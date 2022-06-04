#!/bin/sh

xxd -i core.agate |
  sed 's/unsigned int core_agate_len/\/\/ length:/' |
  sed 's/core_agate/AgateCore/' |
  sed 's/unsigned/static/' |
  sed 's/0x0a$/0x0a, 0x00/' > core.inc
