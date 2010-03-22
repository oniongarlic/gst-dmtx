#!/bin/sh

case "$1" in
video)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! video/x-raw-rgb,width=320,height=240 ! \
	queue ! dmtx scale=1 timeout=50 skip=1 ! \
	queue ! ffmpegcolorspace ! xvimagesink
;;
video-f)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! queue ! dmtx scale=2 skip=1 ! \
	queue ! ffmpegcolorspace ! xvimagesink
;;
video-eos)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! video/x-raw-rgb,width=320,height=240 ! \
	queue leaky=2 ! dmtx scale=2 stop_after=1 timeout=40 ! \
	ffmpegcolorspace ! xvimagesink
;;
video-tee)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! ffmpegcolorspace !video/x-raw-rgb,width=320,height=240 ! \
	tee name=t ! queue ! ffmpegcolorspace ! \
	dmtx scale=2 timeout=100 skip=30 skip_dups=TRUE ! fakesink t. ! queue ! ffmpegcolorspace ! xvimagesink
;;
video-16)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! ffmpegcolorspace ! video/x-raw-rgb,width=320,height=240 ! \
	tee name=t ! queue ! ffmpegcolorspace ! video/x-raw-rgb,width=320,height=240,bpp=16 ! \
	dmtx scale=2 timeout=100 skip=30 skip_dups=TRUE ! fakesink t. ! queue ! ffmpegcolorspace ! xvimagesink
;;
video-8)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs \
	v4l2src ! ffmpegcolorspace !video/x-raw-rgb,width=640,height=480 ! \
	tee name=t ! queue ! ffmpegcolorspace ! video/x-raw-gray,width=640,height=480,bpp=8 ! \
	dmtx scale=1 timeout=250 skip=15 skip_dups=TRUE qos=TRUE ! fakesink t. ! queue ! ffmpegcolorspace ! xvimagesink
;;
png)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs filesrc location=test/test.png ! pngdec ! dmtx skip=0 ! fakesink
;;
png-mosaic)
gst-launch -m --gst-plugin-path=`pwd`/src/.libs filesrc location=test/test-mosaic.png ! pngdec ! dmtx skip=0 type=1 ! fakesink
;;
*)
echo "Usage: $0 png|video|video-eos"
;;
esac
