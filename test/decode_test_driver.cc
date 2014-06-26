/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/register_state_check.h"
#include "test/video_source.h"

namespace libvpx_test {

const char kVP8Name[] = "WebM Project VP8";

vpx_codec_err_t Decoder::PeekStream(const uint8_t *cxdata, size_t size,
                                    vpx_codec_stream_info_t *stream_info) {
  return vpx_codec_peek_stream_info(CodecInterface(),
                                    cxdata, static_cast<unsigned int>(size),
                                    stream_info);
}

vpx_codec_err_t Decoder::DecodeFrame(const uint8_t *cxdata, size_t size) {
  return DecodeFrame(cxdata, size, NULL);
}

vpx_codec_err_t Decoder::DecodeFrame(const uint8_t *cxdata, size_t size,
                                     void *user_priv) {
  vpx_codec_err_t res_dec;
  InitOnce();
  REGISTER_STATE_CHECK(
      res_dec = vpx_codec_decode(&decoder_,
                                 cxdata, static_cast<unsigned int>(size),
                                 user_priv, 0));
  return res_dec;
}

void DecoderTest::RunLoop(CompressedVideoSource *video) {
  vpx_codec_dec_cfg_t dec_cfg = {0};
  Decoder* const decoder = codec_->CreateDecoder(dec_cfg, 0);
  ASSERT_TRUE(decoder != NULL);
  const char *codec_name = decoder->GetDecoderName();
  const bool is_vp8 = strncmp(kVP8Name, codec_name, sizeof(kVP8Name) - 1) == 0;

  // Decode frames.
  for (video->Begin(); !::testing::Test::HasFailure() && video->cxdata();
       video->Next()) {
    PreDecodeFrameHook(*video, decoder);

    vpx_codec_stream_info_t stream_info;
    stream_info.sz = sizeof(stream_info);
    const vpx_codec_err_t res_peek = decoder->PeekStream(video->cxdata(),
                                                         video->frame_size(),
                                                         &stream_info);
    if (is_vp8) {
      /* Vp8's implementation of PeekStream returns an error if the frame you
       * pass it is not a keyframe, so we only expect VPX_CODEC_OK on the first
       * frame, which must be a keyframe. */
      if (video->frame_number() == 0)
        ASSERT_EQ(VPX_CODEC_OK, res_peek) << "Peek return failed: "
            << vpx_codec_err_to_string(res_peek);
    } else {
      /* The Vp9 implementation of PeekStream returns an error only if the
       * data passed to it isn't a valid Vp9 chunk. */
      ASSERT_EQ(VPX_CODEC_OK, res_peek) << "Peek return failed: "
          << vpx_codec_err_to_string(res_peek);
    }

    vpx_codec_err_t res_dec = decoder->DecodeFrame(video->cxdata(),
                                                   video->frame_size());
    if (!HandleDecodeResult(res_dec, *video, decoder))
      break;

    DxDataIterator dec_iter = decoder->GetDxData();
    const vpx_image_t *img = NULL;

    // Get decompressed data
    while ((img = dec_iter.Next()))
      DecompressedFrameHook(*img, video->frame_number());
  }

  delete decoder;
}
}  // namespace libvpx_test
