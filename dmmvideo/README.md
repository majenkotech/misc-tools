dmmvideo
========

Connect to a Uni-T UT61E DMM using `sigrok-cli` and render a virtual
display as a `v4l2loopback` virtual video device.

Display is 512x256 pixels and rendered in a red-on-black LED display
style.

Requires:
--------

* Sigrok (sigrok-cli)
* V4L2Loopback
* FreeSansBold.ttf
* TickingTimebombBB.ttf

Usage:
-----

```
$ dmmvideo -d /dev/video2
```

