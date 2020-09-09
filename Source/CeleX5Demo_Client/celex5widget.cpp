#include "celex5widget.h"
#include <QTimer>
#include <QFileDialog>
#include <QCoreApplication>
#include <QPainter>
#include <QPushButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#ifdef _WIN32
#include <Windows.h>
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define READ_BIN_TIMER    10

QTime runningTime;
VideoStream*  m_pVideoStream = new VideoStream;
QTimer*  m_pSaveImageTimer;
QStringList filePathList;
std::ofstream g_ofWriteMat;
bool g_bDataAligned = false;

SensorDataObserver::SensorDataObserver(CX5SensorDataServer *sensorData, QWidget *parent)
    : QWidget(parent)
    #ifdef TRANSMIT_RESIZED_PIC
    , m_imageMode1(CELEX5_COL_RESIZED, CELEX5_ROW_RESIZED, QImage::Format_RGB888)
    , m_imageMode2(CELEX5_COL_RESIZED, CELEX5_ROW_RESIZED, QImage::Format_RGB888)
    , m_imageMode3(CELEX5_COL_RESIZED, CELEX5_ROW_RESIZED, QImage::Format_RGB888)
    #else
    , m_imageMode1(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode2(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode3(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    #endif
    , m_pCeleX5(NULL)
    , m_bLoopModeEnabled(false)
    , m_bRecordVideoEnabled(false)
    , m_iPicMode(0)
    , m_bRealtimeDisplay(true)
    , m_bShowMultipleWindows(false)
    , m_uiTemperature(0)
    , m_iPlaybackFPS(30)
    , m_uiFullFrameFPS(40)
    , m_uiRealFullFrameFPS(0)
    , m_emSensorMode(CeleX5::Full_Picture_Mode)
    , m_iFPS(30)
    , m_bVideoColorEnable(false)
    , m_bCurrFileFinished(false)
{
    m_pSensorData = sensorData;
    m_pSensorData->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

    m_imageMode1.fill(Qt::black);
    m_imageMode2.fill(Qt::black);
    m_imageMode3.fill(Qt::black);

    m_pUpdateTimer = new QTimer(this);
    m_pSaveImageTimer = new QTimer(this);

    connect(m_pSaveImageTimer,SIGNAL(timeout()),this,SLOT(onSaveImage2Video()));

    m_vecEncodeparam = std::vector<int>(2);
    m_vecEncodeparam[0] = CV_IMWRITE_JPEG_QUALITY;
    m_vecEncodeparam[1] = 50;

    //    connect(m_pUpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdateImage()));
    //m_pUpdateTimer->start(33);

    for (int i = 0; i < 3; i++)
        m_pBuffer[i] = new uchar[CELEX5_PIXELS_NUMBER];

}

SensorDataObserver::~SensorDataObserver()
{
    m_pSensorData->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    for (int i = 0; i < 3; i++)
    {
        delete[] m_pBuffer[i];
        m_pBuffer[i] = NULL;
    }
}

void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
    //    cout << "SensorDataObserver::onFrameDataUpdated" << endl;
    //    if (!m_pCeleX5->isLoopModeEnabled() && m_bRealtimeDisplay)
    //    {
    //        return;
    //    }
    m_uiRealFullFrameFPS = m_pCeleX5->getFullFrameFPS();
    //m_uiTemperature = pSensorData->getTemperature();
    unsigned char* pBuffer = NULL;
    //cout << "mode = " << (int)pSensorData->getSensorMode() << ", loopNum = " << pSensorData->getLoopNum()<< endl;
    CeleX5::CeleX5Mode mode = pSensorData->getSensorMode(); //CeleX5::Event_Address_Only_Mode;
    if (mode == CeleX5::Event_Address_Only_Mode)
    {
        pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
    }
    else if (mode == CeleX5::Event_Optical_Flow_Mode)
    {
        pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
    }
    else if (mode == CeleX5::Event_Intensity_Mode)
    {
        if (m_bShowMultipleWindows)
        {
            m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventGrayPic);
            m_pCeleX5->getEventPicBuffer(m_pBuffer[2], CeleX5::EventAccumulatedPic);
            updateImage(m_pBuffer[1], mode, 2, 0);
            updateImage(m_pBuffer[2], mode, 3, 0);
        }
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventGrayPic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventAccumulatedPic);
    }
    else if (mode == CeleX5::Full_Picture_Mode)
    {
        pBuffer = pSensorData->getFullPicBuffer();
    }
    else
    {
        if (m_bShowMultipleWindows)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Pic);
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[1], CeleX5::Full_Optical_Flow_Speed_Pic);
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[2], CeleX5::Full_Optical_Flow_Direction_Pic);
            updateImage(m_pBuffer[1], mode, 2, 1);
            updateImage(m_pBuffer[2], mode, 3, 2);
        }
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Speed_Pic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Direction_Pic);
    }

    cv::Mat mat;
    if (m_bRecordVideoEnabled)
    {
        mat = cv::Mat(800,1280,CV_8UC1,pBuffer);

        if (m_bVideoColorEnable)
        {
            if(mode == CeleX5::Full_Picture_Mode || mode == CeleX5::Event_Address_Only_Mode || mode == CeleX5::Event_Intensity_Mode)
                m_queueForSaving.push(convertTo3Channels(mat));
            else
                m_queueForSaving.push(convertToColored(mat));
        }
        else
        {
            m_queueForSaving.push(mat);
        }
    }
    updateImage(pBuffer, mode, pSensorData->getLoopNum(), m_iPicMode);

    if (m_emDisplayType == ConvertBin2CSV)
    {
        writeCSVData(pSensorData->getSensorMode());
    }

    if (!m_bRealtimeDisplay)
    {
        if (1000 / m_iPlaybackFPS > 1)
        {
#ifdef _WIN32
            Sleep(1000 / m_iPlaybackFPS);
#else
#endif
        }
    }
}

void SensorDataObserver::setCeleX5(CeleX5 *pCeleX5)
{
    m_pCeleX5 = pCeleX5;
}

void SensorDataObserver::setLoopModeEnabled(bool enable)
{
    m_bLoopModeEnabled = enable;
}

void SensorDataObserver::setRecordVideoEnabled(bool enable)
{
    m_bRecordVideoEnabled = enable;
}

void SensorDataObserver::setPictureMode(int picMode)
{
    m_iPicMode = picMode;
}

void SensorDataObserver::setFPS(int count)
{
    m_iFPS = count;
    m_pUpdateTimer->start(1000/m_iFPS);
}

void SensorDataObserver::setPlaybackFPS(int count)
{
    m_iPlaybackFPS = count;
}

void SensorDataObserver::setFullFrameFPS(uint16_t value)
{
    m_uiFullFrameFPS = value;
}

void SensorDataObserver::setMultipleShowEnabled(bool enable)
{
    m_bShowMultipleWindows = enable;
}

void SensorDataObserver::setDisplayType(DisplayType type)
{
    m_emDisplayType = type;
    if (Realtime == type)
    {
        m_pUpdateTimer->start(1000/m_iFPS);
    }
    else
    {
        m_pUpdateTimer->stop();
    }
}

void SensorDataObserver::setPlaybackEnabled(bool enable)
{
    m_bRealtimeDisplay = !enable;
    if (enable)
        m_pUpdateTimer->stop();
    else
        m_pUpdateTimer->start(1000/m_iFPS);
}

void SensorDataObserver::setSensorMode(CeleX5::CeleX5Mode mode)
{
    m_emSensorMode = mode;
}

void SensorDataObserver::setVideoColorEnable(bool enable)
{
    m_bVideoColorEnable = enable;
}

void SensorDataObserver::setSwitch2NextFileState(bool state)
{
    m_bCurrFileFinished = state;
}

