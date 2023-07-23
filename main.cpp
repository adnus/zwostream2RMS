
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <errno.h>
#include <signal.h>
#include <unistd.h> // for isatty
#include <stdarg.h>

#define DISABLE_OPENCV_24_COMPATIBILITY
#include <opencv4/opencv2/imgproc/imgproc.hpp>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>	   // Basic OpenCV structures (cv::Mat)
#include <opencv4/opencv2/videoio.hpp> // Video write

#include "ASICamera2.h"

using namespace std;

// Format: https://en.cppreference.com/w/c/io/fprintf
void imgPrintf(cv::InputOutputArray img, const char *format, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	cv::putText(
		img,
		buffer,
		cv::Point(10, 40),
		cv::FONT_HERSHEY_SIMPLEX,
		0.7,
		cv::Scalar(0, 0, 0),
		3);
	cv::putText(
		img,
		buffer,
		cv::Point(10, 40),
		cv::FONT_HERSHEY_SIMPLEX,
		0.7,
		cv::Scalar(200, 200, 200),
		1);
}

// Macros for operating on timeval structures are described in
// http://linux.die.net/man/3/timeradd
int64_t get_highres_time()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) == -1)
	{
		fprintf(stderr, "clock_gettime: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return (int64_t)(t.tv_sec) * 1000 + (int64_t)(t.tv_nsec) / 1000000;
}

bool exit_mainloop;

static void sigint_handler(int sig, siginfo_t *si, void *unused)
{
	struct sigaction sa;

	fprintf(stderr, "Caught %s.\n", sig == SIGINT ? "SIGINT" : "SIGTERM");
	// Remove the handler
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = 0;
	sa.sa_flags = SA_RESETHAND;
	if (sigaction(sig, &sa, NULL) == -1)
	{
		fprintf(stderr, "sigaction: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// Break the main loop
	exit_mainloop = true;
}

static void install_sigint_handler(void)
{
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = sigint_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		fprintf(stderr, "sigaction: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1)
	{
		fprintf(stderr, "sigaction: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

struct options
{
	bool verbose;
	long camNumber;
	long exposure_ms;
	bool gain_auto;
	bool exposure_auto;
	long gain;
	long fps;
	long maxGain;
	long maxExp;
	bool cvwrite;
	ASI_IMG_TYPE asi_image_type;
	int cv_array_type;
};

void parse_command_line(int argc, char **argv, options *dest)
{
	int optc;

	// Set defaults
	dest->camNumber =0;
	dest->exposure_ms = 0;
	dest->exposure_auto = false;
	dest->gain_auto = false;
	dest->gain = 50;
	dest->asi_image_type = ASI_IMG_RAW8;
	dest->cv_array_type = CV_8UC1;
	dest->verbose = false;
	dest->cvwrite = false;
	dest->fps = 10;
	dest->maxGain = 100;
	dest->maxExp = 100;

	while ((optc = getopt(argc, argv, "n:h:Ee:Gg:p:v:f:Mm:c")) != -1)
	{
		switch (optc)
		{
		case 'M':
			dest->maxGain = atoi(optarg);
			break;
		case 'm':
			dest->maxExp = atoi(optarg);
			break;
		case 'n':
			dest->camNumber = atoi(optarg);
			break;
		case 'e':
			dest->exposure_ms = atoi(optarg);
			break;
		case 'E':
			dest->exposure_auto = true;
			break;
		case 'G':
			dest->gain_auto = true;
			break;
		case 'c':
			dest->cvwrite = true;
			break;
		case 'g':
			dest->gain = atoi(optarg);
			break;
		case 'f':
			dest->fps = atoi(optarg);
			break;
		case 'p':
			if (strcasecmp(optarg, "RAW8") == 0)
			{
				dest->asi_image_type = ASI_IMG_RAW8;
				dest->cv_array_type = CV_8UC1;
			}
			else if (strcasecmp(optarg, "RAW16") == 0)
			{
				dest->asi_image_type = ASI_IMG_RAW16;
				dest->cv_array_type = CV_16UC1;
			}
			else
			{
				fprintf(stderr, "invalid argument for -p option: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			dest->verbose = true;
			break;
		case 'h':
		case '?':
			printf("Usage: %s\n"
			
				   "  -c Enable write Text to Image (default off)\n"	   
				   "  -n Number of the Camera (default 0)\n"
				   "  -E Enable auto exposure (default off)\n"
				   "  -e {exposure time} Set exposure time in ms (default 500 ms)\n"
				   "  -G Enable auto gain (default off)\n"
				   "  -m Select max exposure in ms(default 100)\n"
				   "  -M Select max gain in (default 100)\n"
				   "  -f Select FPS(default 10)\n"
				   "  -g {gain (0-100)} Set gain or the initial gain if auto gain is enabled (default 50)\n"
				   "  -p {RAW8 | RAW16} Set pixel format for the camera and generated output (default RAW8)\n"
				   "  -h Print this help\n",
				   argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	bool bresult;
	int CamIndex = 0;
	options opt;

	parse_command_line(argc, argv, &opt);
	install_sigint_handler();

	int numDevices = ASIGetNumOfConnectedCameras();

	if (numDevices <= 0)
	{
		fprintf(stderr, "no camera connected\n");
		exit(EXIT_FAILURE);
	}

	ASI_CAMERA_INFO CamInfo;
	fprintf(stderr, "Attached cameras:\n");

	for (i = 0; i < numDevices; i++)
	{
		ASIGetCameraProperty(&CamInfo, i);
		fprintf(stderr, "\t%d: %s\n", i, CamInfo.Name);
	}

	CamIndex = opt.camNumber;

	ASIGetCameraProperty(&CamInfo, CamIndex);
	bresult = ASIOpenCamera(CamInfo.CameraID);
	bresult += ASIInitCamera(CamInfo.CameraID);

	if (bresult)
	{
		fprintf(stderr, "OpenCamera error, are you root?\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "%s information\n", CamInfo.Name);
	fprintf(stderr, "\tResolution: %ldx%ld\n", CamInfo.MaxWidth, CamInfo.MaxHeight);

	if (CamInfo.IsColorCam)
	{
		const char *bayer[] = {"RG", "BG", "GR", "GB"};
		fprintf(stderr, "\tColor Camera: bayer pattern: %s\n", bayer[CamInfo.BayerPattern]);
	}
	else
		fprintf(stderr, "\tMono camera\n");

	int ctrlnum;
	ASIGetNumOfControls(CamInfo.CameraID, &ctrlnum);
	ASI_CONTROL_CAPS ctrlcap;
	for (i = 0; i < ctrlnum; i++)
	{
		ASIGetControlCaps(CamInfo.CameraID, i, &ctrlcap);
		fprintf(stderr, "\t%s '%s' [%ld,%ld] %s\n", ctrlcap.Name, ctrlcap.Description,
				ctrlcap.MinValue, ctrlcap.MaxValue,
				ctrlcap.IsAutoSupported ? "(Auto supported)" : "(Manual only)");
	}

	int fps = opt.fps;
	unsigned fCount = 0;
	long exp = opt.exposure_ms * 1000;
	long gain = opt.gain;
	long maxExp = opt.maxExp;
	long maxGain = opt.maxGain;
	long sensorTemp;

	// const string args = "appsrc ! videoconvert ! vaapih264enc rate-control=cbr bitrate=3000 quality-level=4 keyframe-period=30 num-slices=1 refs=1 max-bframes=2 ! rtspclientsink location=rtsp://localhost:8554/live.sdp";
      const string args = "appsrc ! videoconvert ! video/x-raw,format=BGRx ! identity drop-allocation=true ! v4l2sink device=/dev/video0";

	ASISetControlValue(CamInfo.CameraID, ASI_EXPOSURE, exp, opt.exposure_auto ? ASI_TRUE : ASI_FALSE);
	ASISetControlValue(CamInfo.CameraID, ASI_GAIN, gain, opt.gain_auto ? ASI_TRUE : ASI_FALSE);
	ASISetControlValue(CamInfo.CameraID, ASI_AUTO_MAX_GAIN, maxGain, ASI_TRUE);
	ASISetControlValue(CamInfo.CameraID, ASI_AUTO_MAX_EXP, maxExp, ASI_TRUE);
	ASISetControlValue(CamInfo.CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_TRUE);

	if (CamInfo.IsTriggerCam)
	{
		ASI_CAMERA_MODE mode;
		// Multi mode camera, need to select the camera mode
		ASISetCameraMode(CamInfo.CameraID, ASI_MODE_NORMAL);
		ASIGetCameraMode(CamInfo.CameraID, &mode);
		if (mode != ASI_MODE_NORMAL)
			fprintf(stderr, "Set mode failed!\n");
	}

        if (isatty(fileno(stdout))) {
		fprintf(stderr, "stdout is a tty, will not dump video data\n");
		exit_mainloop = true;
	}
	

	ASISetROIFormat(CamInfo.CameraID, CamInfo.MaxWidth, CamInfo.MaxHeight, 1, opt.asi_image_type);
	cv::Mat img(CamInfo.MaxHeight, CamInfo.MaxWidth, opt.cv_array_type);

	//cv::VideoWriter videoWriter(args, 0, fps, cv::Size(CamInfo.MaxWidth, CamInfo.MaxHeight), false);

	ASIStartVideoCapture(CamInfo.CameraID);
	while (!exit_mainloop)
	{
		ASI_ERROR_CODE code;
		ASI_BOOL bVal;

		code = ASIGetVideoData(CamInfo.CameraID, img.data, img.elemSize() * img.size().area(), 2000);
		if (code != ASI_SUCCESS)
		{
			fprintf(stderr, "ASIGetVideoData() error: %d\n", code);
		}
		else
		{
			fCount++;
			ASIGetControlValue(CamInfo.CameraID, ASI_GAIN, &gain, &bVal);
			ASIGetControlValue(CamInfo.CameraID, ASI_EXPOSURE, &exp, &bVal);
			ASIGetControlValue(CamInfo.CameraID, ASI_TEMPERATURE, &sensorTemp, &bVal);

			char timestamp[20] = {0};
			struct tm *tm;
			time_t t = time(NULL);

			ostringstream sstream;

			// format the exposure duration
			if (exp < 1000)
				sstream << exp << "us";
			else
				sstream << exp / 1000 << "ms";

			string exposure = sstream.str();

			tm = localtime(&t);
			strftime(timestamp, sizeof(timestamp), "%d%m%y %H:%M:%S", tm);

			// add info text overlay to the image
			// make no sense for stream to RMS
			if (opt.cvwrite) {
			imgPrintf(
				img,
				"%s Gain:%ld Exp:%s Frame:%lu Temp:%.0fC",
				timestamp, gain, exposure.c_str(), fCount, sensorTemp / 10.0);
                        }
			// write the frame to the rstp server
			//videoWriter.write(img);
			fwrite(img.data, img.elemSize(), img.size().area(), stdout);
		        fflush(stdout);
			// sleep the loop so it matches the fps of the output
			sleep(1 / fps);
		}
	}

	// release CV VideoWriter
	//videoWriter.release();

	// Close the camera
	ASIStopVideoCapture(CamInfo.CameraID);
	ASICloseCamera(CamInfo.CameraID);

	return 0;
}
