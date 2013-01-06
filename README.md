piMythclient
============

Experimental Mythclient for running on a raspberry pi
This is very experimental an under heavy development so it might not work because some part is being tested and developed.
It only works with myth version 0.26

Prerequisite: Compiled ffmpeg. Please follow guide on http://ffmpeg.org/trac/ffmpeg/wiki/How%20to%20compile%20FFmpeg%20for%20Raspberry%20Pi%20%28Raspbian%29

To compile on the pi do "make pi"

To run do:

./piMythclient -h <hostname>	[-p <port>]	-c <channelnumber> [-l <comma separated list of logattributes to turn on>] [-t <language code>] [-a <0|1>] [-v <0|1>] [-e <0|1>]

  -h <hostname of mythbackend or ip address>
	-p <port of mythbackend. When not specified it will use the default>
	-c <myth channelnumber>
	-l <comma separated list of logattributes to turn on>  "client, mythprotocol, demuxer, omx, client-debug, mythprotocol-debug, demuxer-debug, omx-debug"
	-t <language code> "e.g. dut for dutch"
	-a <0|1> "Set audio on or off. Default 1 (on)"
	-v <0|1> "Set video on or off. Default 1 (on)"
	-e <0|1> "Set audio passthrough on. Decoding is done externally. Default 1 (on)"

It will currently use hdmi as audio and video output.
Audio is currently not yet working.
