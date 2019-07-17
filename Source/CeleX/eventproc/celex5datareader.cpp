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

#include "celex5datareader.h"
#include <iostream>

#define gravity 9.80665
#define accResolution /*(2 * gravity / (std::pow(2.0,15) -1))*/ 0.000598569
#define resolution /*(250.0 / (std::pow(2.0, 15)-1) / 180 * CV_PI)*/ 0.000133162
#define magResolution /*(4800.0 / (std::pow(2.0, 15)-1))*/ 0.146489
#define timePrecise 0.32768

using namespace std;

CeleX5DataReader::CeleX5DataReader()
	: m_uiRow(0)
	, m_uiLastRow(0)
	, m_uiColumn(0)
	, m_uiADC(0)
	, m_uiTFromFPGA(1)
	, m_uiLastTFromFPGA(0)
	, m_uiEventType(0)
	, m_uiSensorMode(0)
	, m_uiClockRate(40)
{
}

CeleX5DataReader::~CeleX5DataReader()
{
}

//Byte 0: {1'b1, row[6:0]}
//Byte 1: {1'b1, row[9:7], time[4:1]}
//Byte 2: {1'b1, time[11:5]}
//Byte 3: {1'b10, time[17:12]}
bool CeleX5DataReader::isRowEvent(unsigned char data[EVENT_SIZE])
{
	return ((unsigned char)data[0] & 0x80) > 0 && !isSpecialEvent(data);
}

//Byte 0: {1'b0, col[6:0]}
//Byte 1: {1'b0, col[10:7], A[2:0]}
//Byte 2: {3'b0, A[9:3]}
//Byte 3: {2'b00, A[11:10], mode[2:0], 1'b0}
bool CeleX5DataReader::isColumnEvent(unsigned char data[EVENT_SIZE])
{
	bool result = ((static_cast<unsigned char>(data[0]) & 0x80) == 0);
	if (result)
		m_uiEventType = (unsigned int)(static_cast<unsigned char>(data[3]) & 0x0E) >> 1; //8'b0000 1110
	return result;
}

//Byte 0: {8'b11111111}
//Byte 1: {8'b11111111}
//Byte 2: {8'b11111111}
//Byte 3: {8'b11111111}
bool CeleX5DataReader::isSpecialEvent(unsigned char data[EVENT_SIZE])
{
	//unsigned char a = static_cast<unsigned char>(data[0]);
	//unsigned char b = static_cast<unsigned char>(data[1]);
	//unsigned char c = static_cast<unsigned char>(data[2]);
	//unsigned char d = static_cast<unsigned char>(data[3]);

	//return (a == 0xff && b == 0xff && c == 0xff);

	unsigned char a = (unsigned char)data[0];
	unsigned char b = (unsigned char)data[1];
	unsigned char c = (unsigned char)data[2];
	unsigned char d = (unsigned char)data[3];

	bool result = (d == 0x89 && c == 0xAB && b == 0xCD && (0xF0 & a) == 0xE0);
	if (result)
		m_uiSensorMode = (0x07 & a);
	return result;
}

//Byte 0: {1'b1, row[6:0]}
//Byte 1: {1'b1, row[9:7], time[4:1]}
//Byte 2: {1'b1, time[11:5]}
//Byte 3: {1'b10, time[17:12]}
void CeleX5DataReader::parseRowEvent(unsigned char data[EVENT_SIZE])
{
	m_uiLastRow = m_uiRow;
	m_uiRow = getRow(data);
	m_uiLastTFromFPGA = m_uiTFromFPGA;
	m_uiTFromFPGA = getTimeStamp(data);
}

//Byte 0: {1'b0, col[6:0]}
//Byte 1: {1'b0, col[10:7], A[2:0]}
//Byte 2: {3'b0, A[9:3]}
//Byte 3: {2'b00, A[11:10], mode[2:0], 1'b0}
void CeleX5DataReader::parseColumnEvent(unsigned char data[EVENT_SIZE])
{
	m_uiColumn = getColumn(data);
	m_uiADC = getADC(data);
}

