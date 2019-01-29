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

int mode_index = 0;

SensorDataObserver::SensorDataObserver(CX5SensorDataServer *sensorData, QWidget *parent)
    : QWidget(parent)
    , m_imageMode1(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode2(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode3(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_pCeleX5(NULL)
    , m_bLoopModeEnabled(false)
    , m_iPicMode(0)
    , m_bRealtimeDisplay(true)
    , m_bShowMultipleWindows(false)
    , m_uiTemperature(0)
    , m_iFPS(30)
    , m_iPlaybackFPS(30)
    , m_uiFullFrameFPS(100)
    , m_uiRealFullFrameFPS(0)
    , m_emSensorMode(CeleX5::Full_Picture_Mode)
{
    m_pSensorData = sensorData;
    m_pSensorData->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

    m_imageMode1.fill(Qt::black);
    m_imageMode2.fill(Qt::black);
    m_imageMode3.fill(Qt::black);

//    m_pUpdateTimer = new QTimer(this);
//    connect(m_pUpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdateImage()));
//    m_pUpdateTimer->start(33);

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
    if (!m_pCeleX5->isLoopModeEnabled() && m_bRealtimeDisplay)
    {
        return;
    }
    m_uiRealFullFrameFPS = pSensorData->getFullFrameFPS();
    m_uiTemperature = pSensorData->getTemperature();
    unsigned char* pBuffer = NULL;
    if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)
    {
        pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
    }
    else if (pSensorData->getSensorMode() == CeleX5::Event_Optical_Flow_Mode)
    {
        pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
    }
    else if (pSensorData->getSensorMode() == CeleX5::Event_Intensity_Mode)
    {
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventGrayPic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventAccumulatedPic);
    }
    else if (pSensorData->getSensorMode() == CeleX5::Full_Picture_Mode)
    {
        pBuffer = pSensorData->getFullPicBuffer();
    }
    else
    {
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Speed_Pic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Direction_Pic);
    }

    updateImage(pBuffer, pSensorData->getSensorMode(), pSensorData->getLoopNum(), m_iPicMode);

    if (!m_bRealtimeDisplay)
    {
        if (1000 / m_iPlaybackFPS > 1)
        {
            Sleep(1000 / m_iPlaybackFPS);
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

unsigned char *pBuffer111 = NULL;
void SensorDataObserver::onUpdateZYNQImage(QByteArray byteArray)
{
    //cout << __FUNCTION__ << endl;
    if (pBuffer111 == NULL)
        pBuffer111 = new unsigned char[1024000];
    for (int i = 0; i < 1024000; i++)
        pBuffer111[i] = byteArray.at(i);
    if (m_bLoopModeEnabled)
    {
        if (0 == mode_index)
            mode_index = 3;
        updateImage(pBuffer111, m_emSensorMode, mode_index, 0);
    }
    else
    {
        updateImage(pBuffer111, m_emSensorMode, 1, 0);
    }
}

void SensorDataObserver::updateImage(unsigned char *pBuffer1, CeleX5::CeleX5Mode mode, int loopNum, int picMode)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif

    //cout << "mode = " << mode << ", loopNum = " << loopNum << endl;
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
        for (int i = 0; i < CELEX5_ROW; ++i)
        {
            for (int j = 0; j < CELEX5_COL; ++j)
            {
                col = j;
                row = i;

                index = row*CELEX5_COL+col;
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
        for (int i = 0; i < CELEX5_ROW; ++i)
        {
            for (int j = 0; j < CELEX5_COL; ++j)
            {
                col = j;
                row = i;

                index = row*CELEX5_COL+col;
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
            painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
            painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
            painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));

            if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Address_Only_Mode)
            {
                painter.fillRect(QRect(0, 0, 165, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 165, 22), "Event Binary Pic");

                painter.fillRect(QRect(660, 0, 255, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 255, 22), "Event Denoised Binary Pic");
            }
            else if (m_pCeleX5->getSensorFixedMode() == CeleX5::Full_Optical_Flow_S_Mode)
            {
                painter.fillRect(QRect(0, 0, 170, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 170, 22), "Optical-flow Pic");

                painter.fillRect(QRect(660, 0, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 230, 22), "Optical-flow Speed Pic");

                painter.fillRect(QRect(0, 440, 270, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 440, 270, 22), "Optical-flow Direction Pic");
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
    uchar* pp1 = NULL;
    if (m_pCeleX5->isLoopModeEnabled())
    {
        return;
    }
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

CeleX5Widget::CeleX5Widget(QWidget *parent)
    : QWidget(parent)
    , m_pCFGWidget(NULL)
    , m_pCeleX5Cfg(NULL)
    , m_pPlaybackBg(NULL)
    , m_pZYNQDataClient(NULL)
    , m_emDeviceType(CeleX5::CeleX5_ZYNQ)
    , m_bLoopModeEnabled(false)
{
    m_pCeleX5 = new CeleX5;
    m_pCeleX5->openSensor(m_emDeviceType);
    m_pCeleX5->getCeleX5Cfg();
    m_mapCfgDefault = m_pCeleX5->getCeleX5Cfg();
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
    m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
    m_pCbBoxImageType->setCurrentIndex(0);
    connect(m_pCbBoxImageType, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageTypeChanged(int)));

    //--- create comboBox to select sensor mode
    //Fixed Mode
    m_pCbBoxFixedMode = createModeComboBox("Fixed", QRect(40, 80, 300, 30), this, false, 0);
    m_pCbBoxFixedMode->setCurrentText("Fixed - Event_Address_Only Mode");
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

    m_pFrameSlider = new CfgSlider(this, 1, 100, 1, 6, this);
    m_pFrameSlider->setGeometry(1330, 250, 460, 70);
    m_pFrameSlider->setBiasType("Row Cycle Count");
    m_pFrameSlider->setDisplayName("Row Cycle Count");
    m_pFrameSlider->setObjectName("Row Cycle Count");

    //m_pCeleX5->setClockRate(100);

    if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
    {
        m_pZYNQDataClient = new TCPDataClient(this);
        connect(m_pZYNQDataClient, SIGNAL(pixmapReady(QByteArray)), m_pSensorDataObserver, SLOT(onUpdateZYNQImage(QByteArray)));
    }
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

void CeleX5Widget::playback(QPushButton *pButton)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
    if (filePath.isEmpty())
        return;
    if ("Playback" == pButton->text())
        pButton->setText("Exit ");
    showPlaybackBoard(true);
    m_pSensorDataObserver->setPlaybackEnabled(true);
    if (m_pCeleX5->openBinFile(filePath.toStdString()))
    {
        CeleX5::BinFileAttributes header = m_pCeleX5->getBinFileAttributes();
        //QString qsHour = header.hour > 9 ? QString::number(header.hour) : "0" + QString::number(header.hour);
        QString qsMinute = header.minute > 9 ? QString::number(header.minute) : "0" + QString::number(header.minute);
        QString qsSecond = header.second > 9 ? QString::number(header.second) : "0" + QString::number(header.second);
        m_pLabelEndTime->setText(/*qsHour + ":" + */qsMinute + ":" + qsSecond);
        m_timeCurrent.setHMS(0, 0, 0);

        onReadBinTimer();
        onUpdatePlayInfo();

        m_pUpdateTimer->start(1000);
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
    //btnNameList.push_back("Start Recording Video");
    btnNameList.push_back("Playback");
    btnNameList.push_back("Enter Loop Mode");
    btnNameList.push_back("Configurations");
    //btnNameList.push_back("Enable Auto ISP");
    //btnNameList.push_back("More Parameters ...");
    btnNameList.push_back("Rotate_LR");
    btnNameList.push_back("Rotate_UD");
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

void CeleX5Widget::changeFPN()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a FPN file", QCoreApplication::applicationDirPath(), "FPN Files (*.txt)");
    if (filePath.isEmpty())
        return;

    m_pCeleX5->setFpnFile(filePath.toStdString());
}

void CeleX5Widget::record()
{
    QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(3);
    if ("Start Recording Bin" == pButton->text())
    {
        pButton->setText("Stop Recording Bin");
        setButtonEnable(pButton);
        //
        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrBinName;
        if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }
        else
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrBinName += QString::number(m_pCeleX5->getClockRate());
        qstrBinName += "M.bin"; //MHz
        std::string filePath = qstrBinName.toStdString();
        m_pCeleX5->startRecording(filePath);
    }
    else
    {
        pButton->setText("Start Recording Bin");
        setButtonNormal(pButton);
        m_pCeleX5->stopRecording();
    }
}

void CeleX5Widget::recordVideo()
{
    QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(4);
    if ("Start Recording Video" == pButton->text())
    {
        pButton->setText("Stop Recording Video");
        setButtonEnable(pButton);

        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrVideoName;
        if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
        {
            qstrVideoName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }
        else
        {
            qstrVideoName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        qstrVideoName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrVideoName += QString::number(m_pCeleX5->getClockRate());
        qstrVideoName += "M.avi"; //MHz
        std::string filePath = qstrVideoName.toStdString();
        m_pCeleX5->startRecordingVideo(filePath,CV_FOURCC('X', 'V', 'I', 'D'), 60.0);
    }
    else
    {
        pButton->setText("Start Recording Video");
        setButtonNormal(pButton);
        m_pCeleX5->stopRecordingVideo();
    }
}

void CeleX5Widget::switchMode()
{
    QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(5);
    if (m_bLoopModeEnabled)
    {
        m_bLoopModeEnabled = false;
        m_pSensorDataObserver->setLoopModeEnabled(false);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_ENABLE_LOOP_MODE;
            msg.value = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setLoopModeEnabled(false);
        }

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
        //m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);

        /*
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_FIXED_MODE;
            msg.value = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
        }
        */

        m_pCbBoxFixedMode->setCurrentText("Fixed - Event_Address_Only Mode");
        m_pCbBoxImageType->setCurrentIndex(0);
    }
    else
    {
        m_bLoopModeEnabled = true;
        m_pSensorDataObserver->setLoopModeEnabled(true);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_ENABLE_LOOP_MODE;
            msg.value = 1;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setLoopModeEnabled(true);
        }

        m_pCbBoxLoopAMode->show();
        m_pCbBoxLoopBMode->show();
        m_pCbBoxLoopCMode->show();


        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
//            SEND_MSG_PACK msg;
//            msg.cmd = CMD_SET_LOOP_MODE;
//            msg.value = CeleX5::Full_Picture_Mode;
//            msg.value2 = 1;
//            m_pZYNQDataClient->sendCmd(msg);
//            Sleep(10);

//            msg.value = CeleX5::Event_Address_Only_Mode;
//            msg.value2 = 2;
//            m_pZYNQDataClient->sendCmd(msg);
//            Sleep(10);

//            msg.value = CeleX5::Full_Optical_Flow_S_Mode;
//            msg.value2 = 3;
//            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, 1);
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, 2);
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, 3);
        }
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

        QPushButton* pButton6 = (QPushButton*)m_pButtonGroup->button(6);
        pButton6->setText("Configurations");
        setButtonNormal(pButton6);
    }
    m_pSensorDataObserver->update();
}

