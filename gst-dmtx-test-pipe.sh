#!/bin/sh
gst-launch --gst-plugin-path=`pwd`/src/.libs v4l2src ! video/x-raw-rgb,width=320, height=240 ! dmtx ! ffmpegcolorspace ! xvimagesink
