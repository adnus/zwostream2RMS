# zwostream2RMS

this project based on the zwostream-rtsp code.

I modified the source for streaming the zwo asi raw pictures to a v4l2loopback device.
Than RMS can grab the pictures from this video device for meteor detection.


Stream to a v4l2loopback device and grab it with RMS Meteor Capture:

./zwostream2RMS -E | ffmpeg -f rawvideo -pixel_format gray8 -vcodec rawvideo -video_size 1936x1216 -i pipe:0 -vf format=yuv420p -f v4l2 /dev/video0


This is the way
