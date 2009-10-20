#!/bin/sh

case "$1" in
video)
gst-launch --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! video/x-raw-rgb,width=320, height=240,fps=10 ! \
	dmtx scale=2 ! \
	queue ! ffmpegcolorspace ! xvimagesink
;;
png)
gst-launch --gst-plugin-path=`pwd`/src/.libs filesrc location=test/test.png ! pngdec ! dmtx ! fakesink
;;
*)
echo "Usage: $0 png|video"
;;
esac