void SensorDataObserver::writeCSVData(CeleX5::CeleX5Mode sensorMode)
{
    std::vector<EventData> vecEvent;
    m_pCeleX5->getEventDataVector(vecEvent);
    size_t dataSize = vecEvent.size();
    if (sensorMode == CeleX5::Event_Address_Only_Mode)
    {
        for (int i = 0; i < dataSize; i++)
        {
            if (!g_bDataAligned)
            {
                if (vecEvent[i].row != 0)
                    g_bDataAligned = true;
            }
            else
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].t << endl;
            }
        }
    }
    else if (sensorMode == CeleX5::Event_Intensity_Mode)
    {
        for (int i = 0; i < dataSize; i++)
        {
            if (!g_bDataAligned)
            {
                if (vecEvent[i].row != 0)
                    g_bDataAligned = true;
            }
            else
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].brightness
                             << ',' << vecEvent[i].polarity << ',' <<  vecEvent[i].t << endl;
            }
        }
    }
    else if (sensorMode == CeleX5::Event_Optical_Flow_Mode)
    {
        for (int i = 0; i < dataSize; i++)
        {
            if (!g_bDataAligned)
            {
                if (vecEvent[i].row != 0)
                    g_bDataAligned = true;
            }
            else
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ','
                             << vecEvent[i].brightness << ',' << vecEvent[i].t << endl;
            }
        }
    }
}

// convert single channel to three-channel
cv::Mat SensorDataObserver::convertTo3Channels(const cv::Mat& binImg)
{
    cv::Mat three_channel = cv::Mat::zeros(binImg.rows, binImg.cols, CV_8UC3);
    std::vector<cv::Mat> channels;
    for (int i = 0; i < 3; i++)
    {
        channels.push_back(binImg);
    }
    cv::merge(channels, three_channel);
    return three_channel;
}

cv::Mat SensorDataObserver::convertToColored(cv::Mat mat)
{
    cv::Mat coloredMat(CELEX5_ROW, CELEX5_COL, CV_8UC3);
    for (int i = 0; i < coloredMat.rows; ++i)
    {
        cv::Vec3b *p = coloredMat.ptr<cv::Vec3b>(i);
        for (int j = 0; j < coloredMat.cols; ++j)
        {
            int value = mat.at<uchar>(i, j);
            //std::cout << value << std::endl;
            if (value == 0)
            {
                p[j][0] = 0;
                p[j][1] = 0;
                p[j][2] = 0;
            }
            else if (value < 51)	//blue
            {
                p[j][0] = 255;
                p[j][1] = 0;
                p[j][2] = 0;
            }
            else if (value < 102)
            {
                p[j][0] = 255;
                p[j][1] = 255;
                p[j][2] = 0;
            }
            else if (value < 153)	//green
            {
                p[j][0] = 0;
                p[j][1] = 255;
                p[j][2] = 0;
            }
            else if (value < 204)
            {
                p[j][0] = 0;
                p[j][1] = 255;
                p[j][2] = 255;
            }
            else	//red
            {
                p[j][0] = 0;
                p[j][1] = 0;
                p[j][2] = 255;
            }
        }
    }
    return coloredMat;
}

void SensorDataObserver::updateImage(unsigned char *pBuffer1, CeleX5::CeleX5Mode mode, int loopNum, int picMode)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif

#ifdef TRANSMIT_RESIZED_PIC
    int row_nbr = CELEX5_ROW_RESIZED;
    int col_nbr = CELEX5_COL_RESIZED;
#else
    int row_nbr = CELEX5_ROW;
    int col_nbr = CELEX5_COL;
#endif

    //    cout << "mode = " << mode << ", loopNum = " << loopNum << endl;
    int row = 0, col = 0, index = 0, value = 0;
    uchar* pp1 = NULL;
    if (loopNum == 1)
        pp1 = m_imageMode1.bits();
    else if (loopNum == 2)
        pp1 = m_imageMode2.bits();
    else if (loopNum == 3)
        pp1 = m_imageMode3.bits();
    else
    {
        pp1 = m_imageMode1.bits();
    }
    //cout << "type = " << (int)m_pCeleX5->getFixedSenserMode() << endl;
    if (mode == CeleX5::Event_Optical_Flow_Mode ||
            mode == CeleX5::Full_Optical_Flow_S_Mode ||
            mode == CeleX5::Full_Optical_Flow_M_Mode)
    {
        for (int i = 0; i < row_nbr; ++i)
        {
            for (int j = 0; j < col_nbr; ++j)
            {
                col = j;
                row = i;

                index = row*col_nbr+col;
                if (pBuffer1)
                {
                    if (0 == picMode)
                    {
                        value = pBuffer1[index];
                        if (0 == value)
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                        else if (value < 50) //blue
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 255;
                        }
                        else if (value < 100)
                        {
                            *pp1 = 0;
                            *(pp1+1) = 255;
                            *(pp1+2) = 255;
                        }
                        else if (value < 150) //green
                        {
                            *pp1 = 0;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else if (value < 200) //yellow
                        {
                            *pp1 = 255;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else //red
                        {
                            *pp1 = 255;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                    }
                    else if (1 == picMode)
                    {
                        value = pBuffer1[index];
                        if (0 == value)
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                        else if (value < 20) //red
                        {
                            *pp1 = 255;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                        else if (value < 40) //yellow
                        {
                            *pp1 = 255;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else if (value < 60) //green
                        {
                            *pp1 = 0;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else if (value < 80) //green blue
                        {
                            *pp1 = 0;
                            *(pp1+1) = 255;
                            *(pp1+2) = 255;
                        }
                        else //blue
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 255;
                        }
                    }
                    else if (2 == picMode)
                    {
                        value = pBuffer1[index];
                        if (0 == value)
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                        else if (value < 21 || value > 210) //30 300 red
                        {
                            *pp1 = 255;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                        else if (value > 32 && value <= 96) //45 135 blue
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 255;
                        }
                        else if (value > 96 && value <= 159) //135 225 green
                        {
                            *pp1 = 0;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else if (value > 159 && value < 223) //225 315 yellow
                        {
                            *pp1 = 255;
                            *(pp1+1) = 255;
                            *(pp1+2) = 0;
                        }
                        else
                        {
                            *pp1 = 0;
                            *(pp1+1) = 0;
                            *(pp1+2) = 0;
                        }
                    }
                    pp1+= 3;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < row_nbr; ++i)
        {
            for (int j = 0; j < col_nbr; ++j)
            {
                col = j;
                row = i;

                index = row*col_nbr+col;
                if (pBuffer1)
                {
                    value = pBuffer1[index];
                    *pp1 = value;
                    *(pp1+1) = value;
                    *(pp1+2) = value;
                    pp1+= 3;
                }
            }
        }
    }

#ifdef _LOG_TIME_CONSUMING_
    gettimeofday(&tv_end, NULL);
    cout << "updateImage time = " << tv_end.tv_usec - tv_begin.tv_usec << endl;
#endif
    update();
}


void SensorDataObserver::paintEvent(QPaintEvent *event)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif

    Q_UNUSED(event);
    QFont font;
    font.setPixelSize(20);
    QPainter painter(this);
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(font);

    if (m_bLoopModeEnabled)
    {
        painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
        painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
        painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
    }
    else
    {
        if (m_bShowMultipleWindows)
        {
            if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Address_Only_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.fillRect(QRect(0, 0, 165, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 165, 22), "Event Binary Pic");

                //                painter.fillRect(QRect(660, 0, 255, 22), QBrush(Qt::darkBlue));
                //                painter.drawText(QRect(660, 0, 255, 22), "Event Denoised Binary Pic");
            }
            else if (m_pCeleX5->getSensorFixedMode() == CeleX5::Full_Optical_Flow_S_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));

                painter.fillRect(QRect(0, 0, 170, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 170, 22), "Optical-flow Pic");

                painter.fillRect(QRect(660, 0, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 230, 22), "Optical-flow Speed Pic");

                painter.fillRect(QRect(0, 440, 270, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 440, 270, 22), "Optical-flow Direction Pic");
            }
            else if(m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Intensity_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
            }
            else
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
            }
        }
        else
        {
            painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1));
            if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Address_Only_Mode ||
                    m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Optical_Flow_Mode ||
                    m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Intensity_Mode)
            {
                //painter.fillRect(QRect(10, 10, 80, 30), QBrush(Qt::blue));
                //painter.drawText(QRect(14, 14, 80, 30), "T: " + QString::number(m_uiTemperature));
            }
            else
            {
                painter.fillRect(QRect(10, 10, 120, 30), QBrush(Qt::blue));
                painter.drawText(QRect(14, 14, 120, 30), "FPS: " + QString::number(m_uiRealFullFrameFPS) + "/" + QString::number(m_uiFullFrameFPS));
            }
        }
    }

