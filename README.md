# zwostream2RMS

This project is based on the zwostream repositories from Eddy Daoud and Asgaut Eng.

I modified the source for streaming the zwo asi raw pictures to a v4l2loopback device.
Than RMS can grab the pictures from this video device for meteor detection.

Stream to a v4l2loopback device and grab it with RMS Meteor Capture:

## requirements

Linux Mint (or similar), a mini pc. we have a Zotac Nano with 4 cores. 

The ZWO ASI SDK is installed in the directory above, so the libs and headers
are in ../lib/x64/ and ../include/.

### opencv4

### v4l2loopback

```
sudo apt-get install v4l2loopback-dkms v4l2loopback-utils
sudo modprobe v4l2loopback devices=2
```


## day 7, break through !!

### autofocus, best for snap a single image every minute

```
# stream the ASI174MM (cam nr 0) to /dev/video0 (v4l2loopback device)
./zwostream2RMS -E -n0 | ffmpeg -f rawvideo -pixel_format gray8 -vcodec rawvideo -video_size 1936x1216 -i pipe:0 -vf format=yuv420p -f v4l2 /dev/video0
# and snap a picture from the stream
ffmpeg -f v4l2 -i /dev/video0 -frames:v 1 output.jpeg

# stream the ASI178MC (cam nr 1) to /dev/video1 (v4l2loopback device)
/zwostream2RMS -E -n1 | ffmpeg -f rawvideo -pixel_format bayer_rggb8 -vcodec rawvideo -video_size 3096x2080 -i pipe:0 -vf format=yuv420p -f v4l2 /dev/video1
# snap a picture from the stream:
ffmpeg -f v4l2 -i /dev/video1 -frames:v 1 output.jpeg

```

Problem: the autofocus modus of the ASI SDK raises timeout errors (error 11) while video capture. So you will loose pictures  in the final stream!

### fixed exposure for hunting meteors in the night

```

./zwostream2RMS -e100 -g200 | ffmpeg -f rawvideo -pixel_format gray8 -vcodec rawvideo -video_size 1936x1216 -i pipe:0 -vf format=yuv420p -f v4l2 /dev/video0

```

and then use RMS to capture the video stream from '0', see RMS config file.

\-g200 is maybe not the best value, I'm testing it. -e100 should be 0.1sec, so you get 10 pictures per second. This is a theory.

#### This is the way!