void CeleX5DataReader::parseRowEventEx(unsigned char data[EVENT_SIZE])
{
	unsigned int byte1 = static_cast<unsigned char>(data[0]) & 0x70; //8'b0111 0000, need >> 4
	unsigned int byte2 = static_cast<unsigned char>(data[1]) & 0x70; //8'b0111 0000, need >> 4
	unsigned int byte3 = static_cast<unsigned char>(data[2]) & 0x70; //8'b0111 0000, need >> 4
	unsigned int byte4 = static_cast<unsigned char>(data[4]) & 0x10; //8'b0001 0000, need >> 4

	m_uiRow = (byte1 >> 4) + (byte2)+(byte3 << 4) + (byte4 << 8);
}

bool CeleX5DataReader::parseFPGAEvent(unsigned char data[EVENT_SIZE], unsigned int* col1, unsigned int* col2) //16bits for no adc event
{
	bool bRowEvent = false;
	//cout << "m_uiRow = " << (int)data[3] << ", " << (int)data[2] << ", " << (int)data[1] << ", " << (int)data[0] << endl;
	if ((data[3] & 0x80) == 0)
	{
		m_uiLastRow = m_uiRow;
		m_uiRow = ((0x03 & data[3]) << 8) + data[2];

		m_uiLastTFromFPGA = m_uiTFromFPGA;
		m_uiTFromFPGA = (0x7C & data[3]) >> 2;

		bRowEvent = true;
	}
	else
	{
		*col1 = ((0x07 & data[3]) << 8) + data[2];
	}

	if ((data[1] & 0x80) == 0)
	{
		//all 0
	}
	else
	{
		*col2 = ((0x07 & data[1]) << 8) + data[0];
	}
	return bRowEvent;
}

bool CeleX5DataReader::parseFPGAEventEx(unsigned char data[EVENT_SIZE], unsigned int* col1, unsigned int* adc1, unsigned int* col2, unsigned int* adc2) //24bits for adc event
{
	bool bRowEvent = false;
	if ((data[3] & 0x80) == 0 && (data[2] & 0x80) == 0 && (data[1] & 0x80) == 0 && (data[0] & 0x80) == 0x80) //0001 row event
	{
		//cout << "-------- " << (int)(data[3] & 0x80) << "-- " << (int)(data[2] & 0x80) << "-- " << (int)(data[1] & 0x80) << "-- " << (int)(data[0] & 0x80) << endl;
		m_uiRow = ((0x07 & data[2]) << 7) + (0x7F & data[1]);
		m_uiLastTFromFPGA = m_uiTFromFPGA;
		m_uiTFromFPGA = ((0x7F & data[3]) << 4) + ((0x78 & data[2]) >> 3);
		//cout << "---------------------------------- m_uiRow = " << m_uiRow << ", m_uiTFromFPGA = " << m_uiTFromFPGA << endl;
		bRowEvent = true;
	}
	else if ((data[3] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[1] & 0x80) == 0x80 && (data[0] & 0x80) == 0) //0110 col event
	{
		*col1 = ((0x0F & data[3]) << 7) + (0x7F & data[2]);
		*adc1 = ((0x7F & m_lastData[0]) << 3) + ((0x70 & data[3]) >> 4);
		//cout << "--- m_uiColumn = " << m_uiColumn << ", m_uiADC = " << m_uiADC << endl;
	}
	else if ((data[3] & 0x80) == 0x80 && (data[2] & 0x80) == 0x80 && (data[1] & 0x80) == 0 && (data[0] & 0x80) == 0x80) //1101 col event
	{
		*col1 = ((0x0F & m_lastData[0]) << 7) + (0x7F & data[3]);
		*adc1 = ((0x7F & m_lastData[1]) << 3) + ((0x70 & m_lastData[0]) >> 4);
		//cout << "------ m_uiColumn = " << m_uiColumn << ", m_uiADC = " << m_uiADC << endl;

		*col2 = ((0x0F & data[1]) << 7) + (0x7F & data[0]);
		*adc2 = ((0x7F & data[2]) << 3) + ((0x70 & data[1]) >> 4);
		//cout << "--------- m_uiColumn1 = " << m_uiColumn1 << ", m_uiADC1 = " << m_uiADC1 << endl;
	}
	else if ((data[3] & 0x80) == 0x80 && (data[2] & 0x80) == 0 && (data[1] & 0x80) == 0x80 && (data[0] & 0x80) == 0x80) //1011 col event
	{
		*col1 = ((0x0F & data[2]) << 7) + (0x7F & data[1]);
		*adc1 = ((0x7F & data[3]) << 3) + ((0x70 & data[2]) >> 4);
		//cout << "------------ m_uiColumn = " << m_uiColumn << ", m_uiADC = " << m_uiADC << endl;
	}
	else
	{
		//cout << "--------------- " << (int)(data[3] & 0x80) << "-- " << (int)(data[2] & 0x80) << "-- " << (int)(data[1] & 0x80) << "-- " << (int)(data[0] & 0x80) << endl;
	}
	for (int i = 0; i < EVENT_SIZE; i++)
		m_lastData[i] = data[i];
	return bRowEvent;
}

