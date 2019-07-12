#ifndef CELEX5WIDGET_H
#define CELEX5WIDGET_H

#include "sliderwidget.h"
#include "cfgslider.h"
#include "doubleslider.h"
#include "./include/celex5/celex5.h"
#include "./include/celex5/celex5datamanager.h"
#include <QTime>
#include <QRadioButton>
#include <QMessageBox>
#include "videostream.h"

using namespace std;

#pragma execution_character_set("utf-8")

enum DisplayType {
    Realtime = 0,
    Playback = 1,
    ConvertBin2Video = 2,
    ConvertBin2CSV = 3
};

class QLabel;
class MainWindow;
class QComboBox;
class SensorDataObserver : public QWidget, public CeleX5DataManager
{
    Q_OBJECT
public:
    SensorDataObserver(CX5SensorDataServer* sensorData, QWidget *parent);
    ~SensorDataObserver();
    virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData); //overrides Observer operation
    void setCeleX5(CeleX5* pCeleX5);
    DisplayType getDisplayType() { return m_emDisplayType; }
    void setLoopModeEnabled(bool enable);
    void setSensorMode(CeleX5::CeleX5Mode mode);
    void setPictureMode(int picMode);
    void setFPS(int count);
    void setPlaybackFPS(int count);
    void setFullFrameFPS(uint16_t value);
    void setMultipleShowEnabled(bool enable);
    void setPlaybackEnabled(bool enable);
    void setDisplayType(DisplayType type);

    void setVideoColorEnable(bool enable);
    void setRecordVideoEnabled(bool enable);
    bool getRecordVideoState(){return m_bRecordVideoEnabled;}
    void setSwitch2NextFileState(bool state);
    bool getSwitch2NextFileState(){return m_bCurrFileFinished;}

public slots:

private:
    void updateImage(unsigned char* pBuffer1, CeleX5::CeleX5Mode mode, int loopNum, int picMode);
    cv::Mat convertTo3Channels(const cv::Mat& mat);
    cv::Mat convertToColored(cv::Mat mat);

protected:
    void paintEvent(QPaintEvent *event);

protected slots:
    void onUpdateImage();
    void onSaveImage2Video();

private:
    QImage                  m_imageMode1;
    QImage                  m_imageMode2;
    QImage                  m_imageMode3;

    CX5SensorDataServer*    m_pSensorData;
    CeleX5*                 m_pCeleX5;
    bool                    m_bLoopModeEnabled;
    int                     m_iPicMode;
    bool                    m_bRealtimeDisplay;
    bool                    m_bShowMultipleWindows; //for fixed mode
    uint16_t                m_uiTemperature;
    uint16_t                m_uiFullFrameFPS;
    uint16_t                m_uiRealFullFrameFPS;
    int                     m_iFPS;
    int                     m_iPlaybackFPS;
    uchar*                  m_pBuffer[3];
    QTimer*                 m_pUpdateTimer;
    CeleX5::CeleX5Mode      m_emSensorMode;

    // for video record and convert
    bool                    m_bRecordVideoEnabled;
    bool                    m_bVideoColorEnable;
    std::queue<cv::Mat>      m_queueForSaving;
    DisplayType             m_emDisplayType;
    bool                    m_bCurrFileFinished;
    std::vector<int>        m_vecEncodeparam;

};

class QHBoxLayout;
class QAbstractButton;
class QPushButton;
class QButtonGroup;
class CeleX5Widget : public QWidget
{
    Q_OBJECT
public:
    explicit CeleX5Widget(QWidget *parent = 0);
    ~CeleX5Widget();
    void closeEvent(QCloseEvent *event);

private:
    QComboBox *createModeComboBox(QString text, QRect rect, QWidget* parent, bool bLoop, int loopNum);
    void createButtons(QHBoxLayout* layout);
    QPushButton* createButton(QString text, QRect rect, QWidget *parent);
    SliderWidget* createSlider(CeleX5::CfgInfo cfgInfo, int value, QRect rect, QWidget *parent, QWidget *widgetSlot);
    void setButtonEnable(QPushButton* pButton);
    void setButtonNormal(QPushButton* pButton);
    void switchMode(QPushButton* pButton);
    void setSliderMaxValue(QWidget* parent, QString objName, int value);
    int  getSliderMax(QWidget* parent, QString objName);
    void showPlaybackBoard(bool show);

    void changeFPN();
    void playback(QPushButton* pButton);

    void record(QPushButton* pButton);
    void recordVideo();
    void recordVideo(QPushButton* pButton);

    void showCFG();
    void showVideoCFG();

    void onBtnRotateLRReleased(QPushButton* pButton);
    void onBtnRotateUDReleased(QPushButton* pButton);
    void convertBins2Videos();
    void convertBin2Video();

signals:

protected slots:
    void onButtonClicked(QAbstractButton* button);
    void onValueChanged(uint32_t value, CfgSlider* slider);
    void onPipeoutDataTimer();
    void onReadBinTimer();
    void onUpdatePlayInfo();
    void onRecordColoredVideo(bool enable);
    void onBtnOKRelease();
    void onRecordVideoTimer();
    void onRecordDataTimer();
    void onSensorModeChanged(QString text);
    void onImageTypeChanged(int index);
    void onShowMultipleWindows();

private:
    CeleX5*             m_pCeleX5;
    QWidget*            m_pCFGWidget;
    QButtonGroup*       m_pButtonGroup;
    //
    QComboBox*          m_pCbBoxFixedMode;
    QComboBox*          m_pCbBoxLoopAMode;
    QComboBox*          m_pCbBoxLoopBMode;
    QComboBox*          m_pCbBoxLoopCMode;
    //
    QComboBox*          m_pCbBoxImageType;
    QPushButton*        m_pBtnShowStyle;
    //
    QWidget*            m_pPlaybackBg;
    DoubleSlider*       m_pSliderPlayer;
    CfgSlider*          m_pFPSSlider;
    CfgSlider*          m_pFrameSlider;

    QLabel*             m_pLabelCurrentTime;
    QLabel*             m_pLabelEndTime;
    QTime               m_timeCurrent;
    //
    QTimer*             m_pPipeOutDataTimer;
    QTimer*             m_pReadBinTimer;
    QTimer*             m_pUpdateTimer;
    QTimer*             m_pRecordVideoTimer;
    QTimer*             m_pRecordDataTimer;

    SensorDataObserver* m_pSensorDataObserver;

    CeleX5::DeviceType  m_emDeviceType;
    bool                m_bLoopModeEnabled;
    //ipconfig
    std::string         m_strIPAddress;
    unsigned short      m_usIPPort;
    unsigned int        m_uiRecordTimeInterval;

    //video record
    QWidget*            m_pVideoCFGWidget;
    int                 m_iVideoFPS;
    std::string         m_strButtonName;
};

#endif // CELEX5WIDGET_H
