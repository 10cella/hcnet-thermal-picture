#include "hcnet_thermal.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

#include "HCNetSDK.h"

namespace {

std::mutex g_runtime_mutex;
int g_runtime_users = 0;
bool g_runtime_initialized = false;

bool acquire_runtime(uint32_t *sdk_error) {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (!g_runtime_initialized) {
        if (!NET_DVR_Init()) {
            if (sdk_error) *sdk_error = NET_DVR_GetLastError();
            return false;
        }
        g_runtime_initialized = true;
    }
    ++g_runtime_users;
    return true;
}

void release_runtime() {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (g_runtime_users > 0) --g_runtime_users;
    if (g_runtime_users == 0 && g_runtime_initialized) {
        NET_DVR_Cleanup();
        g_runtime_initialized = false;
    }
}

void set_sdk_error(uint32_t *sdk_error, uint32_t value) {
    if (sdk_error) *sdk_error = value;
}

bool copy_login_field(char *destination, size_t capacity, const char *source) {
    if (!source || std::strlen(source) >= capacity) return false;
    std::snprintf(destination, capacity, "%s", source);
    return true;
}

}  // namespace

struct hcnet_thermal_handle {
    LONG user_id = -1;
    int32_t channel = 0;
    bool logged_in = false;
    std::mutex mutex;
    std::vector<uint8_t> jpeg_buffer;
};

int hcnet_thermal_login(
    const char *host,
    uint16_t port,
    const char *username,
    const char *password,
    int32_t channel,
    hcnet_thermal_handle **out_handle,
    uint32_t *sdk_error) {
    set_sdk_error(sdk_error, 0);
    if (out_handle) *out_handle = NULL;
    if (!host || !username || !password || !out_handle || port == 0 || channel < 0) {
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }

    if (!acquire_runtime(sdk_error)) return HCNET_THERMAL_ERR_SDK_INIT;

    hcnet_thermal_handle *handle = NULL;
    try {
        handle = new hcnet_thermal_handle();
        handle->channel = channel;
        handle->jpeg_buffer.resize(HCNET_THERMAL_MAX_JPEG_SIZE);
    } catch (const std::bad_alloc &) {
        delete handle;
        release_runtime();
        return HCNET_THERMAL_ERR_OUT_OF_MEMORY;
    }

    NET_DVR_USER_LOGIN_INFO login_info{};
    login_info.bUseAsynLogin = 0;
    login_info.wPort = port;

    if (!copy_login_field(login_info.sDeviceAddress, sizeof(login_info.sDeviceAddress), host) ||
        !copy_login_field(login_info.sUserName, sizeof(login_info.sUserName), username) ||
        !copy_login_field(login_info.sPassword, sizeof(login_info.sPassword), password)) {
        delete handle;
        release_runtime();
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }

    NET_DVR_DEVICEINFO_V40 device_info{};
    const LONG user_id = NET_DVR_Login_V40(&login_info, &device_info);
    if (user_id < 0) {
        set_sdk_error(sdk_error, NET_DVR_GetLastError());
        delete handle;
        release_runtime();
        return HCNET_THERMAL_ERR_LOGIN;
    }

    handle->user_id = user_id;
    handle->logged_in = true;
    *out_handle = handle;
    return HCNET_THERMAL_OK;
}

int hcnet_thermal_capture_jpeg(
    hcnet_thermal_handle *handle,
    int quality,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *out_size,
    uint32_t *sdk_error) {
    set_sdk_error(sdk_error, 0);
    if (out_size) *out_size = 0;
    if (!handle || !buffer || buffer_size == 0 || !out_size) {
        return HCNET_THERMAL_ERR_INVALID_ARGUMENT;
    }
    if (quality < HCNET_THERMAL_QUALITY_BEST || quality > HCNET_THERMAL_QUALITY_NORMAL) {
        return HCNET_THERMAL_ERR_UNSUPPORTED_QUALITY;
    }

    std::lock_guard<std::mutex> lock(handle->mutex);
    if (!handle->logged_in) return HCNET_THERMAL_ERR_NOT_LOGGED_IN;

    NET_DVR_JPEGPARA jpeg_param{};
    jpeg_param.wPicSize = HCNET_THERMAL_JPEG_PIC_SIZE;
    jpeg_param.wPicQuality = static_cast<WORD>(quality);

    DWORD returned_size = 0;
    const BOOL captured = NET_DVR_CaptureJPEGPicture_NEW(
        handle->user_id,
        handle->channel,
        &jpeg_param,
        reinterpret_cast<char *>(handle->jpeg_buffer.data()),
        static_cast<DWORD>(handle->jpeg_buffer.size()),
        &returned_size);

    if (!captured) {
        set_sdk_error(sdk_error, NET_DVR_GetLastError());
        return HCNET_THERMAL_ERR_CAPTURE;
    }
    if (returned_size == 0 || returned_size > handle->jpeg_buffer.size()) {
        return HCNET_THERMAL_ERR_CAPTURE;
    }

    *out_size = static_cast<size_t>(returned_size);
    if (buffer_size < *out_size) return HCNET_THERMAL_ERR_BUFFER_TOO_SMALL;

    std::memcpy(buffer, handle->jpeg_buffer.data(), *out_size);
    return HCNET_THERMAL_OK;
}

int hcnet_thermal_logout(hcnet_thermal_handle *handle, uint32_t *sdk_error) {
    set_sdk_error(sdk_error, 0);
    if (!handle) return HCNET_THERMAL_ERR_INVALID_ARGUMENT;

    int result = HCNET_THERMAL_OK;
    {
        std::lock_guard<std::mutex> lock(handle->mutex);
        if (handle->logged_in) {
            if (!NET_DVR_Logout(handle->user_id)) {
                set_sdk_error(sdk_error, NET_DVR_GetLastError());
                result = HCNET_THERMAL_ERR_LOGOUT;
            }
            handle->logged_in = false;
            handle->user_id = -1;
        }
    }

    delete handle;
    release_runtime();
    return result;
}

const char *hcnet_thermal_strerror(int status) {
    switch (status) {
        case HCNET_THERMAL_OK: return "ok";
        case HCNET_THERMAL_ERR_INVALID_ARGUMENT: return "invalid argument";
        case HCNET_THERMAL_ERR_SDK_INIT: return "SDK initialization failed";
        case HCNET_THERMAL_ERR_LOGIN: return "device login failed";
        case HCNET_THERMAL_ERR_CAPTURE: return "JPEG capture failed";
        case HCNET_THERMAL_ERR_BUFFER_TOO_SMALL: return "output buffer is too small";
        case HCNET_THERMAL_ERR_UNSUPPORTED_QUALITY: return "unsupported JPEG quality";
        case HCNET_THERMAL_ERR_NOT_LOGGED_IN: return "handle is not logged in";
        case HCNET_THERMAL_ERR_OUT_OF_MEMORY: return "out of memory";
        case HCNET_THERMAL_ERR_LOGOUT: return "device logout failed";
        default: return "unknown error";
    }
}
