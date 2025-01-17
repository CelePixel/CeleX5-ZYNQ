# CeleX5-SDK
SDK for CeleX5 sensor.

![Structure](https://github.com/CelePixel/CeleX5-Zynq/blob/master/Source/CeleX5Demo_Client/CeleX5-Zynq_Structure.png)

* CeleX<sup>TM</sup> is a family of smart image sensor, specially designed for machine vision. Each pixel in CeleX<sup>TM</sup>
sensor can individually monitor the relative change in light intensity and report an event if a threshold is
reached.

* The output of the sensor is not a frame, but a stream of asynchronous digital events. The speed of the sensor
is not limited by any traditional concept such as exposure time and frame rate. It can detect fast motion
which is traditionally captured by expensive, high speed cameras running at thousands of frames per second,
but with drastically reduced amount of data.

* Our technology allows post-capture change of frame-rate for video playback. One can view the video at
10,000 frames per second to see high speed events or at normal rate of 25 frames per second.

* This SDK provides an easy-to-use software interface to get data from the sensor and communicate with the
sensor, and it is consistent across the Windows (32-/64-bit) and Linux (32-/64-bit) development
environments. In addition, it provides both pure C++ interfaces without any third libraries and
OpenCV-based interfaces to obtain data from the sensor.

* CeleX5 is a multifunctional smart image sensor with 1Mega-pixels.

* This SDK provides three working modes of CeleX5 Sensor: Full-frame Picture data, Event data, and Full-frame Optical-Flow data. CeleX5 also provides Loop Mode data which alternately renders Full-frame Picture data, Event data, and Full-frame Optical-Flow data.

`The CeleX5-SDK is structured as follows:`

* _DemoGUI_: CeleX5 Demo GUI execution (Windows and Linux).
* _Documentation_:
  * _CeleX5_SDK_Reference_: The introduction of CeleX5 sensor and the references of all the classes and functions in the SDK.
  * _CeleX5_SDK_Getting_Started_Guide_: The instructions to use the CeleX5 sensor demo kit, install CX3 USB3.0 driver, run the CeleXDemo GUI and compile the source code.
* _SD_Card_: 
  * _boot_: System files required to boot the CeleX5 ZYNQ board.
  * _root_: Demo executable program and start.sh file.
* _Sources_:
  * _CeleX_: Source code of CeleX5 SDK.
  * _CeleX5Demo_Client_: Source code (developed with Qt) of Demo Client.
  * _CeleX5Demo_Server_: Source code of Demo Server.
* _Samples_: Several examples developed based on SDK and a sample user manual file.
* _ReleaseNotes.txt_: New features, fixed bugs and SDK development environment.