#ifdef _LOG_TIME_CONSUMING_
    gettimeofday(&tv_end, NULL);
    cout << "paintEvent time = " << tv_end.tv_usec - tv_begin.tv_usec << endl;
#endif
}

void SensorDataObserver::onUpdateImage()
{
    if (m_pCeleX5->isLoopModeEnabled())
    {
        return;
    }
    uchar* pp1 = NULL;
    pp1 = m_imageMode1.bits();
    CeleX5::CeleX5Mode mode = m_pCeleX5->getSensorFixedMode();
    if (mode == CeleX5::Full_Picture_Mode)
    {
        m_pCeleX5->getFullPicBuffer(m_pBuffer[0]);
    }
    else if (mode == CeleX5::Event_Address_Only_Mode)
    {
        if (m_bShowMultipleWindows)
        {
            m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventDenoisedBinaryPic);
            updateImage(m_pBuffer[1], mode, 2, 0);
        }
        else
        {
            if (0 == m_iPicMode)
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
            else if (1 == m_iPicMode)
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventDenoisedBinaryPic);

        }
    }
    else if (mode == CeleX5::Event_Optical_Flow_Mode)
    {
        m_pCeleX5->getEventPicBuffer(m_pBuffer[0]);
    }
    else if (mode == CeleX5::Event_Intensity_Mode)
    {
        if (m_bShowMultipleWindows)
        {
            m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventGrayPic);
            m_pCeleX5->getEventPicBuffer(m_pBuffer[2], CeleX5::EventAccumulatedPic);
            updateImage(m_pBuffer[1], mode, 2, 0);
            updateImage(m_pBuffer[2], mode, 3, 0);
        }
        else
        {
            if (0 == m_iPicMode)
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
            else if (1 == m_iPicMode)
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventGrayPic);
            else if (2 == m_iPicMode)
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventAccumulatedPic);
        }
    }
    else
    {
        if (m_bShowMultipleWindows)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Pic);
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[1], CeleX5::Full_Optical_Flow_Speed_Pic);
            m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[2], CeleX5::Full_Optical_Flow_Direction_Pic);
            updateImage(m_pBuffer[1], mode, 2, 1);
            updateImage(m_pBuffer[2], mode, 3, 2);
        }
        else
        {
            if (0 == m_iPicMode)
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Pic);
            else if (1 == m_iPicMode)
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Speed_Pic);
            else if (2 == m_iPicMode)
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Direction_Pic);
        }
    }

    updateImage(m_pBuffer[0], mode, 1, m_iPicMode);

    m_uiRealFullFrameFPS = m_pCeleX5->getFullFrameFPS();
    update();
}

void SensorDataObserver::onSaveImage2Video()
{
    if(m_queueForSaving.size()>0)
    {
        std::vector<uchar> dataEncode;
        cv::imencode(".jpg", m_queueForSaving.front(), dataEncode, m_vecEncodeparam);

        m_pVideoStream->avWriter(dataEncode);
        m_queueForSaving.pop();
    }
    else
    {
        if(!m_bRecordVideoEnabled)
        {
            qDebug("Write frames to output file finished!\n");
            m_pVideoStream->avWriterRelease();
            m_pSaveImageTimer->stop();
            setSwitch2NextFileState(true);
        }
    }
}

CeleX5Widget::CeleX5Widget(QWidget *parent)
    : QWidget(parent)
    , m_pCFGWidget(NULL)
    , m_pPlaybackBg(NULL)
    , m_pVideoCFGWidget(NULL)
    , m_emDeviceType(CeleX5::CeleX5_ZYNQ)
    , m_bLoopModeEnabled(false)
    , m_strIPAddress("192.168.1.11")
    , m_usIPPort(1234)
    , m_iVideoFPS(30)
    , m_uiRecordTimeInterval(60)
{
    m_pCeleX5 = new CeleX5;
    m_pCeleX5->openSensor(m_emDeviceType);
    //
    m_pPipeOutDataTimer = new QTimer(this);

    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    {
        connect(m_pPipeOutDataTimer, SIGNAL(timeout()), this, SLOT(onPipeoutDataTimer()));
        m_pPipeOutDataTimer->start(1);
    }
    else if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
    {
        connect(m_pPipeOutDataTimer, SIGNAL(timeout()), this, SLOT(onPipeoutDataTimer()));
        m_pPipeOutDataTimer->start(200);
    }

    m_pReadBinTimer = new QTimer(this);
    m_pReadBinTimer->setSingleShot(true);
    connect(m_pReadBinTimer, SIGNAL(timeout()), this, SLOT(onReadBinTimer()));

    m_pUpdateTimer = new QTimer(this);
    connect(m_pUpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdatePlayInfo()));

    m_pRecordVideoTimer = new QTimer(this);
    connect(m_pRecordVideoTimer,SIGNAL(timeout()), this, SLOT(onRecordVideoTimer()));

    m_pRecordDataTimer = new QTimer(this);
    connect(m_pRecordDataTimer,SIGNAL(timeout()), this, SLOT(onRecordDataTimer()));

    QHBoxLayout* layoutBtn = new QHBoxLayout;
    //layoutBtn->setContentsMargins(0, 0, 0, 880);
    createButtons(layoutBtn);

    m_pSensorDataObserver = new SensorDataObserver(m_pCeleX5->getSensorDataServer(), this);
    m_pSensorDataObserver->show();
    //m_pSensorDataObserver->setGeometry(40, 130, 1280, 1000);
    m_pSensorDataObserver->setCeleX5(m_pCeleX5);

    //--- create comboBox to select image type
    m_pCbBoxImageType = new QComboBox(this);
    QString style1 = "QComboBox {font: 18px Calibri; color: #FFFF00; border: 2px solid darkgrey; "
                     "border-radius: 5px; background: green;}";
    QString style2 = "QComboBox:editable {background: green;}";

    m_pCbBoxImageType->setGeometry(1070, 80, 250, 30);
    m_pCbBoxImageType->show();
    m_pCbBoxImageType->setStyleSheet(style1 + style2);
    m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
    //    m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
    m_pCbBoxImageType->setCurrentIndex(0);
    connect(m_pCbBoxImageType, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageTypeChanged(int)));

    //--- create comboBox to select sensor mode
    //Fixed Mode
    m_pCbBoxFixedMode = createModeComboBox("Fixed", QRect(40, 80, 300, 30), this, false, 0);
    //m_pCbBoxFixedMode->setCurrentText("Fixed - Event_Address_Only Mode");
    m_pCbBoxFixedMode->setCurrentIndex(0);
    m_pCbBoxFixedMode->show();
    //Loop A
    m_pCbBoxLoopAMode = createModeComboBox("LoopA", QRect(40, 80, 300, 30), this, true, 1);
    //Loop B
    m_pCbBoxLoopBMode = createModeComboBox("LoopB", QRect(40+660, 80, 300, 30), this, true, 2);
    //Loop C
    m_pCbBoxLoopCMode = createModeComboBox("LoopC", QRect(40, 80+440, 300, 30), this, true, 3);

    m_pBtnShowStyle = createButton("Show Multiple Windows", QRect(350, 80, 220, 30), this);
    connect(m_pBtnShowStyle, SIGNAL(released()), this, SLOT(onShowMultipleWindows()));

    QHBoxLayout* layoutSensorImage = new QHBoxLayout;
    layoutSensorImage->setContentsMargins(30, 0, 0, 0);
    layoutSensorImage->addWidget(m_pSensorDataObserver);

    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addLayout(layoutBtn);
    pMainLayout->addSpacing(70);
    //pMainLayout->addLayout(layoutComboBox);
    pMainLayout->addLayout(layoutSensorImage);
    this->setLayout(pMainLayout);

    m_pFPSSlider = new CfgSlider(this, 1, 100, 1, 30, this);
    m_pFPSSlider->setGeometry(1330, 150, 460, 70);
    m_pFPSSlider->setBiasType("Display FPS");
    m_pFPSSlider->setDisplayName("Display FPS");
    m_pFPSSlider->setObjectName("Display FPS");

    m_pFrameSlider = new CfgSlider(this, 1, 100, 1, 30, this);
    m_pFrameSlider->setGeometry(1330, 250, 460, 70);
    m_pFrameSlider->setBiasType("Frame time");
    m_pFrameSlider->setDisplayName("Frame time");
    m_pFrameSlider->setObjectName("Frame time");

    if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
    {
        //----------- read ini files -----------
        QString config_path = QCoreApplication::applicationDirPath() + "/config.ini";
        //qDebug() << config_path;
        QFile file(config_path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            int index = 0;
            while (!file.atEnd())
            {
                QByteArray line = file.readLine();
                QString str(line);
                if (str.contains("#"))
                    continue;
                if (!str.contains("="))
                    continue;
                if (str.contains("\n"))
                    str.remove("\n");
                if (str.contains(" "))
                    str.remove(" ");
                QStringList strList = str.split("=", QString::SkipEmptyParts);
                //qDebug() << strList;
                index++;
                if (strList[0] == "address")
                {
                    m_strIPAddress = strList[1].toStdString();
                    cout << "address = " << m_strIPAddress << endl;
                }
                else if (strList[0] == "port")
                {
                    m_usIPPort = strList[1].toShort();
                    cout << "port = " << m_usIPPort << endl;
                }
                else if (strList[0] == "time_interval")
                {
                    m_uiRecordTimeInterval = strList[1].toInt();
                    cout << "time_interval = " << m_uiRecordTimeInterval << endl;
                }
            }
            file.close();
        }
    }
    QLabel* pIPLabel = new QLabel(this);
    pIPLabel->setGeometry(1700, 78, 200, 30);
    pIPLabel->setAlignment(Qt::AlignCenter);
    pIPLabel->setStyleSheet("QLabel {font: 20px Calibri; color: #FF0000; background: white;}");
    pIPLabel->setText("IP: "+QString::fromStdString(m_strIPAddress));
    pIPLabel->show();
}

