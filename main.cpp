#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "HCNetSDK.h"

struct Args {
    std::string ip = "192.168.1.64";
    int port = 8000;
    std::string user = "admin";
    std::string password = "abcd1234";
    std::string channel = "1";
};

extern "C" {
struct HcnetCaptureResult {
    unsigned char *jpeg_pic;
    size_t jpeg_pic_len;
    unsigned char *p2p_data;
    size_t p2p_data_len;
    unsigned char *visible_pic;
    size_t visible_pic_len;
};

struct HcnetCaptureArgs {
    const char *ip;
    int port;
    const char *user;
    const char *password;
    const char *channel;
};

void hcnet_free_buffer(unsigned char *ptr) { delete[] ptr; }

bool hcnet_capture_picture(const HcnetCaptureArgs *args, HcnetCaptureResult *result);
}

static bool capture_picture(const Args &args, HcnetCaptureResult &result) {
    if (NET_DVR_Init() == FALSE) {
        std::fprintf(stderr, "NET_DVR_Init failed\n");
        return false;
    }

    NET_DVR_USER_LOGIN_INFO login_info{};
    login_info.bUseAsynLogin = 0;
    std::snprintf(login_info.sDeviceAddress, sizeof(login_info.sDeviceAddress), "%s", args.ip.c_str());
    login_info.wPort = static_cast<WORD>(args.port);
    std::snprintf(login_info.sUserName, sizeof(login_info.sUserName), "%s", args.user.c_str());
    std::snprintf(login_info.sPassword, sizeof(login_info.sPassword), "%s", args.password.c_str());

    NET_DVR_DEVICEINFO_V40 device_info{};
    LONG user_id = NET_DVR_Login_V40(&login_info, &device_info);
    if (user_id < 0) {
        std::fprintf(stderr, "Login failed, error code: %d\n", NET_DVR_GetLastError());
        NET_DVR_Cleanup();
        return false;
    }

    const int channel = std::atoi(args.channel.c_str());

    NET_DVR_XML_CONFIG_INPUT input{};
    input.dwSize = sizeof(input);
    NET_DVR_XML_CONFIG_OUTPUT output{};
    output.dwSize = sizeof(output);

    char url[512] = { 0 };
    std::snprintf(url, sizeof(url), "GET /ISAPI/Thermal/channels/%d/thermometry/pixelToPixelParam\r\n", channel);
    input.lpRequestUrl = url;
    input.dwRequestUrlLen = std::strlen(url);

    char out_buf[8 * 1024] = { 0 };
    char status_buf[1024] = { 0 };
    output.lpOutBuffer = out_buf;
    output.dwOutBufferSize = sizeof(out_buf);
    output.lpStatusBuffer = status_buf;
    output.dwStatusSize = sizeof(status_buf);

    if (!NET_DVR_STDXMLConfig(user_id, &input, &output)) {
        std::fprintf(stderr, "GET failed, error code: %d\n", NET_DVR_GetLastError());
    }

    std::snprintf(url, sizeof(url), "PUT /ISAPI/Thermal/channels/%d/thermometry/pixelToPixelParam\r\n", channel);
    char put_buf[2 * 1024] = { 0 };
    std::snprintf(put_buf, sizeof(put_buf),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<PixelToPixelParam version=\"2.0\" xmlns=\"http://www.hikvision.com/ver20/XMLSchema\">"
        "<id>%d</id>"
        "<maxFrameRate>400</maxFrameRate>"
        "<reflectiveEnable>false</reflectiveEnable>"
        "<reflectiveTemperature>20.00</reflectiveTemperature>"
        "<emissivity>0.98</emissivity>"
        "<distance>3000</distance>"
        "<refreshInterval>50</refreshInterval>"
        "<distanceUnit>centimeter</distanceUnit>"
        "<temperatureDataLength>4</temperatureDataLength>"
        "<JpegPictureWithAppendData>"
        "<jpegPicEnabled>true</jpegPicEnabled>"
        "<visiblePicEnabled>true</visiblePicEnabled>"
        "<thermalPicResolution>640*480</thermalPicResolution>"
        "</JpegPictureWithAppendData>"
        "</PixelToPixelParam>", channel);

    input.lpRequestUrl = url;
    input.dwRequestUrlLen = std::strlen(url);
    input.lpInBuffer = put_buf;
    input.dwInBufferSize = std::strlen(put_buf);

    char put_out[8 * 1024] = { 0 };
    char put_status[1024] = { 0 };
    output.lpOutBuffer = put_out;
    output.dwOutBufferSize = sizeof(put_out);
    output.lpStatusBuffer = put_status;
    output.dwStatusSize = sizeof(put_status);

    if (!NET_DVR_STDXMLConfig(user_id, &input, &output)) {
        std::fprintf(stderr, "PUT failed, error code: %d\n", NET_DVR_GetLastError());
    }

    NET_DVR_JPEGPICTURE_WITH_APPENDDATA jpeg{};
    jpeg.pJpegPicBuff = new char[2 * 1024 * 1024]();
    jpeg.pP2PDataBuff = new char[2 * 1024 * 1024]();
    jpeg.pVisiblePicBuff = new char[10 * 1024 * 1024]();

    bool ok = NET_DVR_CaptureJPEGPicture_WithAppendData(user_id, channel, &jpeg);
    if (!ok) {
        std::fprintf(stderr, "Capture failed, error code: %d\n", NET_DVR_GetLastError());
    } else {
        if (jpeg.dwJpegPicLen > 0) {
            result.jpeg_pic = reinterpret_cast<unsigned char *>(jpeg.pJpegPicBuff);
            result.jpeg_pic_len = jpeg.dwJpegPicLen;
            jpeg.pJpegPicBuff = nullptr;
        }
        if (jpeg.dwP2PDataLen > 0) {
            result.p2p_data = reinterpret_cast<unsigned char *>(jpeg.pP2PDataBuff);
            result.p2p_data_len = jpeg.dwP2PDataLen;
            jpeg.pP2PDataBuff = nullptr;
        }
        if (jpeg.dwVisiblePicLen > 0) {
            result.visible_pic = reinterpret_cast<unsigned char *>(jpeg.pVisiblePicBuff);
            result.visible_pic_len = jpeg.dwVisiblePicLen;
            jpeg.pVisiblePicBuff = nullptr;
        }
    }

    delete[] jpeg.pJpegPicBuff;
    delete[] jpeg.pP2PDataBuff;
    delete[] jpeg.pVisiblePicBuff;

    NET_DVR_Logout(user_id);
    NET_DVR_Cleanup();
    return ok;
}

bool hcnet_capture_picture(const HcnetCaptureArgs *args, HcnetCaptureResult *result) {
    if (args == nullptr || result == nullptr) {
        return false;
    }
    Args cpp_args;
    if (args->ip != nullptr) cpp_args.ip = args->ip;
    cpp_args.port = args->port;
    if (args->user != nullptr) cpp_args.user = args->user;
    if (args->password != nullptr) cpp_args.password = args->password;
    if (args->channel != nullptr) cpp_args.channel = args->channel;

    HcnetCaptureResult cpp_result{};
    const bool ok = capture_picture(cpp_args, cpp_result);
    result->jpeg_pic = cpp_result.jpeg_pic;
    result->jpeg_pic_len = cpp_result.jpeg_pic_len;
    result->p2p_data = cpp_result.p2p_data;
    result->p2p_data_len = cpp_result.p2p_data_len;
    result->visible_pic = cpp_result.visible_pic;
    result->visible_pic_len = cpp_result.visible_pic_len;
    return ok;
}