//-- CeleX4
//Byte 0: {1'b0, X[6:0]}
//Byte 1: {1'b0, C[0], X[8:7], A[3:0]}
//-- CeleX5
//Byte 0: {1'b0, col[6:0]}
//Byte 1: {1'b0, col[10:7]), A[2:0]}
unsigned int CeleX5DataReader::getColumn(unsigned char data[EVENT_SIZE])
{
	unsigned int low7bit = static_cast<unsigned char>(data[0]) & 0x7F;  //8'b0111 1111
	unsigned int high4bit = static_cast<unsigned char>(data[1]) & 0x78; //8'b0111 1000, need >> 3

	return low7bit + (high4bit << 4); // << (7 - 3)
}

//-- CeleX4
//Byte 0: {1'b1, Y[6:0]}
//Byte 1: {1'b1, Y[9:7], T[3:0]}
//-- CeleX5
//Byte 0: {1'b1, row[6:0]}
//Byte 1: {1'b1, row[9:7], time[4:1]}
unsigned int CeleX5DataReader::getRow(unsigned char data[EVENT_SIZE])
{
	unsigned int low7bit = static_cast<unsigned char>(data[0]) & 0x7F;  //8'b0111 1111
	unsigned int high3bit = static_cast<unsigned char>(data[1]) & 0x70; //8'b0111 0000, need >> 4
	return low7bit + (high3bit << 3); // << (7 - 4)
}

//-- CeleX4
//Byte 1: {1'b1, Y[9,7], T[3:0]}
//Byte 2: {1'b1, T[10:4]}
//Byte 3: {1'b1, T[17:11]}
//-- CeleX5
//Byte 1: {1'b1, row[9:7], time[4:1]}
//Byte 2: {1'b1, time[11:5]}
//Byte 3: {1'b10, time[17:12]} 
unsigned int CeleX5DataReader::getTimeStamp(unsigned char data[EVENT_SIZE])
{
	unsigned int low5bit = static_cast<unsigned int>(data[1] & 0x0F) << 1; //8'b0000 1111
	unsigned int mid7bit = static_cast<unsigned int>(data[2] & 0x7F);      //8'b0111 1111
	unsigned int high6bit = static_cast<unsigned int>(data[3] & 0x3F);     //8'b0011 1111
	return low5bit + (mid7bit << 5) + (high6bit << 12);
}

