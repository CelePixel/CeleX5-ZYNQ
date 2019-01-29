#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/reboot.h>
#include <sys/stat.h>

#include <opencv2/core/mat.hpp>
#include <sys/vfs.h>


//using namespace lbf;
using namespace cv;

using namespace std;



extern bool sensor_get_image(Mat* image, Mat* evt, int *o_av_als, int *o_als, int *o_bright);

int monitor_entry()
{

	printf("monitor_entry\n");


	while(1)
	{

		bool b = sensor_get_image(&image, &event, &av_als, &als, &birghtness);
        usleep(70000);
	}

}


