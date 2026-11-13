#include "hcnet_thermal.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#if HCNET_THERMAL_HAVE_UVC
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#endif

struct hcnet_thermal_uvc_handle {
#if HCNET_THERMAL_HAVE_UVC
    cv::VideoCapture capture;
#endif
    std::mutex mutex;
    std::vector<unsigned char> encoded;
};

namespace {

#if HCNET_THERMAL_HAVE_UVC

bool parse_camera_index(const char *device, int *index) {
    if (!device || !*device) return false;
    char *end = NULL;
    const long value = std::strtol(device, &end, 10);
    if (!end || *end != '\0' || value < 0) return false;
    *index = static_cast<int>(value);
    return true;
}

#endif  // HCNET_THERMAL_HAVE_UVC

}  // namespace

int hcnet_thermal_uvc_available(void) {
#if HCNET_THERMAL_HAVE_UVC
    return 1;
#else
    return 0;
#endif
}

int hcnet_thermal_uvc_open(
    const char *device,
    int width,
    int height,
    int warmup_frames,
    hcnet_thermal_uvc_handle **out_handle) {
    if (out_handle) *out_handle = NULL;
    if (!device || !*device || !out_handle || width <= 0 || height <= 0 ||
        warmup_frames < 0) {
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }

#if HCNET_THERMAL_HAVE_UVC
    hcnet_thermal_uvc_handle *handle = new (std::nothrow) hcnet_thermal_uvc_handle();
    if (!handle) return HCNET_THERMAL_ERR_OUT_OF_MEMORY;

    int index = 0;
    if (parse_camera_index(device, &index)) {
        handle->capture.open(index, cv::CAP_ANY);
    } else {
        handle->capture.open(std::string(device), cv::CAP_ANY);
    }
    if (!handle->capture.isOpened()) {
        delete handle;
        return HCNET_THERMAL_ERR_UVC_OPEN;
    }

    handle->capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
    handle->capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    /* Best effort: some cameras return a few empty frames while starting. */
    cv::Mat frame;
    for (int i = 0; i < warmup_frames; ++i) {
        handle->capture.read(frame);
    }

    *out_handle = handle;
    return HCNET_THERMAL_OK;
#else
    (void)width;
    (void)height;
    (void)warmup_frames;
    return HCNET_THERMAL_ERR_UVC_UNAVAILABLE;
#endif
}

int hcnet_thermal_uvc_capture_jpeg(
    hcnet_thermal_uvc_handle *handle,
    int jpeg_quality,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *out_size) {
    if (out_size) *out_size = 0;
    if (!handle || !buffer || buffer_size == 0 || !out_size) {
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }
    if (jpeg_quality < 1 || jpeg_quality > 100) {
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }

#if HCNET_THERMAL_HAVE_UVC
    std::lock_guard<std::mutex> lock(handle->mutex);

    cv::Mat frame;
    if (!handle->capture.read(frame) || frame.empty()) {
        return HCNET_THERMAL_ERR_UVC_CAPTURE;
    }

    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(jpeg_quality);

    handle->encoded.clear();
    if (!cv::imencode(".jpg", frame, handle->encoded, params)) {
        return HCNET_THERMAL_ERR_UVC_ENCODE;
    }

    *out_size = handle->encoded.size();
    if (buffer_size < *out_size) {
        return HCNET_THERMAL_ERR_BUFFER_TOO_SMALL;
    }

    std::memcpy(buffer, handle->encoded.data(), *out_size);
    return HCNET_THERMAL_OK;
#else
    (void)jpeg_quality;
    (void)buffer;
    (void)buffer_size;
    return HCNET_THERMAL_ERR_UVC_UNAVAILABLE;
#endif
}

int hcnet_thermal_uvc_close(hcnet_thermal_uvc_handle *handle) {
    if (!handle) return HCNET_THERMAL_ERR_INVALID_ARGUMENT;

#if HCNET_THERMAL_HAVE_UVC
    {
        std::lock_guard<std::mutex> lock(handle->mutex);
        handle->capture.release();
    }
#endif

    delete handle;
    return HCNET_THERMAL_OK;
}