//-- CeleX4
//Byte 1: {1'b0, C[0], X[8:7], A[3:0]}
//Byte 2: {3'b0, A[8:4]}
//-- CeleX5
//Byte 1: {1'b0, col[10:7], A[2:0]}
//Byte 2: {3'b0, A[9:3]}
//Byte 3: {2'b00, A[11:10], mode[2:0], 1'b0}
unsigned int CeleX5DataReader::getADC(unsigned char data[EVENT_SIZE]) //ADC
{
	unsigned int low3bit = static_cast<unsigned int>(data[1] & 0x07);  //8'b0000 0111
	unsigned int mid7bit = static_cast<unsigned int>(data[2] & 0x7F);  //8'b0111 1111
	unsigned int high2bit = static_cast<unsigned int>(data[3] & 0x30); //8'b0011 0000, need >> 4
	return low3bit + (mid7bit << 3) + (high2bit << 6); // << (10 - 4)
}


//Byte 0: {8'b11111110}
//Byte 1: {8'b11011100}
//Byte 2: {8'b10111010}
//Byte 3: {8'b10011000}
bool CeleX5DataReader::isIMUSpecialEvent(unsigned char data[EVENT_SIZE])
{
	unsigned char a = (unsigned char)data[0];
	unsigned char b = (unsigned char)data[1];
	unsigned char c = (unsigned char)data[2];
	unsigned char d = (unsigned char)data[3];

	bool result = (d == 0xFE && c == 0xDC && b == 0xBA && a == 0x98);
	//if (result)
	//	cout << "----------IMU special---------" << endl;
	return result;
}

bool CeleX5DataReader::isIMUSpecialEventEnd(unsigned char data[EVENT_SIZE])
{
	unsigned char a = (unsigned char)data[0];
	unsigned char b = (unsigned char)data[1];
	unsigned char c = (unsigned char)data[2];
	unsigned char d = (unsigned char)data[3];

	bool result = (d == 0xFE && c == 0xDC && b == 0xBA && a == 0x9A) || (d == 0xFE && c == 0xDC && b == 0xBA && a == 0x99);
	//if (result)
	//	cout << "----------IMU special End---------" << endl;
	return result;
}

