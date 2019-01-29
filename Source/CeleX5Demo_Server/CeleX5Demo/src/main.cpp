#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include "../include/celex5/celex5.h"
#include "socketcfg.h"


//using namespace lbf;
using namespace cv;


#define PACKET_DATA_LEN (500*1024) //2000
#define SDK_PORT1 4000
#define SDK_PORT2 8888

#define DETECT_STATUS_END_NOR 0
#define DETECT_STATUS_END_WAR 1
#define DETECT_STATUS_START 2

//#define PIXELS_NUMBER    1024000

typedef struct
{
	int cmd;
	int fin;
	int len;
	unsigned char data[1024000];
} PACKET_AP;

typedef struct
{
	int cmd;
	int value;
	int value2;
} PACKET_RQ;

extern int detect_status;
extern int sub_status;
extern char cur_det_pic[128];
Mat image_dt(640,  768,  CV_8UC1);

unsigned char image_buffer[1024000];

pthread_mutex_t sensor_mtx;
//pthread_mutex_t led_status_mtx;
//pthread_mutex_t led_detail_mtx;

char upg_version[9] = "0.0.0";

#define SDK_PORT   1234


using namespace cv;

CeleX5 sensor;

PACKET_AP packet_pic, packet_det;

PACKET_RQ packet_req;

int connect_fd = 0;
int listenfd = 0;
int send_ok = 0;
void* pic_req_entry(void *para)
{
	printf("create socket and wait connect\n");
	//create listen socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0)
	{
		printf("create a new socket failed\n");
		return 0;
	}

	//bind listen socket
	struct sockaddr_in bd_addr;
	memset(&bd_addr,0,sizeof(bd_addr));
	bd_addr.sin_family = AF_INET;
	bd_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bd_addr.sin_port = htons(SDK_PORT);
	if (bind(listenfd, (struct sockaddr *)&bd_addr, sizeof(sockaddr)) < 0)
	{
		printf("bind a socket failed\n");
		return 0;
	}

	//listen listen socket
	if (listen(listenfd, 1024) < 0)
	{
		printf("listen a socket failed\n");
		return 0;
	}

	while (1)
	{
        if (connect_fd == 0)
		{
			//wait connect to listen socket
			printf("wait connect...\n");

			if ((connect_fd = accept(listenfd, (struct sockaddr*)NULL, NULL)) <= 0)
			{
				printf("accpet socket error: %s errno :%d\n", strerror(errno), errno);
				sleep(1);
				connect_fd = 0;
				continue;
			}
		}

		if (recv(connect_fd, &packet_req, sizeof(PACKET_RQ), 0))
		{
            //printf("packet cmd=%d, value=%d, value2=%d.\n", packet_req.cmd, packet_req.value, packet_req.value2);
            if (CMD_GET_PIC == packet_req.cmd)
            {
                packet_pic.cmd = 10;
                packet_pic.len = 1280 * 800;

                CeleX5::CeleX5Mode mode;
                int cmdID;
                if (sensor.isLoopModeEnabled())
                    mode = (CeleX5::CeleX5Mode)packet_req.value;
                else
                    mode = sensor.getSensorFixedMode();
                switch (mode)
                {
                case CeleX5::Full_Picture_Mode:
                    sensor.getFullPicBuffer(packet_pic.data);
                    break;

                case CeleX5::Event_Address_Only_Mode:
                    sensor.getEventPicBuffer(packet_pic.data, CeleX5::EventBinaryPic);
                    break;

                case CeleX5::Event_Intensity_Mode:
                    sensor.getEventPicBuffer(packet_pic.data, CeleX5::EventGrayPic);
                    break;

                case CeleX5::Event_Optical_Flow_Mode:
                case CeleX5::Full_Optical_Flow_S_Mode:
                case CeleX5::Full_Optical_Flow_M_Mode:
                    sensor.getOpticalFlowPicBuffer(packet_pic.data);
                    break;
                }
                //memset(packet_pic.data, 255, 1024000);
                if ((send(connect_fd, &packet_pic, packet_pic.len+sizeof(int)*3, 0)) < 0)
                {
                    printf("send mes error: %s errno : %d", strerror(errno), errno);
                    //data_remain = 0;
                    connect_fd = 0;
                    break; //return -1;
                }
            }
            else if (CMD_SET_FIXED_MODE == packet_req.cmd)
            {
                sensor.setSensorFixedMode(CeleX5::CeleX5Mode(packet_req.value));
            }
            else if (CMD_SET_BRIGHTNESS == packet_req.cmd)
            {
                sensor.setBrightness(packet_req.value);
            }
            else if (CMD_SET_CONTRAST == packet_req.cmd)
            {
                sensor.setContrast(packet_req.value);
            }
            else if (CMD_SET_THRESHOLD == packet_req.cmd)
            {
                sensor.setThreshold(packet_req.value);
            }
            else if (CMD_ENABLE_LOOP_MODE == packet_req.cmd)
            {
                sensor.setLoopModeEnabled(packet_req.value);
                if (0 == packet_req.value)
                {
                    sensor.setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
                }
                else
                {
                    sensor.setSensorLoopMode(CeleX5::Full_Picture_Mode, 1);
                    sensor.setSensorLoopMode(CeleX5::Event_Address_Only_Mode, 2);
                    sensor.setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, 3);
                }
            }
            else if (CMD_SET_LOOP_EVENT_DURATION == packet_req.cmd)
            {
                sensor.setEventDuration(packet_req.value);
            }
            else if (CMD_SET_LOOP_FULL_PIC_NUM == packet_req.cmd)
            {
                CeleX5::CeleX5Mode val2 = static_cast<CeleX5::CeleX5Mode>(packet_req.value2);
                sensor.setPictureNumber(packet_req.value, val2);
            }
            else if (CMD_SET_LOOP_S_FO_PIC_NUM == packet_req.cmd)
            {
                CeleX5::CeleX5Mode val2 = static_cast<CeleX5::CeleX5Mode>(packet_req.value2);
                sensor.setPictureNumber(packet_req.value, val2);
            }
            else if (CMD_SET_LOOP_M_FO_PIC_NUM == packet_req.cmd)
            {
                CeleX5::CeleX5Mode val2 = static_cast<CeleX5::CeleX5Mode>(packet_req.value2);
                sensor.setPictureNumber(packet_req.value, val2);
            }
            else if (CMD_SET_CLOCK == packet_req.cmd)
            {
                printf("packet cmd=%d, value=%d, value2=%d.\n", packet_req.cmd, packet_req.value, packet_req.value2);
                sensor.setClockRate(packet_req.value);
            }
            else if (10 == packet_req.cmd)
            {
                sensor.generateFPN("/opt/FPN.txt");
            }
		}
        usleep(10);
	}

	return 0;
}

