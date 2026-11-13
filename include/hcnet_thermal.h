#ifndef HCNET_THERMAL_H
#define HCNET_THERMAL_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(HCNET_THERMAL_EXPORTS)
#define HCNET_THERMAL_API __declspec(dllexport)
#else
#define HCNET_THERMAL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Recommended caller buffer size. Increase if the device produces larger JPEGs. */
#define HCNET_THERMAL_MAX_JPEG_SIZE (8u * 1024u * 1024u)

/*
 * NET_DVR_JPEGPARA.wPicSize:
 *   0xff = use the current stream resolution
 *   6    = VGA 640x480
 *   2    = 4CIF / D1
 * Override at compile time if the device does not support 0xff.
 */
#ifndef HCNET_THERMAL_JPEG_PIC_SIZE
#define HCNET_THERMAL_JPEG_PIC_SIZE 0xff
#endif

typedef struct hcnet_thermal_handle hcnet_thermal_handle;

enum hcnet_thermal_status {
    HCNET_THERMAL_OK = 0,
    HCNET_THERMAL_ERR_INVALID_ARGUMENT = -1,
    HCNET_THERMAL_ERR_SDK_INIT = -2,
    HCNET_THERMAL_ERR_LOGIN = -3,
    HCNET_THERMAL_ERR_CAPTURE = -4,
    HCNET_THERMAL_ERR_BUFFER_TOO_SMALL = -5,
    HCNET_THERMAL_ERR_UNSUPPORTED_QUALITY = -6,
    HCNET_THERMAL_ERR_NOT_LOGGED_IN = -7,
    HCNET_THERMAL_ERR_OUT_OF_MEMORY = -8,
    HCNET_THERMAL_ERR_LOGOUT = -9
};

/* Maps to NET_DVR_JPEGPARA.wPicQuality. */
enum hcnet_thermal_jpeg_quality {
    HCNET_THERMAL_QUALITY_BEST = 0,
    HCNET_THERMAL_QUALITY_BETTER = 1,
    HCNET_THERMAL_QUALITY_NORMAL = 2
};

/*
 * Log in to a Hikvision device and create a capture handle.
 *
 * On success, *out_handle receives a handle that must be passed to
 * hcnet_thermal_capture_jpeg() and eventually hcnet_thermal_logout().
 * On failure, *out_handle is set to NULL. If sdk_error is non-NULL, it
 * receives the NET_DVR_GetLastError() value when applicable.
 */
HCNET_THERMAL_API int hcnet_thermal_login(
    const char *host,
    uint16_t port,
    const char *username,
    const char *password,
    int32_t channel,
    hcnet_thermal_handle **out_handle,
    uint32_t *sdk_error);

/*
 * Capture one JPEG frame.
 *
 * quality must be one of hcnet_thermal_jpeg_quality. The JPEG is written
 * into buffer, and *out_size receives the number of bytes written.
 *
 * The function is serialized per handle with an internal mutex. Do not call
 * hcnet_thermal_logout() concurrently with this function on the same handle.
 */
HCNET_THERMAL_API int hcnet_thermal_capture_jpeg(
    hcnet_thermal_handle *handle,
    int quality,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *out_size,
    uint32_t *sdk_error);

/*
 * Log out and release the handle.
 *
 * The handle is invalid after this call, even if the device-side logout fails.
 * The return value reports whether the device acknowledged the logout.
 */
HCNET_THERMAL_API int hcnet_thermal_logout(
    hcnet_thermal_handle *handle,
    uint32_t *sdk_error);

/* Returns a static English description for a library status code. */
HCNET_THERMAL_API const char *hcnet_thermal_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* HCNET_THERMAL_H */