CeleX5Widget::~CeleX5Widget()
{
    if (m_pSensorDataObserver)
    {
        delete m_pSensorDataObserver;
        m_pSensorDataObserver = NULL;
    }
    if (m_pCeleX5)
    {
        delete m_pCeleX5;
        m_pCeleX5 = NULL;
    }
}

void CeleX5Widget::closeEvent(QCloseEvent *)
{
    cout << "CeleX5Widget::closeEvent" << endl;
    //QWidget::closeEvent(event);
    if (m_pCFGWidget)
    {
        m_pCFGWidget->close();
    }
}

QComboBox *CeleX5Widget::createModeComboBox(QString text, QRect rect, QWidget *parent, bool bLoop, int loopNum)
{
    QString style1 = "QComboBox {font: 18px Calibri; color: white; border: 2px solid darkgrey; "
                     "border-radius: 5px; background: green;}";
    QString style2 = "QComboBox:editable {background: green;}";

    QComboBox* pComboBoxMode = new QComboBox(parent);
    pComboBoxMode->setGeometry(rect);
    pComboBoxMode->show();
    pComboBoxMode->setStyleSheet(style1 + style2);
    QStringList modeList;
    if (bLoop)
    {
        if (loopNum == 1)
            modeList << "Full_Picture Mode";
        else if (loopNum == 2)
            modeList << "Event_Address_Only Mode";
        else if (loopNum == 3)
            modeList << "Full_Optical_Flow_S Mode" << "Full_Optical_Flow_M Mode";
    }
    else
    {
        modeList << "Event_Address_Only Mode" << "Event_Optical_Flow Mode" << "Event_Intensity Mode"
                 << "Full_Picture Mode" << "Full_Optical_Flow_S Mode";
    }

    for (int i = 0; i < modeList.size(); i++)
    {
        pComboBoxMode->insertItem(i, text+" - "+modeList.at(i));
    }
    pComboBoxMode->hide();
    connect(pComboBoxMode, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSensorModeChanged(QString)));

    return pComboBoxMode;
}

void CeleX5Widget::createButtons(QHBoxLayout* layout)
{
    QStringList btnNameList;
    btnNameList.push_back("RESET");
    btnNameList.push_back("Generate FPN");
    btnNameList.push_back("Change FPN");
    btnNameList.push_back("Start Recording Bin");
    btnNameList.push_back("Start Recording Video");
    btnNameList.push_back("Playback");
    btnNameList.push_back("Enter Loop Mode");
    btnNameList.push_back("Configurations");
    //btnNameList.push_back("Enable Auto ISP");
    //btnNameList.push_back("More Parameters ...");
    btnNameList.push_back("Rotate_LR");
    btnNameList.push_back("Rotate_UD");
    btnNameList.push_back("ConvertBin2Video");
    btnNameList.push_back("ConvertBin2CSV");
    //btnNameList.push_back("Test: Save Pic");
    btnNameList.push_back("Connect");
    //btnNameList.push_back("Save Parameters");
    //btnNameList.push_back("Enable Anti-Flashlight");

    m_pButtonGroup = new QButtonGroup;
    for (int i = 0; i < btnNameList.count(); ++i)
    {
        QPushButton* pButton = createButton(btnNameList.at(i), QRect(20, 20, 100, 36), this);
        pButton->setObjectName(btnNameList.at(i));
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
        m_pButtonGroup->addButton(pButton, i);
        layout->addWidget(pButton);
    }
    connect(m_pButtonGroup, SIGNAL(buttonReleased(QAbstractButton*)), this, SLOT(onButtonClicked(QAbstractButton*)));
}

QPushButton *CeleX5Widget::createButton(QString text, QRect rect, QWidget *parent)
{
    QPushButton* pButton = new QPushButton(text, parent);
    pButton->setGeometry(rect);

    pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }"
                           "QPushButton:disabled {background: #777777; color: lightgray;}");
    return pButton;
}

SliderWidget *CeleX5Widget::createSlider(CeleX5::CfgInfo cfgInfo, int value, QRect rect, QWidget *parent, QWidget *widgetSlot)
{
    SliderWidget* pSlider = new SliderWidget(parent, cfgInfo.min, cfgInfo.max, cfgInfo.step, value, widgetSlot, false);
    pSlider->setGeometry(rect);
    pSlider->setBiasType(cfgInfo.name);
    pSlider->setDisplayName(QString::fromStdString(cfgInfo.name));
    pSlider->setBiasAddr(cfgInfo.high_addr, cfgInfo.middle_addr, cfgInfo.low_addr);
    pSlider->setObjectName(QString::fromStdString(cfgInfo.name));
    pSlider->setDisabled(true);
    if (value < 0)
        pSlider->setLineEditText("--");
    return pSlider;
}

void CeleX5Widget::setButtonEnable(QPushButton *pButton)
{
    pButton->setStyleSheet("QPushButton {background: #008800; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }"
                           "QPushButton:disabled {background: #777777; color: lightgray;}");
}

void CeleX5Widget::setButtonNormal(QPushButton *pButton)
{
    pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }"
                           "QPushButton:disabled {background: #777777; color: lightgray;}");
}


