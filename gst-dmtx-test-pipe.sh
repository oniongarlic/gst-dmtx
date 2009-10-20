#!/bin/sh

case "$1" in
video)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! video/x-raw-rgb,width=320, height=240,fps=10 ! \
	dmtx scale=2 timeout=20 ! \
	queue ! ffmpegcolorspace ! xvimagesink
;;
video-eos)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! video/x-raw-rgb,width=320, height=240,fps=2 ! \
	dmtx scale=3 stop_after=1 timeout=10 ! \
	queue ! ffmpegcolorspace ! xvimagesink
;;
png)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs filesrc location=test/test.png ! pngdec ! dmtx ! fakesink
;;
*)
echo "Usage: $0 png|video|video-eos"
;;
esac
