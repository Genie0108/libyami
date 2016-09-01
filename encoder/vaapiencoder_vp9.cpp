/*
 * Copyright (C) 2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vaapiencoder_vp9.h"
#include "common/scopedlogger.h"
#include "common/common_def.h"
#include "vaapi/vaapicontext.h"
#include "vaapi/vaapidisplay.h"
#include "vaapicodedbuffer.h"
#include "vaapiencpicture.h"
#include "vaapiencoder_factory.h"
#include <algorithm>

namespace YamiMediaCodec {

// each frame can have at most 3 reference frames, golden, alt and last
enum maxSizeValues {
    kMaxReferenceFrames = 8,
    kMaxWidth = 4096,
    kMaxHeight = kMaxWidth,
    kMaxHeaderSize=kMaxWidth
};

enum VP9QPValues { kMinQPValue = 9, kDefaultQPValue = 60, kMaxQPValue = 127 };

enum VP9FrameType { kKeyFrame, kInterFrame };

enum VP9LevelValues { kDefaultSharpnessLevel, kDefaultFilterLevel = 10 };

class VaapiEncPictureVP9 : public VaapiEncPicture {
public:
    VaapiEncPictureVP9(const ContextPtr& context, const SurfacePtr& surface,
                       int64_t timeStamp)
        : VaapiEncPicture(context, surface, timeStamp)
    {
        return;
    }

    VAGenericID getCodedBufferID() { return m_codedBuffer->getID(); }
};

VaapiEncoderVP9::VaapiEncoderVP9()
    : m_frameCount(0)
{
    m_videoParamCommon.profile = VAProfileVP9Profile0;
    m_videoParamCommon.rcParams.minQP = kMinQPValue;
    m_videoParamCommon.rcParams.maxQP = kMaxQPValue;
    m_videoParamCommon.rcParams.initQP = kDefaultQPValue;

    // add extra surfaces to operate due to kMaxReferenceFrames
    // vaapi_encoder class will create 5 extra surfaces already
    m_maxOutputBuffer = kMaxReferenceFrames;
}

VaapiEncoderVP9::~VaapiEncoderVP9() {}

YamiStatus VaapiEncoderVP9::getMaxOutSize(uint32_t* maxSize)
{
    FUNC_ENTER();
    *maxSize = m_maxCodedbufSize;
    return YAMI_SUCCESS;
}

void VaapiEncoderVP9::resetParams()
{
    m_maxCodedbufSize = width() * height() * 3 / 2;

    // adding extra padding.  In particular small resolutions require more
    // space depending on other quantization parameters during execution. The
    // value below is a good compromise
    m_maxCodedbufSize += kMaxHeaderSize;
}

YamiStatus VaapiEncoderVP9::start()
{
    FUNC_ENTER();
    resetParams();
    return VaapiEncoderBase::start();
}

void VaapiEncoderVP9::flush()
{
    FUNC_ENTER();
    m_frameCount = 0;
    m_reference.clear();
    VaapiEncoderBase::flush();
}

YamiStatus VaapiEncoderVP9::stop()
{
    flush();
    return VaapiEncoderBase::stop();
}

YamiStatus VaapiEncoderVP9::setParameters(VideoParamConfigType type,
                                          Yami_PTR videoEncParams)
{
    YamiStatus status = YAMI_SUCCESS;
    FUNC_ENTER();
    if (!videoEncParams)
        return YAMI_INVALID_PARAM;

    switch (type) {
    default:
        status = VaapiEncoderBase::setParameters(type, videoEncParams);
        break;
    }
    return status;
}

YamiStatus VaapiEncoderVP9::getParameters(VideoParamConfigType type,
                                          Yami_PTR videoEncParams)
{
    FUNC_ENTER();
    if (!videoEncParams)
        return YAMI_INVALID_PARAM;

    return VaapiEncoderBase::getParameters(type, videoEncParams);
}

YamiStatus VaapiEncoderVP9::doEncode(const SurfacePtr& surface,
                                     uint64_t timeStamp, bool forceKeyFrame)
{
    YamiStatus ret;
    if (!surface)
        return YAMI_INVALID_PARAM;

    PicturePtr picture(new VaapiEncPictureVP9(m_context, surface, timeStamp));

    m_frameCount %= keyFramePeriod();
    picture->m_type = (m_frameCount ? VAAPI_PICTURE_P : VAAPI_PICTURE_I);
    m_frameCount++;

    CodedBufferPtr codedBuffer
        = VaapiCodedBuffer::create(m_context, m_maxCodedbufSize);
    if (!codedBuffer)
        return YAMI_OUT_MEMORY;
    picture->m_codedBuffer = codedBuffer;
    codedBuffer->setFlag(ENCODE_BUFFERFLAG_ENDOFFRAME);
    if (picture->m_type == VAAPI_PICTURE_I) {
        codedBuffer->setFlag(ENCODE_BUFFERFLAG_SYNCFRAME);
    }
    ret = encodePicture(picture);
    if (ret != YAMI_SUCCESS) {
        return ret;
    }
    output(picture);
    return YAMI_SUCCESS;
}

bool VaapiEncoderVP9::fill(VAEncSequenceParameterBufferVP9* seqParam) const
{
    seqParam->max_frame_width = kMaxWidth;
    seqParam->max_frame_height = kMaxHeight;

    seqParam->bits_per_second = bitRate();
    seqParam->intra_period = intraPeriod();
    seqParam->kf_min_dist = 1;
    seqParam->kf_max_dist = intraPeriod();

    return true;
}

// Fills in VA picture parameter buffer
bool VaapiEncoderVP9::fill(VAEncPictureParameterBufferVP9* picParam,
                           const PicturePtr& picture,
                           const SurfacePtr& surface) const
{
    picParam->reconstructed_frame = surface->getID();
    picParam->coded_buf = picture->getCodedBufferID();

    if (picture->m_type == VAAPI_PICTURE_I) {
        for (uint32_t i = 0; i < kMaxReferenceFrames ; i++)
            picParam->reference_frames[i] = VA_INVALID_SURFACE;
    }
    else {
        picParam->refresh_frame_flags = 0x01; // refresh last frame
        picParam->pic_flags.bits.frame_type = kInterFrame;

        ReferenceQueue::const_iterator it = m_reference.begin();

        for (uint32_t i = 0; it != m_reference.end(); ++it, i++)
            picParam->reference_frames[i] = (*it)->getID();

        // last/golden/alt is used as reference frame. L0 forward
        picParam->ref_flags.bits.ref_frame_ctrl_l0 = 0x7;

        // golden and alt are last KeyFrame
        // last is last decoded frame
        picParam->ref_flags.bits.ref_last_idx = 0;
        picParam->ref_flags.bits.ref_gf_idx = 1;
        picParam->ref_flags.bits.ref_arf_idx = 2;
        picParam->pic_flags.bits.frame_context_idx = 0;
    }

    picParam->frame_width_src = width();
    picParam->frame_height_src = height();
    picParam->frame_width_dst = width();
    picParam->frame_height_dst = height();

    picParam->pic_flags.bits.show_frame = 1;

    picParam->luma_ac_qindex = kDefaultQPValue;
    picParam->luma_dc_qindex_delta = 1;
    picParam->chroma_ac_qindex_delta = 1;
    picParam->chroma_dc_qindex_delta = 1;

    picParam->filter_level = kDefaultFilterLevel;
    picParam->sharpness_level = kDefaultSharpnessLevel;

    return true;
}

bool VaapiEncoderVP9::fill(
    VAEncMiscParameterTypeVP9PerSegmantParam* segParam) const
{
    return true;
}

bool VaapiEncoderVP9::ensureSequence(const PicturePtr& picture)
{
    if (picture->m_type != VAAPI_PICTURE_I)
        return true;

    VAEncSequenceParameterBufferVP9* seqParam;
    if (!picture->editSequence(seqParam) || !fill(seqParam)) {
        ERROR("failed to create sequence parameter buffer (SPS)");
        return false;
    }
    return true;
}

bool VaapiEncoderVP9::ensurePicture(const PicturePtr& picture,
                                    const SurfacePtr& surface)
{
    VAEncPictureParameterBufferVP9* picParam;

    if (!picture->editPicture(picParam) || !fill(picParam, picture, surface)) {
        ERROR("failed to create picture parameter buffer (PPS)");
        return false;
    }
    return true;
}

bool VaapiEncoderVP9::ensureQMatrix(const PicturePtr& picture)
{
    VAEncMiscParameterTypeVP9PerSegmantParam* segmentParam;

    if (picture->m_type != VAAPI_PICTURE_I)
        return true;

    if (!picture->editQMatrix(segmentParam) || !fill(segmentParam)) {
        ERROR("failed to create qMatrix");
        return false;
    }
    return true;
}

bool VaapiEncoderVP9::referenceListUpdate(const PicturePtr& pic,
                                          const SurfacePtr& recon)
{

    if (pic->m_type == VAAPI_PICTURE_I) {
        m_reference.clear();
        m_reference.insert(m_reference.end(), kMaxReferenceFrames, recon);
    }
    else {
        m_reference.pop_front();
        m_reference.push_front(recon);
    }

    return true;
}

YamiStatus VaapiEncoderVP9::encodePicture(const PicturePtr& picture)
{
    YamiStatus ret = YAMI_FAIL;
    SurfacePtr reconstruct = createSurface();
    if (!reconstruct)
        return ret;

    if (!ensureSequence(picture))
        return ret;

    if (!ensureQMatrix(picture))
        return ret;

    if (!ensureMiscParams(picture.get()))
        return ret;

    if (!ensurePicture(picture, reconstruct))
        return ret;

    if (!picture->encode())
        return ret;

    if (!referenceListUpdate(picture, reconstruct))
        return ret;

    return YAMI_SUCCESS;
}

const bool VaapiEncoderVP9::s_registered
    = VaapiEncoderFactory::register_<VaapiEncoderVP9>(YAMI_MIME_VP9);
}