void CeleX5Widget::showCFG()
{
    if (!m_pCFGWidget)
    {
        m_pCFGWidget = new QWidget;
        m_pCFGWidget->setWindowTitle("Configuration Settings");
        m_pCFGWidget->setGeometry(300, 50, 1300, 850);

        QString style1 = "QGroupBox {"
                         "border: 1px solid #990000;"
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
        speedGroup->setGeometry(10, 0, groupWidth, 110);
        speedGroup->setStyleSheet(style1 + style2);

        //----- Group 2 -----
        QGroupBox* picGroup = new QGroupBox("Sensor Control Parameters: ", m_pCFGWidget);
        picGroup->setGeometry(10, 130, groupWidth, 320);
        picGroup->setStyleSheet(style1 + style2);
        QStringList cfgList;
        cfgList << "Clock" << "Brightness" << "Contrast" << "Threshold" << "ISO";
        int min[5] = {20, 0, 1, 50, 1};
        int max[5] = {100, 1023, 3, 511, 6};
        int value[5] = {20, 120, 2, 171, 4};
        int step[5] = {10, 1, 1, 1, 1};
        int top[5] = {30, 160, 230, 300, 370};
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
        loopGroup->setGeometry(670, 0, groupWidth, 360);
        loopGroup->setStyleSheet(style1 + style2);
        QStringList cfgList2;
        cfgList2 << "Event Duration" << "FullPic Num" << "S FO Pic Num"  << "M FO Pic Num";
        int min2[4] = {0};
        int max2[4] = {1023, 255, 255, 255};
        int value2[4] = {20, 1, 1, 3};
        for (int i = 0; i < cfgList2.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min2[i], max2[i], 1, value2[i], this);
            pSlider->setGeometry(670, 30+i*80, 600, 70);
            pSlider->setBiasType(QString(cfgList2.at(i)).toStdString());
            pSlider->setDisplayName(cfgList2.at(i));
            pSlider->setObjectName(cfgList2.at(i));
        }

        //----- Group 4 -----
        QGroupBox* autoISPGroup = new QGroupBox("Auto ISP Control Parameters: ", m_pCFGWidget);
        autoISPGroup->setGeometry(10, 470, 1270, 360);
        autoISPGroup->setStyleSheet(style1 + style2);
        QStringList cfgList4;
        cfgList4 << "Brightness 1" << "Brightness 2" << "Brightness 3" << "Brightness 4"
                 << "BRT Threshold 1" << "BRT Threshold 2" << "BRT Threshold 3";
        int min4[7] = {0};
        int max4[7] = {1023, 1023, 1023, 1023, 4095, 4095, 4095};
        int value4[7] = {100, 130, 150, 175, 60, 500, 2500};
        int left4[7] = {10, 10, 10, 10, 670, 670, 670};
        int top4[7] = {500, 580, 660, 740, 500, 580, 660};
        for (int i = 0; i < cfgList4.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min4[i], max4[i], 1, value4[i], this);
            pSlider->setGeometry(left4[i], top4[i], 600, 70);
            pSlider->setBiasType(QString(cfgList4.at(i)).toStdString());
            pSlider->setDisplayName(cfgList4.at(i));
            pSlider->setObjectName(cfgList4.at(i));
        }
        m_pCFGWidget->setFocus();
    }

    m_pCFGWidget->show();
    if (m_pCFGWidget->isMinimized())
        m_pCFGWidget->showNormal();
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

