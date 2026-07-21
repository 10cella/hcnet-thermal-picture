#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "HCNetSDK.h"

struct Args {
    std::string ip = "192.168.1.64";
    int port = 8000;
    std::string user = "admin";
    std::string password = "abcd1234";
    int channel = 1;
    std::string output_dir = "./output";
};

static bool parse_args(int argc, char *argv[], Args &args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) return false;
        if (key == "--ip") args.ip = argv[++i];
        else if (key == "--port") args.port = std::atoi(argv[++i]);
        else if (key == "--user") args.user = argv[++i];
        else if (key == "--password") args.password = argv[++i];
        else if (key == "--channel") args.channel = std::atoi(argv[++i]);
        else if (key == "--out") args.output_dir = argv[++i];
        else return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    Args args;
    if (!parse_args(argc, argv, args)) return 2;
    if (NET_DVR_Init() == FALSE) return 1;

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
        return 1;
    }

    NET_DVR_XML_CONFIG_INPUT input{};
    input.dwSize = sizeof(input);
    NET_DVR_XML_CONFIG_OUTPUT output{};
    output.dwSize = sizeof(output);
    char url[512] = {};
    std::snprintf(url, sizeof(url), "PUT /ISAPI/Thermal/channels/%d/thermometry/pixelToPixelParam\r\n", args.channel);
    char put_buf[2048] = {};
    std::snprintf(put_buf, sizeof(put_buf),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><PixelToPixelParam version=\"2.0\" xmlns=\"http://www.hikvision.com/ver20/XMLSchema\"><id>%d</id><maxFrameRate>400</maxFrameRate><reflectiveEnable>false</reflectiveEnable><reflectiveTemperature>20.00</reflectiveTemperature><emissivity>0.98</emissivity><distance>3000</distance><refreshInterval>50</refreshInterval><distanceUnit>centimeter</distanceUnit><temperatureDataLength>4</temperatureDataLength><JpegPictureWithAppendData><jpegPicEnabled>true</jpegPicEnabled><visiblePicEnabled>true</visiblePicEnabled><thermalPicResolution>640*480</thermalPicResolution></JpegPictureWithAppendData></PixelToPixelParam>", args.channel);
    input.lpRequestUrl = url;
    input.dwRequestUrlLen = std::strlen(url);
    input.lpInBuffer = put_buf;
    input.dwInBufferSize = std::strlen(put_buf);
    char put_out[8192] = {};
    char put_status[1024] = {};
    output.lpOutBuffer = put_out;
    output.dwOutBufferSize = sizeof(put_out);
    output.lpStatusBuffer = put_status;
    output.dwStatusSize = sizeof(put_status);
    NET_DVR_STDXMLConfig(user_id, &input, &output);

    NET_DVR_JPEGPICTURE_WITH_APPENDDATA jpeg{};
    jpeg.pJpegPicBuff = new char[2 * 1024 * 1024]();
    jpeg.pP2PDataBuff = new char[2 * 1024 * 1024]();
    jpeg.pVisiblePicBuff = new char[10 * 1024 * 1024]();
    const bool captured = NET_DVR_CaptureJPEGPicture_WithAppendData(user_id, args.channel, &jpeg);
    if (captured && jpeg.dwJpegPicLen > 0) {
        const std::string path = args.output_dir + "/thermal.jpg";
        if (FILE *file = std::fopen(path.c_str(), "wb")) {
            std::fwrite(jpeg.pJpegPicBuff, 1, jpeg.dwJpegPicLen, file);
            std::fclose(file);
        }
    }
    delete[] jpeg.pJpegPicBuff;
    delete[] jpeg.pP2PDataBuff;
    delete[] jpeg.pVisiblePicBuff;
    NET_DVR_Logout(user_id);
    NET_DVR_Cleanup();
    return captured ? 0 : 1;
}
