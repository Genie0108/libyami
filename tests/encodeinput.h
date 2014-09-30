/*
 *  h264_encode.cpp - h264 encode test
 *
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Cong Zhong<congx.zhong@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "VideoEncoderDefs.h"
#include "VideoEncoderInterface.h"
#include <vector>
#include <tr1/memory>

using namespace YamiMediaCodec;

class EncodeStreamInput;
class EncodeStreamInputFile;
class EncodeStreamInputCamera;
typedef std::tr1::shared_ptr<EncodeStreamInput> EncodeStreamInputPtr;
class EncodeStreamInput {
public:
    static EncodeStreamInputPtr create(const char* inputFileName, uint32_t fourcc, int width, int height);
    EncodeStreamInput() : m_width(0), m_height(0), m_frameSize(0) {};
    ~EncodeStreamInput() {};
    virtual bool init(const char* inputFileName, uint32_t fourcc, int width, int height) = 0;
    virtual bool getOneFrameInput(VideoEncRawBuffer &inputBuffer) = 0;
    virtual bool recycleOneFrameInput(VideoEncRawBuffer &inputBuffer) {return true;};
    virtual bool isEOS() = 0;
    int getWidth() { return m_width;}
    int getHeight() { return m_height;}

protected:
    uint32_t m_fourcc;
    int m_width;
    int m_height;
    int m_frameSize;
};

class EncodeStreamInputFile : public EncodeStreamInput {
public:
    EncodeStreamInputFile();
    ~EncodeStreamInputFile();
    virtual bool init(const char* inputFileName, uint32_t fourcc, int width, int height);
    virtual bool getOneFrameInput(VideoEncRawBuffer &inputBuffer);
    virtual bool isEOS() {return m_readToEOS;}

protected:
    FILE *m_fp;
    uint8_t *m_buffer;
    bool m_readToEOS;
};

class EncodeStreamInputCamera : public EncodeStreamInput {
public:
    enum CameraDataMode{
        CAMERA_DATA_MODE_MMAP,
        // CAMERA_DATA_MODE_DMABUF_MMAP,
        // CAMERA_DATA_MODE_USRPTR,
        // CAMERA_DATA_MODE_DMABUF_USRPTR,
    };
    EncodeStreamInputCamera() :m_frameBufferCount(5), m_frameBufferSize(0), m_dataMode(CAMERA_DATA_MODE_MMAP) {};
    ~EncodeStreamInputCamera();
    virtual bool init(const char* cameraPath, uint32_t fourcc, int width, int height);
    bool setDataMode(CameraDataMode mode = CAMERA_DATA_MODE_MMAP) {m_dataMode = mode; return true;};

    virtual bool getOneFrameInput(VideoEncRawBuffer &inputBuffer);
    virtual bool recycleOneFrameInput(VideoEncRawBuffer &inputBuffer);
    virtual bool isEOS() { return false; }
    // void getSupportedResolution();
private:
    int m_fd;
    CameraDataMode m_dataMode;
    std::vector<uint8_t*> m_frameBuffers;
    uint32_t m_frameBufferCount;
    uint32_t m_frameBufferSize;

    bool openDevice();
    bool initDevice(const char *cameraDevicePath);
    bool initMmap();
    bool startCapture();
    int32_t dequeFrame();
    bool enqueFrame(int32_t index);
    bool stopCapture();
    bool uninitDevice();

};

class EncodeStreamOutput {
public:
    EncodeStreamOutput();
    ~EncodeStreamOutput();
    static  EncodeStreamOutput* create(const char* outputFileName, int width , int height);
    virtual bool write(void* data, int size);
    virtual const char* getMimeType() = 0;
protected:
    virtual bool init(const char* outputFileName, int width , int height);
    FILE *m_fp;
};

class EncodeStreamOutputH264 : public EncodeStreamOutput
{
    virtual const char* getMimeType();
};

class EncodeStreamOutputVP8 : public EncodeStreamOutput {
public:
    EncodeStreamOutputVP8();
    ~EncodeStreamOutputVP8();
    virtual const char* getMimeType();
    virtual bool write(void* data, int size);
protected:
    virtual bool init(const char* outputFileName, int width , int height);
private:
    int m_frameCount;
};

bool createOutputBuffer(VideoEncOutputBuffer* outputBuffer, int maxOutSize);
