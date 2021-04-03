// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2021 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>
#include <libyuv/video_common.h>
#include <media/base/video_broadcaster.h>
#include <media/base/video_common.h>
#include <modules/desktop_capture/desktop_capture_options.h>
#include <modules/desktop_capture/desktop_capturer.h>
#include <pc/video_track_source.h>
#include <rtc_base/logging.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "open3d/core/Tensor.h"
#include "open3d/t/io/ImageIO.h"
#include "open3d/utility/Console.h"
#include "open3d/visualization/webrtc_server/BitmapTrackSource.h"
#include "open3d/visualization/webrtc_server/GlobalBuffer.h"

namespace open3d {
namespace visualization {
namespace webrtc_server {

class ImageCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
    ImageCapturer(const std::string& url_,
                  const std::map<std::string, std::string>& opts);
    virtual ~ImageCapturer();

    static ImageCapturer* Create(
            const std::string& url,
            const std::map<std::string, std::string>& opts);

    ImageCapturer(const std::map<std::string, std::string>& opts);

    virtual void AddOrUpdateSink(
            rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
            const rtc::VideoSinkWants& wants) override;

    virtual void RemoveSink(
            rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void OnCaptureResult(const std::shared_ptr<core::Tensor>& frame);

protected:
    int width_;
    int height_;
    rtc::VideoBroadcaster broadcaster_;
};

class ImageTrackSource : public BitmapTrackSource {
public:
    /// \param video_url Typically window_uid
    static rtc::scoped_refptr<BitmapTrackSourceInterface> Create(
            const std::string& video_url,
            const std::map<std::string, std::string>& opts) {
        // TODO: do we need to call SanitizeLabel for video_url?
        std::unique_ptr<ImageCapturer> capturer =
                absl::WrapUnique(ImageCapturer::Create(video_url, opts));
        if (!capturer) {
            return nullptr;
        }
        rtc::scoped_refptr<BitmapTrackSourceInterface> video_source =
                new rtc::RefCountedObject<ImageTrackSource>(
                        std::move(capturer));
        return video_source;
    }

    void OnFrame(const std::shared_ptr<core::Tensor>& frame) final override {
        capturer_->OnCaptureResult(frame);
    }

protected:
    explicit ImageTrackSource(std::unique_ptr<ImageCapturer> capturer)
        : BitmapTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

private:
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
        return capturer_.get();
    }
    std::unique_ptr<ImageCapturer> capturer_;
};

}  // namespace webrtc_server
}  // namespace visualization
}  // namespace open3d
