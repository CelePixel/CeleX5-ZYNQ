#ifndef VIDEOSTREAM_H
#define VIDEOSTREAM_H
#include <QApplication>
#include <stdio.h>
#include <QDebug>
#include <fstream>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#define DATASIZE 800*1280
using namespace std;

class VideoStream
{
public:
    VideoStream();
    ~VideoStream();
    bool avWriterInit(const char* out_filename,int fps);
    void avWriter(std::vector<uchar> buffer);
    bool avWriterRelease();

private:
    AVStream *addVideoStream(AVFormatContext *oc, enum AVCodecID codec_id);

private:
    AVFormatContext *ofmt_ctx;
    AVPacket pkt;
    int frameCount;
    int videoFPS;

};

#endif // VIDEOSTREAM_H