void CeleX5Widget::onButtonClicked(QAbstractButton *button)
{
    cout << "MainWindow::onButtonClicked: " << button->objectName().toStdString() << endl;
    if ("RESET" == button->objectName())
    {
        m_pCeleX5->reset();
    }
    else if ("Generate FPN" == button->objectName())
    {
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_GENERATE_FPN;
            msg.value = 0;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->generateFPN("");
        }
    }
    else if ("Change FPN" == button->objectName())
    {
        changeFPN();
    }
    else if ("Start Recording Bin" == button->objectName())
    {
        record();
    }
    else if("Start Recording Video" == button->objectName())
    {
        recordVideo();
    }
    else if ("Playback" == button->objectName())
    {
        playback((QPushButton*)button);
    }
    else if ("Enter Loop Mode" == button->objectName())
    {
        switchMode();
    }
    else if ("Configurations" == button->objectName())
    {
        showCFG();
    }
    else if ("Enable Auto ISP" == button->objectName())
    {
        if (CeleX5::Full_Picture_Mode == m_pCeleX5->getSensorFixedMode() ||
                m_pCeleX5->isLoopModeEnabled())
        {
            if ("Enable Auto ISP" == button->text())
            {
                button->setText("Disable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(true);
                setButtonEnable((QPushButton*)button);
            }
            else
            {
                button->setText("Enable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(false);
                setButtonNormal((QPushButton*)button);
            }
        }
    }
    else if ("More Parameters ..." == button->objectName())
    {
        showMoreParameters(5);
    }
    else if ("Test: Save Pic" == button->objectName())
    {
        m_pCeleX5->beginSaveFullPic("");
    }
    else if ("Rotate_LR" == button->objectName())
    {
        onBtnRotateLRReleased();
    }
    else if ("Rotate_UD" == button->objectName())
    {
        onBtnRotateUDReleased();
    }
    else if ("Connect" == button->objectName())
    {
        if (m_pZYNQDataClient)
        {
            m_pZYNQDataClient->connectToZYNQServer();
        }
    }
}
void CeleX5Widget::onBtnRotateLRReleased()
{
    cout << m_pCeleX5->getRotateType() << endl;
    if (m_pCeleX5->getRotateType() >= 2)
    {
        m_pCeleX5->setRotateType(-2);
        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(10);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(2);
        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(10);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onBtnRotateUDReleased()
{
    if (m_pCeleX5->getRotateType()%2 == 1)
    {
        m_pCeleX5->setRotateType(-1);
        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(11);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(1);
        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(11);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onValueChanged(uint32_t value, CfgSlider *slider)
{
    cout << "TestSensorWidget::onValueChanged: " << slider->getBiasType() << ", " << value << endl;
    if ("Clock" == slider->getBiasType())
    {
        //m_pCeleX5->setClockRate(value);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_CLOCK;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setClockRate(value);
        }
        m_pSensorDataObserver->setFullFrameFPS(value);
    }
    else if ("Brightness" == slider->getBiasType())
    {
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_CONTRAST;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setBrightness(value);
        }
    }
    else if ("Threshold" == slider->getBiasType())
    {
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_THRESHOLD;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setThreshold(value);
        }
    }
    else if ("Contrast" == slider->getBiasType())
    {
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_CONTRAST;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setContrast(value);
        }
    }
    else if ("ISO" == slider->getBiasType())
    {
        //m_pCeleX5->setISO(value);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_ISO;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setISO(value);
        }
    }
    else if ("Event Duration" == slider->getBiasType())
    {
        //m_pCeleX5->setEventDuration(value);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_LOOP_EVENT_DURATION;
            msg.value = value;
            msg.value2 = 0;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setEventDuration(value);
        }
    }
    else if ("FullPic Num" == slider->getBiasType())
    {
        //m_pCeleX5->setPictureNumber(value, CeleX5::Full_Picture_Mode);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_LOOP_FULL_PIC_NUM;
            msg.value = value;
            msg.value2 = CeleX5::Full_Picture_Mode;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setPictureNumber(value, CeleX5::Full_Picture_Mode);
        }
    }
    else if ("S FO Pic Num" == slider->getBiasType())
    {
        //m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_S_Mode);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_LOOP_S_FO_PIC_NUM;
            msg.value = value;
            msg.value2 = CeleX5::Full_Optical_Flow_S_Mode;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_S_Mode);
        }
    }
    else if ("M FO Pic Num" == slider->getBiasType())
    {
        //m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_M_Mode);
        if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
        {
            SEND_MSG_PACK msg;
            msg.cmd = CMD_SET_LOOP_M_FO_PIC_NUM;
            msg.value = value;
            msg.value2 = CeleX5::Full_Optical_Flow_M_Mode;
            m_pZYNQDataClient->sendCmd(msg);
        }
        else
        {
            m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_M_Mode);
        }
    }
    else if ("Display FPS" == slider->getBiasType())
    {
        m_pSensorDataObserver->setFPS(value);
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
    else if ("Brightness 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 1);
    }
    else if ("Brightness 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 2);
    }
    else if ("Brightness 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 3);
    }
    else if ("Brightness 4" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 4);
    }
    else if ("BRT Threshold 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 1);
    }
    else if ("BRT Threshold 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 2);
    }
    else if ("BRT Threshold 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 3);
    }
    else if ("Row Cycle Count" == slider->getBiasType())
    {
        m_pCeleX5->setRowCycleCount(value);
    }
}

