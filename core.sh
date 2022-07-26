#!/bin/sh

if which xembed > /dev/null
then
  xembed core.agate core.inc AgateCore
else
  echo "Install xembed. See https://github.com/agate-lang/xembed."
fi