void CeleX5Widget::switchMode(QPushButton* pButton)
{
    if (m_bLoopModeEnabled)
    {
        m_bLoopModeEnabled = false;
        m_pSensorDataObserver->setLoopModeEnabled(false);
        m_pCeleX5->setLoopModeEnabled(false);

        m_pCbBoxLoopAMode->hide();
        m_pCbBoxLoopBMode->hide();
        m_pCbBoxLoopCMode->hide();
        //
        m_pCbBoxImageType->show();
        m_pCbBoxFixedMode->show();
        m_pFPSSlider->show();
        m_pFrameSlider->show();
        m_pBtnShowStyle->show();
        pButton->setText("Enter Loop Mode");
        //
        m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);

        m_pCbBoxFixedMode->setCurrentText("Fixed - Event_Address_Only Mode");
        m_pCbBoxImageType->setCurrentIndex(0);
    }
    else
    {
        m_bLoopModeEnabled = true;
        m_pSensorDataObserver->setLoopModeEnabled(true);
        m_pCeleX5->setLoopModeEnabled(true);

        m_pCbBoxLoopAMode->show();
        m_pCbBoxLoopBMode->show();
        m_pCbBoxLoopCMode->show();

        m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, 1);
        m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, 2);
        m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, 3);
        //
        m_pCbBoxImageType->hide();
        m_pCbBoxFixedMode->hide();
        m_pFPSSlider->hide();
        m_pFrameSlider->hide();
        m_pBtnShowStyle->hide();
        pButton->setText("Enter Fixed Mode");
        //
        //Loop A
        m_pCbBoxLoopAMode->setCurrentText("LoopA - Full_Picture Mode");
        //Loop B
        m_pCbBoxLoopBMode->setCurrentText("LoopB - Event_Address_Only Mode");
        //Loop C
        m_pCbBoxLoopCMode->setCurrentText("LoopC - Full_Optical_Flow_S Mode");

        //        QPushButton* pButton6 = (QPushButton*)m_pButtonGroup->button(6);
        //        pButton6->setText("Configurations");
        //        setButtonNormal(pButton6);
    }
    m_pSensorDataObserver->update();
}

void CeleX5Widget::setSliderMaxValue(QWidget *parent, QString objName, int value)
{
    for (int i = 0; i < parent->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)parent->children().at(i);
        if (pWidget->objectName() == objName)
        {
            pWidget->setMaximum(value);
            return;
        }
    }
}

int CeleX5Widget::getSliderMax(QWidget *parent, QString objName)
{
    for (int i = 0; i < parent->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)parent->children().at(i);
        if (pWidget->objectName() == objName)
        {
            return pWidget->maximum();
        }
    }
    return 0;
}

void CeleX5Widget::showPlaybackBoard(bool show)
{
    if (!m_pPlaybackBg)
    {
        m_pPlaybackBg = new QWidget(this);
        m_pPlaybackBg->setStyleSheet("background-color: lightgray; ");
        m_pPlaybackBg->setGeometry(1350, 300, 800, 500);

        CfgSlider* pSlider1 = new CfgSlider(m_pPlaybackBg, 1, 1000, 1, 1000, this);
        pSlider1->setGeometry(0, 0, 500, 70);
        pSlider1->setBiasType("FPS");
        pSlider1->setDisplayName("FPS");
        pSlider1->setObjectName("FPS");

        CfgSlider* pSlider2 = new CfgSlider(m_pPlaybackBg, 0, 1000, 1, 0, this);
        pSlider2->setGeometry(0, 100, 500, 70);
        pSlider2->setBiasType("DataCount");
        pSlider2->setDisplayName("DataCount");
        pSlider2->setObjectName("DataCount");

        m_pLabelCurrentTime = new QLabel(m_pPlaybackBg);
        m_pLabelCurrentTime->setGeometry(220, 200, 70, 40);
        m_pLabelCurrentTime->setAlignment(Qt::AlignCenter);
        m_pLabelCurrentTime->setText("00:00");
        m_pLabelCurrentTime->setStyleSheet("font: 18px Calibri; color: black; ");

        int left = 10;
        int top = 230;
        QLabel* m_pLabelStartTime = new QLabel(m_pPlaybackBg);
        m_pLabelStartTime->setGeometry(left, top, 50, 35);
        m_pLabelStartTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_pLabelStartTime->setText("00:00");
        m_pLabelStartTime->setStyleSheet("font: 18px Calibri; color: black; ");

        left += m_pLabelStartTime->width() + 6;
        m_pSliderPlayer = new DoubleSlider(m_pPlaybackBg);
        m_pSliderPlayer->setGeometry(left, top, 380, 35);
        m_pSliderPlayer->setRange(0, 10000);
        m_pSliderPlayer->setValue(0);
        connect(m_pSliderPlayer, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
        connect(m_pSliderPlayer, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
        connect(m_pSliderPlayer, SIGNAL(valueChanged(ulong)), this, SLOT(onSliderValueChanged(ulong)));
        connect(m_pSliderPlayer, SIGNAL(minValueChanged(unsigned long)), this, SLOT(onMinValueChanged(unsigned long)));
        connect(m_pSliderPlayer, SIGNAL(maxValueChanged(unsigned long)), this, SLOT(onMaxValueChanged(unsigned long)));

        left += m_pSliderPlayer->width() + 6;
        m_pLabelEndTime = new QLabel(m_pPlaybackBg);
        m_pLabelEndTime->setGeometry(left, top, 50, 35);
        m_pLabelEndTime->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_pLabelEndTime->setStyleSheet("font: 18px Calibri; color: black; ");
        m_pLabelEndTime->setText("00:00");
    }
    if (show)
        m_pPlaybackBg->show();
    else
        m_pPlaybackBg->hide();
}

void CeleX5Widget::changeFPN()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a FPN file", QCoreApplication::applicationDirPath(), "FPN Files (*.txt)");
    if (filePath.isEmpty())
        return;

    m_pCeleX5->setFpnFile(filePath.toLocal8Bit().data());
}

void CeleX5Widget::playback(QPushButton *pButton)
{
    if ("Playback" == pButton->text())
    {
        QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
        if (filePath.isEmpty())
            return;

        pButton->setText("Exit Playback");
        setButtonEnable(pButton);

        showPlaybackBoard(false);
        m_pSensorDataObserver->setPlaybackEnabled(true);
        m_pSensorDataObserver->setDisplayType(Playback);
        m_pCeleX5->setIsPlayBack(true);

        if (m_pCeleX5->openBinFile(filePath.toLocal8Bit().data()))
        {
            CeleX5::BinFileAttributes header = m_pCeleX5->getBinFileAttributes();
            //QString qsHour = header.hour > 9 ? QString::number(header.hour) : "0" + QString::number(header.hour);
            QString qsMinute = header.minute > 9 ? QString::number(header.minute) : "0" + QString::number(header.minute);
            QString qsSecond = header.second > 9 ? QString::number(header.second) : "0" + QString::number(header.second);
            m_pLabelEndTime->setText(/*qsHour + ":" + */qsMinute + ":" + qsSecond);
            m_timeCurrent.setHMS(0, 0, 0);

            if(!m_pCeleX5->isLoopModeEnabled())
                m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());

            onReadBinTimer();
            m_pUpdateTimer->start(30);
        }
    }
    else
    {
        showPlaybackBoard(false);
        m_pSensorDataObserver->setPlaybackEnabled(false);
        m_pSensorDataObserver->setDisplayType(Realtime);
        m_pCeleX5->setIsPlayBack(false);
        if(!m_pCeleX5->isLoopModeEnabled())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
            m_pCbBoxFixedMode->setCurrentIndex(0);
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        pButton->setText("Playback");
        setButtonNormal(pButton);
    }
}

void CeleX5Widget::convertBin2CSV(QPushButton *pButton)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
    if (filePath.isEmpty())
        return;

    g_bDataAligned = false;

    QString path = filePath.left(filePath.size() - 4);
    path += ".csv";
    g_ofWriteMat.open(path.toLocal8Bit().data());

    showPlaybackBoard(false);
    m_pSensorDataObserver->setPlaybackEnabled(true);
    m_pSensorDataObserver->setDisplayType(ConvertBin2CSV);
    pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }");
    m_pCeleX5->setIsPlayBack(true);

    if (m_pCeleX5->openBinFile(filePath.toLocal8Bit().data()))
    {
        CeleX5::BinFileAttributes header = m_pCeleX5->getBinFileAttributes();
        //QString qsHour = header.hour > 9 ? QString::number(header.hour) : "0" + QString::number(header.hour);
        QString qsMinute = header.minute > 9 ? QString::number(header.minute) : "0" + QString::number(header.minute);
        QString qsSecond = header.second > 9 ? QString::number(header.second) : "0" + QString::number(header.second);
        m_pLabelEndTime->setText(/*qsHour + ":" + */qsMinute + ":" + qsSecond);
        m_timeCurrent.setHMS(0, 0, 0);

        if(!m_pCeleX5->isLoopModeEnabled())
            m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());

        onReadBinTimer();
        m_pUpdateTimer->start(30);
    }
}

