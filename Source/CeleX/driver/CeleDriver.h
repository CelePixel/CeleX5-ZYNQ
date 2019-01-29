/*
* Copyright (c) 2017-2018  CelePixel Technology Co. Ltd.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef CELEPIXEL_H
#define CELEPIXEL_H

#include <stdint.h>
#include <vector>

#ifdef __linux__
using namespace std;

class CeleDriver
#else
#ifdef FRONTPANEL_EXPORTS
#define CELEPIXEL_DLL_API __declspec(dllexport)
#else
#define CELEPIXEL_DLL_API __declspec(dllimport)
#endif

using namespace std;

class CELEPIXEL_DLL_API CeleDriver
#endif
{
public:
	CeleDriver(void);
    ~CeleDriver(void);

public:
    bool Open(void);
	bool openUSB(); //added by xiaoqin @2018.11.02
	bool openStream(); //added by xiaoqin @2018.11.02

    void Close(void);
	void closeUSB(); //added by xiaoqin @2018.11.07
	void closeStream(); //added by xiaoqin @2018.11.07

	bool writeSerialNumber(std::string number); //added by xiaoqin @2018.12.06
	std::string getSerialNumber(); //added by xiaoqin @2018.12.06
	std::string getFirmwareVersion(); //added by xiaoqin @2018.12.06
	std::string getFirmwareDate(); //added by xiaoqin @2018.12.06

    bool getimage(vector<uint8_t> &image);
	void clearData();

public:
    bool i2c_set(uint16_t reg,uint16_t value);
    bool i2c_get(uint16_t reg,uint16_t &value);
    bool mipi_set(uint16_t reg,uint16_t value);
    bool mipi_get(uint16_t reg,uint16_t &value);
};

#endif // CELEPIXEL_H