void CeleX5Widget::onPipeoutDataTimer()
{
    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    {
        m_pCeleX5->pipeOutFPGAData();
    }
    else if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
    {
        SEND_MSG_PACK msg;
        msg.cmd = CMD_GET_PIC;
        if (m_bLoopModeEnabled)
        {
            mode_index = mode_index % 3;
            if (mode_index == 0)
            {
                msg.value = 3;
            }
            else if (mode_index == 1)
            {
                msg.value = 0;
            }
            else if (mode_index == 2)
            {
                msg.value = 4;
            }
            m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            msg.value2 = 0;
            mode_index++;
        }
        else
        {
            msg.value = 0;
            msg.value2 = 0;
        }
        m_pZYNQDataClient->sendCmd(msg);
    }
}

void CeleX5Widget::onReadBinTimer()
{
    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    {
        if (!m_pCeleX5->readPlayBackData())
        {
            m_pReadBinTimer->start(READ_BIN_TIMER);
        }
    }
    else if (m_emDeviceType == CeleX5::CeleX5_MIPI)
    {
        if (!m_pCeleX5->readBinFileData())
        {
            m_pReadBinTimer->start(READ_BIN_TIMER);
        }
    }
}

void CeleX5Widget::onUpdatePlayInfo()
{
    if (m_pCeleX5->getTotalPackageCount() != getSliderMax(m_pPlaybackBg, "DataCount"))
        setSliderMaxValue(m_pPlaybackBg, "DataCount", m_pCeleX5->getTotalPackageCount());
    //cout << "------------------- " << m_pCeleX5->getCurrentPackageNo() << endl;
    for (int i = 0; i < m_pPlaybackBg->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)m_pPlaybackBg->children().at(i);
        if (pWidget->objectName() == "DataCount")
        {
            pWidget->updateValue(m_pCeleX5->getCurrentPackageNo());
            if (m_pCeleX5->getTotalPackageCount() == m_pCeleX5->getCurrentPackageNo())
            {
                m_pUpdateTimer->stop();
            }
            break;
        }
    }
    QString qsMinute = m_timeCurrent.minute() > 9 ? QString::number(m_timeCurrent.minute()) : "0" + QString::number(m_timeCurrent.minute());
    QString qsSecond = m_timeCurrent.second() > 9 ? QString::number(m_timeCurrent.second()) : "0" + QString::number(m_timeCurrent.second());
    m_pLabelCurrentTime->setText(qsMinute + ":" + qsSecond);

    m_timeCurrent = m_timeCurrent.addSecs(1);
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
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 0;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Event_Optical_Flow Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 1;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Event_Optical_Flow_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event OpticalFlow Pic");
        }
        else if (mode == "Event_Intensity Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 2;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            m_pCbBoxImageType->insertItem(1, "Event Gray Pic");
            m_pCbBoxImageType->insertItem(2, "Event Accumulative Pic");
            m_pCbBoxImageType->setCurrentIndex(1);
        }
        else if (mode == "Full_Picture Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 3;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Full_Picture_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full Pic");
        }
        else if (mode == "Full_Optical_Flow_S Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 4;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Full_Optical_Flow_M Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 5;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_M_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Full_Optical_Flow_Test Mode")
        {
            if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
            {
                SEND_MSG_PACK msg;
                msg.cmd = CMD_SET_FIXED_MODE;
                msg.value = 6;
                m_pZYNQDataClient->sendCmd(msg);
                m_pSensorDataObserver->setSensorMode((CeleX5::CeleX5Mode)msg.value);
            }
            else
            {
                m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_Test_Mode);
            }
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        QPushButton* pButton = (QPushButton*)m_pButtonGroup->button(7);
        pButton->setText("Configurations");
        setButtonNormal(pButton);
    }
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

void CeleX5Widget::showMoreParameters(int index)
{
    if (!m_pCeleX5Cfg)
    {
        m_pCeleX5Cfg = new CeleX5Cfg(m_pCeleX5);
        //m_pCeleX5Cfg->setTestWidget(this);
    }
    m_pCeleX5Cfg->setCurrentIndex(index);
    m_pCeleX5Cfg->show();
}