void CeleX5Widget::record(QPushButton* pButton)
{
    if ("Start Recording Bin" == pButton->text())
    {
        pButton->setText("Stop Recording Bin");
        setButtonEnable(pButton);
        //
        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrBinName;
        if (CeleX5::CeleX5_MIPI == m_emDeviceType)
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        else
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }
        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        if (m_pCeleX5->isLoopModeEnabled())
            qstrBinName += "_Loop_";
        else
            qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrBinName += QString::number(m_pCeleX5->getClockRate());
        qstrBinName += "M.bin"; //MHz
        //        std::string filePath = qstrBinName.toStdString();
        m_pCeleX5->startRecording(qstrBinName.toLocal8Bit().data());

        m_pRecordDataTimer->start(m_uiRecordTimeInterval*1000);
    }
    else
    {
        pButton->setText("Start Recording Bin");
        setButtonNormal(pButton);
        m_pCeleX5->stopRecording();
        //
        m_pRecordDataTimer->stop();
    }
}

void CeleX5Widget::recordVideo()
{
    QList<QPushButton*> pButton = this->findChildren<QPushButton *>("Start Recording Video");

    if ("Start Recording Video" == pButton[0]->text())
    {
        m_pSaveImageTimer->start(1);
        m_pRecordVideoTimer->start(m_uiRecordTimeInterval*1000);

        pButton[0]->setText("Stop Recording Video");
        setButtonEnable(pButton[0]);
        //
        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrBinName;
        if (CeleX5::CeleX5_MIPI == m_emDeviceType)
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        else
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }

        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        if (m_pCeleX5->isLoopModeEnabled())
            qstrBinName += "_Loop_";
        else
            qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrBinName += QString::number(m_pCeleX5->getClockRate());
        qstrBinName += "M.mkv"; //MHz

        m_pVideoStream->avWriterInit(qstrBinName.toLocal8Bit().data(),m_iVideoFPS);
        m_pSensorDataObserver->setRecordVideoEnabled(true);
    }
}

void CeleX5Widget::recordVideo(QPushButton* pButton)
{
    if ("Stop Recording Video" == pButton->text())
    {
        //        m_pSaveImageTimer->stop();
        m_pRecordVideoTimer->stop();
        m_pSensorDataObserver->setRecordVideoEnabled(false);
        //        m_pVideoStream->avWriterRelease();
        pButton->setText("Start Recording Video");
        setButtonNormal(pButton);
    }
}

void CeleX5Widget::showCFG()
{
    if (!m_pCFGWidget)
    {
        m_pCFGWidget = new QWidget;
        m_pCFGWidget->setWindowTitle("Configuration Settings");
        m_pCFGWidget->setGeometry(300, 50, 1300, 450);

        QString style1 = "QGroupBox {"
                         "border: 2px solid #990000;"
                         "font: 20px Calibri; color: #990000;"
                         "border-radius: 9px;"
                         "margin-top: 0.5em;"
                         "background: rgba(50,0,0,10);"
                         "}";
        QString style2 = "QGroupBox::title {"
                         "subcontrol-origin: margin;"
                         "left: 10px;"
                         "padding: 0 3px 0 3px;"
                         "}";
        int groupWidth = 610;

        //----- Group 1 -----
        QGroupBox* speedGroup = new QGroupBox("Sensor Speed Parameters: ", m_pCFGWidget);
        speedGroup->setGeometry(10, 20, groupWidth, 110);
        speedGroup->setStyleSheet(style1 + style2);

        //----- Group 2 -----
        QGroupBox* picGroup = new QGroupBox("Sensor Control Parameters: ", m_pCFGWidget);
        picGroup->setGeometry(10, 150, groupWidth, 250);
        picGroup->setStyleSheet(style1 + style2);
        QStringList cfgList;
        //cfgList << "Clock" << "Brightness" << "Contrast" << "Threshold" << "ISO";
        cfgList << "Clock" << "Brightness" << "Threshold" << "ISO";
        int min[5]   = {20,  0,    50,  1};
        int max[5]   = {160, 1023, 511, 4};
        int value[5] = {40, 130,  171, 2};
        int step[5]  = {10,  1,    1,   1};
        int top[5]   = {50,  180,  250, 320};
        for (int i = 0; i < cfgList.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min[i], max[i], step[i], value[i], this);
            pSlider->setGeometry(10, top[i], 600, 70);
            pSlider->setBiasType(QString(cfgList.at(i)).toStdString());
            pSlider->setDisplayName(cfgList.at(i));
            pSlider->setObjectName(cfgList.at(i));
        }

        //----- Group 3 -----
        QGroupBox* loopGroup = new QGroupBox("Loop Mode Duration: ", m_pCFGWidget);
        loopGroup->setGeometry(670, 20, groupWidth, 380);
        loopGroup->setStyleSheet(style1 + style2);
        QStringList cfgList2;
        cfgList2 << "Event Duration" << "FullPic Num" << "S FO Pic Num"  << "M FO Pic Num";
        int min2[4] = {0};
        int max2[4] = {1023, 255, 255, 255};
        int value2[4] = {20, 1, 3, 3};
        for (int i = 0; i < cfgList2.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min2[i], max2[i], 1, value2[i], this);
            pSlider->setGeometry(670, 50+i*80, 600, 70);
            pSlider->setBiasType(QString(cfgList2.at(i)).toStdString());
            pSlider->setDisplayName(cfgList2.at(i));
            pSlider->setObjectName(cfgList2.at(i));
        }
        m_pCFGWidget->setFocus();
    }
    QList<CfgSlider*> slider = m_pCFGWidget->findChildren<CfgSlider *>("Clock");
    if (slider.size() > 0)
        slider[0]->updateValue(m_pCeleX5->getClockRate());
    m_pCFGWidget->raise();
    m_pCFGWidget->show();
    if (m_pCFGWidget->isMinimized())
        m_pCFGWidget->showNormal();
}

void CeleX5Widget::showVideoCFG()
{
    if (!m_pVideoCFGWidget)
    {
        m_pVideoCFGWidget = new QWidget;
        m_pVideoCFGWidget->setWindowTitle("Video record Settings");
        m_pVideoCFGWidget->setGeometry(300, 50, 800, 400);

        QString style1 = "QGroupBox {"
                         "border: 2px solid #990000;"
                         "font: 20px Calibri; color: #990000;"
                         "border-radius: 9px;"
                         "margin-top: 0.5em;"
                         "background: rgba(50,0,0,10);"
                         "}";
        QString style2 = "QGroupBox::title {"
                         "subcontrol-origin: margin;"
                         "left: 10px;"
                         "padding: 0 3px 0 3px;"
                         "}";
        QGroupBox* otherGroup = new QGroupBox("Video Parameters: ", m_pVideoCFGWidget);
        otherGroup->setGeometry(50, 20, 700, 330);
        otherGroup->setStyleSheet(style1 + style2);

        // video fps slider
        QStringList cfgList2;
        cfgList2 << "FPS Parameter";
        QStringList cfgObj2;
        cfgObj2 << "Video FPS";
        int min2[1] = {0};
        int max2[1] = {60};
        int value2[1] = {30};
        for (int i = 0; i < cfgList2.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pVideoCFGWidget, min2[i], max2[i], 1, value2[i], this);
            pSlider->setGeometry(90, 90+i*90, 600, 70);
            pSlider->setBiasType(QString(cfgObj2.at(i)).toStdString());
            pSlider->setDisplayName(cfgList2.at(i));
            pSlider->setObjectName(cfgObj2.at(i));
        }

        //--- video Type ---
        QLabel* pLabe2 = new QLabel(tr("Whether to save a colored video"), m_pVideoCFGWidget);
        pLabe2->setGeometry(100, 200, 600, 50);
        pLabe2->setStyleSheet("QLabel {background: transparent; font: 20px Calibri; }");

        // radio button
        QRadioButton *pRadioBtnTs = new QRadioButton(tr(" close"), m_pVideoCFGWidget);
        pRadioBtnTs->setGeometry(120, 250, 600, 50);
        pRadioBtnTs->setStyleSheet("QRadioButton {background: transparent; color: gray; font: 20px Calibri; }");
        pRadioBtnTs->setChecked(0);
        pRadioBtnTs->show();
        connect(pRadioBtnTs, SIGNAL(toggled(bool)), this, SLOT(onRecordColoredVideo(bool)));
        pRadioBtnTs->setObjectName("Record colored");
        pRadioBtnTs->setAutoExclusive(false);

        // ok button
        QPushButton* pButton = new QPushButton("OK",m_pVideoCFGWidget);
        pButton->setObjectName("OK");
        pButton->setGeometry(580, 250, 100, 30);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
        connect(pButton, SIGNAL(released()), this, SLOT(onBtnOKRelease()));

        m_pVideoCFGWidget->setFocus();

    }
    m_pVideoCFGWidget->raise();
    m_pVideoCFGWidget->show();
}


