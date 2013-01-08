piMythclient
============

Experimental Mythclient for running on a raspberry pi
This is very experimental an under heavy development so it might not work because some part is being tested and developed.
It only works with myth version 0.26

Prerequisite: Compiled ffmpeg. Please follow guide on http://ffmpeg.org/trac/ffmpeg/wiki/How%20to%20compile%20FFmpeg%20for%20Raspberry%20Pi%20%28Raspbian%29

To compile on the pi do "make pi"

To run do:

./piMythclient -h &lt;hostname&gt;	[-p &lt;port&gt;]	-c &lt;channelnumber&gt; [-l &lt;comma separated list of logattributes to turn on&gt;] [-t &lt;language code&gt;] [-a &lt;0|1&gt;] [-v &lt;0|1&gt;] [-e &lt;0|1&gt;] [-e &lt;basename&gt;]

* -h hostname of mythbackend or ip address
* -p port of mythbackend. When not specified it will use the default
* -c myth channelnumber
* -l comma separated list of logattributes to turn on  "client,mythprotocol,demuxer,omx,client-debug,mythprotocol-debug,demuxer-debug,omx-debug"
* -t language code "e.g. dut for dutch"
* -a 0|1 "Set audio on or off. Default 1 (on)"
* -v 0|1 "Set video on or off. Default 1 (on)"
* -e 0|1 "Set audio passthrough on. Decoding is done externally. Default 1 (on)"
* -r <basename> Name of the file in the recordings folder of mythtv backend.

It will currently use hdmi as audio and video output.

* For video currently only H264 is supported. 
* Audio is working but it is decoded in software and converted to stereo in software.

Still working on trying to get it decoded in hardware.