IMUData CeleX5DataReader::parseIMUData(unsigned char data[EVENT_SIZE], uint64_t frameNo, uint32_t tCounter)
{
	imuRawData.frameNo = frameNo + 1;
	imuRawData.time_stamp = (128 * tCounter)/(1000* m_uiClockRate);
	//cout << "-----------parse IMU Data--------------" << int((data[3] & 0xC0) >> 6) << endl;
	//cout << hex << int(data[0]) << ", " << int(data[1]) << ", " << int(data[2]) << ", " << int(data[3]) << endl;
	//  GYROS_byte0 = {1'b1,  x_gyros[6 : 0] };
	//	GYROS_byte1 = { 1'b0,  x_gyros[13: 7] };
	//	GYROS_byte2 = { 1'b1,  time_cnt[17:14], i2c_err_ind, x_gyros[15:14] };  // precise : 0.32768 ms
	//	GYROS_byte3 = { 2'b00, y_gyros[5:0]};

	if ((data[0] & 0x80) == 0x80 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0)
	{
		int16_t low7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t mid7bit = static_cast<int16_t>(data[1] & 0x7F);
		int16_t high2bit = static_cast<int16_t>(data[2] & 0x03);

		m_iHighGYROS_T = static_cast<int16_t>(data[2] & 0xF0);	//time

		m_iLowGYROS_Y = static_cast<int16_t>(data[3] & 0x3F);

		int16_t x_GYROS = low7bit + (mid7bit << 7) + (high2bit << 14);
		imuRawData.x_GYROS = x_GYROS * resolution;
		//cout << "x_GYROS:" << x_GYROS << ", imuRawData.x_GYROS:" << imuRawData.x_GYROS << endl;
	}
	//	GYROS_byte4 = { 1'b1,  y_gyros[12: 6] };
	//	GYROS_byte5 = { 1'b0,  z_gyros[3:0],y_gyros[15:13] };
	//	GYROS_byte6 = { 1'b1,  time_cnt[13], z_gyros[9:4] };  // precise : 0.32768 ms
	//	GYROS_byte7 = { 2'b01, z_gyros[15:10]};
	else if ((data[0] & 0x80) == 0x80 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0x40)
	{
		int16_t low6bit = m_iLowGYROS_Y;
		int16_t mid7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t high3bit = static_cast<int16_t>(data[1] & 0x07);

		int16_t y_GYROS = low6bit + (mid7bit << 6) + (high3bit << 13);
		imuRawData.y_GYROS = y_GYROS * resolution;

		int16_t low4bit = static_cast<int16_t>(data[1] & 0x78);
		int16_t mid6bit = static_cast<int16_t>(data[2] & 0x3F);
		int16_t high6bit = static_cast<int16_t>(data[3] & 0x3F);

		int16_t z_GYROS = low4bit + (mid6bit << 4) + (high6bit << 10);
		imuRawData.z_GYROS = z_GYROS * resolution;

		int16_t low1bit = static_cast<int16_t>(data[2] & 0x40);
		int16_t t_GYROS = low1bit + (m_iHighGYROS_T << 1);
		imuRawData.t_GYROS = t_GYROS * timePrecise;
	}
	//	ACC_byte0 = { 1'b1,  x_acc[6 : 0] };
	//	ACC_byte1 = { 1'b0,  x_acc[13: 7] };
	//	ACC_byte2 = { 1'b1,  time_cnt[17:14], i2c_err_ind, x_acc[15:14] };  // precise : 0.32768 ms
	//	ACC_byte3 = { 2'b10, y_acc[5:0]};
	else if ((data[0] & 0x80) == 0x80 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0x80)
	{
		int16_t low7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t mid7bit = static_cast<int16_t>(data[1] & 0x7F);
		int16_t high2bit = static_cast<int16_t>(data[2] & 0x03);
		m_iHighACC_T = static_cast<int16_t>(data[2] & 0xF0);	//time

		m_iLowACC_Y = static_cast<int16_t>(data[3] & 0x3F);

		int16_t x_ACC = low7bit + (mid7bit << 7) + (high2bit << 14);
		imuRawData.x_ACC = x_ACC * accResolution;
		//cout << "x_ACC:" << x_ACC << ", imuRawData.x_ACC:" << imuRawData.x_ACC << endl;
	}
	//	ACC_byte4 = { 1'b1,  y_acc[12: 6] };
	//	ACC_byte5 = { 1'b0,  z_acc[3:0],y_acc[15:13] };
	//	ACC_byte6 = { 1'b1,  time_cnt[13], z_acc[9:4] };  // precise : 0.32768 ms
	//	ACC_byte7 = { 2'b11, z_acc[15:10]};
	else if ((data[0] & 0x80) == 0x80 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0xC0)
	{
		int16_t low6bit = m_iLowACC_Y;
		int16_t mid7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t high3bit = static_cast<int16_t>(data[1] & 0x07);
		m_iHighACC_T = static_cast<int16_t>(data[2] & 0xF0);	//time

		int16_t y_ACC = low6bit + (mid7bit << 6) + (high3bit << 13);
		imuRawData.y_ACC = y_ACC * accResolution;
		//cout << "y_ACC:" << y_ACC << ", imuRawData.y_ACC:" << imuRawData.y_ACC << endl;

		int16_t low4bit = static_cast<int16_t>(data[1] & 0x78);
		int16_t mid6bit = static_cast<int16_t>(data[2] & 0x3F);
		int16_t high6bit = static_cast<int16_t>(data[3] & 0x3F);

		int16_t z_ACC = low4bit + (mid6bit << 4) + (high6bit << 10);
		imuRawData.z_ACC = z_ACC * accResolution;

		int16_t low1bit = static_cast<int16_t>(data[2] & 0x40);
		int16_t t_ACC = low1bit + (m_iHighACC_T << 1);
		imuRawData.t_ACC = t_ACC * timePrecise;
	}
	//	Magn_byte0 = { 1'b0,  x_magn[6 : 0] };
	//	Magn_byte1 = { 1'b0,  x_magn[13: 7] };
	//	Magn_byte2 = { 1'b1,  time_cnt[17:14], i2c_err_ind, x_magn[15:14] };  // precise : 0.32768 ms
	//	Magn_byte3 = { 2'b00, y_magn[5:0]};
	else if ((data[0] & 0x80) == 0 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0)
	{
		int16_t low7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t mid7bit = static_cast<int16_t>(data[1] & 0x7F);
		int16_t high2bit = static_cast<int16_t>(data[2] & 0x03);
		m_iHighMAG_T = static_cast<int16_t>(data[2] & 0xF0);	//time

		m_iLowMAG_Y = static_cast<int16_t>(data[3] & 0x3F);

		int16_t x_MAG = low7bit + (mid7bit << 7) + (high2bit << 14);
		imuRawData.x_MAG = x_MAG * magResolution;
	}
	//	Magn_byte4 = { 1'b0,  y_magn[12: 6] };
	//	Magn_byte5 = { 1'b0,  z_magn[3:0],y_magn[15:13] };
	//	Magn_byte6 = { 1'b1,  time_cnt[13], z_magn[9:4] };  // precise : 0.32768 ms
	//	Magn_byte7 = { 2'b01, z_magn[15:10]};
	else if ((data[0] & 0x80) == 0 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0x40)
	{
		int16_t low6bit = m_iLowMAG_Y;
		int16_t mid7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t high3bit = static_cast<int16_t>(data[1] & 0x07);

		int16_t y_MAG = low6bit + (mid7bit << 6) + (high3bit << 13);
		imuRawData.y_MAG = y_MAG * magResolution;

		int16_t low4bit = static_cast<int16_t>(data[1] & 0x78);
		int16_t mid6bit = static_cast<int16_t>(data[2] & 0x3F);
		int16_t high6bit = static_cast<int16_t>(data[3] & 0x3F);

		int16_t z_MAG = low4bit + (mid6bit << 4) + (high6bit << 10);
		imuRawData.z_MAG = z_MAG * magResolution;

		int16_t low1bit = static_cast<int16_t>(data[2] & 0x40);
		int16_t t_MAG = low1bit + (m_iHighMAG_T << 1);
		imuRawData.t_MAG = t_MAG * timePrecise;
	}
	//	Temp_byte0 = { 1'b0,  temperature[6 : 0] };
	//	Temp_byte1 = { 1'b0,  temperature[13: 7] };
	//	Temp_byte2 = { 1'b1,  time_cnt[17:14], i2c_err_ind, temperature[15:14] };  // precise : 0.32768 ms
	//	Temp_byte3 = { 2'b10, pad_y[5:0]};
	else if ((data[0] & 0x80) == 0 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0x80)
	{
		int16_t low7bit = static_cast<int16_t>(data[0] & 0x7F);
		int16_t mid7bit = static_cast<int16_t>(data[1] & 0x7F);
		int16_t high2bit = static_cast<int16_t>(data[2] & 0x03);

		int16_t temp = low7bit + (mid7bit << 7) + (high2bit << 14);
		imuRawData.x_TEMP = 21 + (temp / 333.87);
		//cout << "temp:" << temp << ", imuRawData.temp:" << imuRawData.x_TEMP << endl;
	}
	//	Temp_byte4 = { 1'b0,  pad_y[12: 6] };
	//	Temp_byte5 = { 1'b0,  pad_z[3:0],pad_y[15:13] };
	//	Temp_byte6 = { 1'b1,  time_cnt[13], pad_z[9:4] };  // precise : 0.32768 ms
	//	Temp_byte7 = { 2'b11, pad_z[15:10]};
	else if ((data[0] & 0x80) == 0 && (data[1] & 0x80) == 0 && (data[2] & 0x80) == 0x80 && (data[3] & 0xC0) == 0xC0)
	{

	}
	else
	{

	}
	return imuRawData;
}