void CeleX5Widget::onBtnRotateLRReleased(QPushButton* pButton)
{
    cout << m_pCeleX5->getRotateType() << endl;
    if (m_pCeleX5->getRotateType() >= 2)
    {
        m_pCeleX5->setRotateType(-2);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(2);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onBtnRotateUDReleased(QPushButton* pButton)
{
    if (m_pCeleX5->getRotateType()%2 == 1)
    {
        m_pCeleX5->setRotateType(-1);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(1);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::convertBins2Videos()
{
    filePathList = QFileDialog::getOpenFileNames(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");

    if(filePathList.size()>0)
    {
        showPlaybackBoard(false);
        m_pCeleX5->setIsPlayBack(true);
        m_pSensorDataObserver->setDisplayType(ConvertBin2Video);

        QList<QPushButton*> pButton = this->findChildren<QPushButton *>("ConvertBin2Video");
        pButton[0]->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                                  "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                  "font: 20px Calibri; }"
                                  "QPushButton:pressed {background: #992F6F; }");
        convertBin2Video();
    }
}

void CeleX5Widget::convertBin2Video()
{
    if(!m_pSaveImageTimer->isActive())
    {
        m_pSaveImageTimer->start(1);

        m_pSensorDataObserver->setSwitch2NextFileState(false);
        QString filePath = filePathList[0];
        if (filePath.isEmpty())
            return;

        std::string path = filePath.left(filePath.size() - 4).toStdString();
#ifdef _WIN32
        path += ".mkv";
#else
        path += ".mp4";
#endif
        if (m_pCeleX5->openBinFile(filePath.toLocal8Bit().data()))
        {
            QList<QPushButton*> button=this->findChildren<QPushButton *>("Enter Loop Mode");

            if(m_pCeleX5->isLoopModeEnabled())
            {
                //            switchMode(button[0], true, true);
            }
            else
            {
                //            switchMode(button[0], false, true);
                m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());
            }
            onReadBinTimer();
            m_pUpdateTimer->start(30);
            m_pVideoStream->avWriterInit(path.c_str(),m_iVideoFPS);
            m_pSensorDataObserver->setRecordVideoEnabled(true);
        }
    }
}

void CeleX5Widget::onButtonClicked(QAbstractButton *button)
{
    cout << "MainWindow::onButtonClicked: " << button->objectName().toStdString() << endl;
    if ("RESET" == button->objectName())
    {
        m_pCeleX5->reset();
    }
    else if ("Generate FPN" == button->objectName())
    {
        m_pCeleX5->generateFPN("");
    }
    else if ("Change FPN" == button->objectName())
    {
        changeFPN();
    }
    else if ("Start Recording Bin" == button->objectName())
    {
        record((QPushButton*)button);
    }
    else if ("Start Recording Video" == button->objectName())
    {
        m_strButtonName = "Start Recording Video";
        if (button->text() == "Start Recording Video")
            showVideoCFG();
        else
            recordVideo((QPushButton*)button);
    }
    else if ("Playback" == button->objectName())
    {
        playback((QPushButton*)button);
    }
    else if ("Enter Loop Mode" == button->objectName())
    {
        switchMode((QPushButton*)button);
    }
    else if ("Configurations" == button->objectName())
    {
        showCFG();
    }
    else if ("Rotate_LR" == button->objectName())
    {
        onBtnRotateLRReleased((QPushButton*)button);
    }
    else if ("Rotate_UD" == button->objectName())
    {
        onBtnRotateUDReleased((QPushButton*)button);
    }
    else if ("ConvertBin2Video" == button->objectName())
    {
        m_strButtonName = "ConvertBin2Video";
        showVideoCFG();
    }
    else if ("ConvertBin2CSV" == button->objectName())
    {
        convertBin2CSV((QPushButton*)button);
    }
    else if ("Connect" == button->objectName())
    {
        m_pCeleX5->connectToZYNQServer(m_strIPAddress, m_usIPPort);
    }
}

void CeleX5Widget::onValueChanged(uint32_t value, CfgSlider *slider)
{
    cout << "TestSensorWidget::onValueChanged: " << slider->getBiasType() << ", " << value << endl;
    if ("Clock" == slider->getBiasType())
    {
        m_pCeleX5->setClockRate(value);
        m_pSensorDataObserver->setFullFrameFPS(value);
    }
    else if ("Brightness" == slider->getBiasType())
    {
        m_pCeleX5->setBrightness(value);
    }
    else if ("Threshold" == slider->getBiasType())
    {
        m_pCeleX5->setThreshold(value);
    }
    else if ("ISO" == slider->getBiasType())
    {
        m_pCeleX5->setISO(value);
        QString filePath = QCoreApplication::applicationDirPath()+"/FPN_"+QString::number(value)+".txt";
        if (filePath.isEmpty())
            return;
        m_pCeleX5->setFpnFile(filePath.toLocal8Bit().data());
    }
    else if ("Event Duration" == slider->getBiasType())
    {
        m_pCeleX5->setEventDuration(value);
    }
    else if ("FullPic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Picture_Mode);
    }
    else if ("S FO Pic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_S_Mode);
    }
    else if ("M FO Pic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_M_Mode);
    }
    else if ("Display FPS" == slider->getBiasType())
    {
        m_pSensorDataObserver->setPlaybackFPS(value);
    }
    else if ("FPS" == slider->getBiasType())
    {
        m_pSensorDataObserver->setPlaybackFPS(value);
    }
    else if ("DataCount" == slider->getBiasType())
    {
        m_pCeleX5->setCurrentPackageNo(value);
        m_pReadBinTimer->start(READ_BIN_TIMER);
        m_pUpdateTimer->start(1000);
        m_timeCurrent.setHMS(0, 0, 0);
    }
    else if ("Frame time" == slider->getBiasType())
    {
        m_pCeleX5->setEventFrameTime(value);
    }
    else if("Video FPS" == slider->getBiasType())
    {
        m_iVideoFPS = value;
    }
}

void CeleX5Widget::onPipeoutDataTimer()
{
    //    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    //    {
    //        m_pCeleX5->pipeOutFPGAData();
    //    }
}

void CeleX5Widget::onReadBinTimer()
{
    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly ||
            m_emDeviceType == CeleX5::CeleX5_ZYNQ)
    {
        if (!m_pCeleX5->readBinFileData())
        {
            m_pReadBinTimer->start(READ_BIN_TIMER);
        }
    }
}

void CeleX5Widget::onUpdatePlayInfo()
{
    //    if (m_pCeleX5->getTotalPackageCount() != getSliderMax(m_pPlaybackBg, "DataCount"))
    //        setSliderMaxValue(m_pPlaybackBg, "DataCount", m_pCeleX5->getTotalPackageCount());
    //    //cout << "------------------- " << m_pCeleX5->getCurrentPackageNo() << endl;
    //    for (int i = 0; i < m_pPlaybackBg->children().size(); ++i)
    //    {
    //        CfgSlider* pWidget = (CfgSlider*)m_pPlaybackBg->children().at(i);
    //        if (pWidget->objectName() == "DataCount")
    //        {
    //            pWidget->updateValue(m_pCeleX5->getCurrentPackageNo());
    //            if (m_pCeleX5->getTotalPackageCount() == m_pCeleX5->getCurrentPackageNo())
    //            {
    //                m_pUpdateTimer->stop();
    //            }
    //            break;
    //        }
    //    }
    //    QString qsMinute = m_timeCurrent.minute() > 9 ? QString::number(m_timeCurrent.minute()) : "0" + QString::number(m_timeCurrent.minute());
    //    QString qsSecond = m_timeCurrent.second() > 9 ? QString::number(m_timeCurrent.second()) : "0" + QString::number(m_timeCurrent.second());
    //    m_pLabelCurrentTime->setText(qsMinute + ":" + qsSecond);

    //    m_timeCurrent = m_timeCurrent.addSecs(1);

    if (m_pCeleX5->getPlaybackState() == PlayFinished)
    {
        m_pSensorDataObserver->setRecordVideoEnabled(false);

        if (m_pSensorDataObserver->getSwitch2NextFileState())
        {
            cout << "------------- Convert " << filePathList[0].toStdString() << " Finished! -------------" << endl;

            filePathList.removeAt(0);
            if(filePathList.size()>0)
            {
                convertBin2Video();
            }
            else
            {
                if (m_pSensorDataObserver->getDisplayType() == ConvertBin2Video)
                {
                    QList<QPushButton*> button=this->findChildren<QPushButton *>("ConvertBin2Video");
                    button[0]->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                             "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                             "font: 20px Calibri; }"
                                             "QPushButton:pressed {background: #992F6F;}");
                    QMessageBox::information(NULL, "convertBin2Video", "Convert Bin to Video completely!", QMessageBox::Yes, QMessageBox::Yes);
                    m_pSensorDataObserver->setDisplayType(Realtime);
                    m_pCeleX5->setIsPlayBack(false);
                }
                m_pUpdateTimer->stop();
            }
        }
        if (m_pSensorDataObserver->getDisplayType() == ConvertBin2CSV)
        {
            QList<QPushButton*> button=this->findChildren<QPushButton *>("ConvertBin2CSV");
            button[0]->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
            QMessageBox::information(NULL, "convertBin2Video", "Convert Bin to Video completely!", QMessageBox::Yes, QMessageBox::Yes);
            m_pSensorDataObserver->setDisplayType(Realtime);
            m_pCeleX5->setIsPlayBack(false);
            m_pUpdateTimer->stop();
            g_ofWriteMat.close();
        }
    }
}

