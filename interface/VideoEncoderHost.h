/*
 *  VideoEncoderHost.h- basic va decoder for video
 *
 *  Copyright (C) 2013 Intel Corporation
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

#ifndef VIDEO_ENCODER_HOST_H_
#define VIDEO_ENCODER_HOST_H_

#include "VideoEncoderInterface.h"
extern "C" { // for dlsym usage

/** \file VideoEncoderHost.h
*/

/** \fn IVideoEncoder *createVideoEncoder(const char *mimeType)
 * \brief create encoder basing on given mimetype
*/
YamiMediaCodec::IVideoEncoder *createVideoEncoder(const char *mimeType);
///brief destroy encoder
void releaseVideoEncoder(YamiMediaCodec::IVideoEncoder * p);

typedef YamiMediaCodec::IVideoEncoder *(*YamiCreateVideoEncoderFuncPtr) (const char *mimeType);
typedef void (*YamiReleaseVideoEncoderFuncPtr)(YamiMediaCodec::IVideoEncoder * p);
}
#endif                          /* VIDEO_ENCODER_HOST_H_ */