#include <semaphore.h> SA_RESTART
#include <signal.h>
sem_t sem;
int value;

void catch_signal(int sign)
{
    switch (sign)
    {
    case SIGINT:
	case SIGTERM:
	case SIGABRT:
        printf("SIGINT is signaled!\n");
        //sensor.wireIn(0, 0,  (1<<20));
		//sensor.stopGetImage();
		//sensor.Close();
        //exit(1);
        //exit(0);
        break;

    case SIGIO:
        //printf("SIGIO is signaled!\n");
        sem_post(&sem);
        sem_getvalue(&sem, &value);
        //printf("SIGIO sem value = %d\n",value);
        //exit(0);
        break;
    }
}

struct sigaction s;
int main(int argc, char* argv[])
{
	printf("--------------- CeleX5Demo 2018-01-08 ---------------\n");
	int mode = 0;
	if (!sensor.openSensor(CeleX5::CeleX5_ZYNQ, CeleX5::Event_Address_Only_Mode))
	{
		printf("---------------- open sensor error!\n");
		return -1;// to do
	}
	sensor.setSensorFixedMode(CeleX5::Event_Address_Only_Mode);

	memset(&s, 0, sizeof(s));
    s.sa_handler = catch_signal;
    s.sa_flags = SA_RESTART;
    sigaction(SIGIO, &s, NULL);
    sigaction(SIGINT, &s, NULL);
	sigaction(SIGTERM, &s, NULL);
	sigaction(SIGABRT, &s, NULL);

    //sigaction(SIGINT,&s, NULL);

    int res= sem_init(&sem, 0, 0);

    if(res==-1)
    {
        printf("sem init fail.=====================================================\n");
    }

    printf("fcntl before\n");
    sensor.setfcntl();
    printf("fcntl after\n");

	//pic_req_entry(nullptr);

    pthread_t tid2;
	int err = pthread_create(&tid2, NULL, pic_req_entry, NULL);
	if (err != 0)
	{
		perror("create pic_req_entry failed.\n");
		return err;
	}

	if (sensor.getSensorData(nullptr))
	{
        printf("get pic successfully !!!!!!!!!!!!!!!!\n");

	}
	else
	{
        printf("get pic failed !!!!!!!!!!!!!!!!\n");
	}

	while (1)
	{
        sem_wait(&sem);
        int pos = sensor.getbufpos();
		sensor.getImagedata(pos);

        //sensor.getSensorData(nullptr);
        /*sensor.getEventPicBuffer(image_buffer);
        send_ok++;

        if (connect_fd == 0)
		{
			//wait connect to listen socket
			printf("wait connect...\n");

			if ((connect_fd = accept(listenfd, (struct sockaddr*)NULL, NULL)) <= 0)
			{
				printf("accpet socket error: %s errno :%d\n", strerror(errno), errno);
				sleep(1);
				connect_fd = 0;
				continue;
			}
		}

		if (recv(connect_fd, &packet_req, sizeof(PACKET_RQ), 0))
		{
            printf("packet cmd=%d, value=%d, len=%d.\n", packet_req.cmd, packet_req.value);
		}

		if (send_ok == 10)
		{
            packet_pic.cmd = 10;
            packet_pic.len = 1280 * 800;
            sensor.getEventPicBuffer(packet_pic.data);
            //memset(packet_pic.data, 255, 1024000);
            if ((send(connect_fd, &packet_pic, packet_pic.len+sizeof(int)*3, 0)) < 0)
            {
                printf("send mes error: %s errno : %d", strerror(errno), errno);
                //data_remain = 0;
                connect_fd = 0;
                break; //return -1;
            }
            send_ok = 0;
		}*/
        usleep(10);
	}
	return 0;
}