void CeleX5Widget::onRecordColoredVideo(bool enable)
{
    QList<QRadioButton*> radio1 = m_pVideoCFGWidget->findChildren<QRadioButton *>("Record colored");
    m_pSensorDataObserver->setVideoColorEnable(enable);
    if (enable)
    {
        if (radio1.size() > 0)
        {
            radio1[0]->setText(" open");
            radio1[0]->setStyleSheet("QRadioButton {background: transparent; color: #990000; font: 20px Calibri; }");
        }
    }
    else
    {
        if (radio1.size() > 0)
        {
            radio1[0]->setText(" close");
            radio1[0]->setStyleSheet("QRadioButton {background: transparent; color: gray; font: 20px Calibri; }");
        }
    }
}

void CeleX5Widget::onBtnOKRelease()
{
    m_pVideoCFGWidget->hide();
    if(m_strButtonName == "Start Recording Video")
        recordVideo();
    else if(m_strButtonName == "ConvertBin2Video")
        convertBins2Videos();
}

void CeleX5Widget::onRecordVideoTimer()
{
    if(!m_pRecordVideoTimer->isActive())
        return;

    if(m_pSensorDataObserver->getRecordVideoState())
    {
        qDebug("onRecordVideoTimer - Write frames to output file finished!\n");
        m_pVideoStream->avWriterRelease();
        m_pSaveImageTimer->stop();
        m_pSensorDataObserver->setRecordVideoEnabled(false);

        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrBinName;
        if (CeleX5::CeleX5_MIPI == m_emDeviceType)
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        else
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }

        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        if (m_pCeleX5->isLoopModeEnabled())
            qstrBinName += "_Loop_";
        else
            qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrBinName += QString::number(m_pCeleX5->getClockRate());
        qstrBinName += "M.mkv"; //MHz

        m_pVideoStream->avWriterInit(qstrBinName.toLocal8Bit().data(),m_iVideoFPS);
        m_pSaveImageTimer->start(1);
        m_pSensorDataObserver->setRecordVideoEnabled(true);
    }
}

void CeleX5Widget::onRecordDataTimer()
{
    if (!m_pRecordDataTimer->isActive())
        return;

    m_pCeleX5->stopRecording();
    //
    const QDateTime now = QDateTime::currentDateTime();
    const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));
    QString qstrBinName;
    qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;

    QStringList modeList;
    modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
    if (m_pCeleX5->isLoopModeEnabled())
        qstrBinName += "_Loop_";
    else
        qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

    qstrBinName += QString::number(m_pCeleX5->getClockRate());
    qstrBinName += "M.bin"; //MHz
    std::string filePath = qstrBinName.toStdString();
    m_pCeleX5->startRecording(filePath);
}

void CeleX5Widget::onSensorModeChanged(QString text)
{
    cout << text.toStdString() << endl;
    if (m_pCeleX5->isLoopModeEnabled())
    {
        int loopNum = 0;
        if (text.contains("LoopA"))
            loopNum = 1;
        else if (text.contains("LoopB"))
            loopNum = 2;
        else if (text.contains("LoopC"))
            loopNum = 3;

        QString mode = text.mid(8);
        //cout << loopNum << ", " << mode.toStdString() << endl;
        if (mode == "Event_Address_Only Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, loopNum);
        else if (mode == "Event_Optical_Flow Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Optical_Flow_Mode, loopNum);
        else if (mode == "Event_Intensity Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Intensity_Mode, loopNum);
        else if (mode == "Full_Picture Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, loopNum);
        else if (mode == "Full_Optical_Flow_S Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, loopNum);
        else if (mode == "Full_Optical_Flow_M Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_M_Mode, loopNum);
    }
    else
    {
        QString mode = text.mid(8);
        //cout << mode.toStdString() << endl;
        if (mode == "Event_Address_Only Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            //            m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Event_Optical_Flow Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Optical_Flow_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event OpticalFlow Pic");
        }
        else if (mode == "Event_Intensity Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            m_pCbBoxImageType->insertItem(1, "Event Gray Pic");
            m_pCbBoxImageType->insertItem(2, "Event Accumulative Pic");
            m_pCbBoxImageType->setCurrentIndex(1);
        }
        else if (mode == "Full_Picture Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Picture_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full Pic");
        }
        else if (mode == "Full_Optical_Flow_S Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Full_Optical_Flow_M Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_M_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Full_Optical_Flow_Test Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_Test_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        //        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(7);
        //        pButton->setText("Configurations");
        //        setButtonNormal(pButton);
    }
    m_pCeleX5->reset();
}

void CeleX5Widget::onImageTypeChanged(int index)
{
    cout << "CeleX5Widget::onImageTypeChanged: " << index << endl;
    m_pSensorDataObserver->setPictureMode(index);
}

void CeleX5Widget::onShowMultipleWindows()
{
    if ("Show Multiple Windows" == m_pBtnShowStyle->text())
    {
        m_pSensorDataObserver->setMultipleShowEnabled(true);
        m_pBtnShowStyle->setText("Show Single Windows");
    }
    else
    {
        m_pSensorDataObserver->setMultipleShowEnabled(false);
        m_pBtnShowStyle->setText("Show Multiple Windows");
    }
}
