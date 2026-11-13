#include "hcnet_thermal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string host = "192.168.1.64";
    unsigned int port = 8000;
    std::string username = "admin";
    std::string password = "abcd1234";
    int channel = 1;
    int quality = HCNET_THERMAL_QUALITY_BEST;
    std::string output = "thermal.jpg";
};

bool parse_args(int argc, char *argv[], Args &args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            std::printf(
                "usage: %s [--ip IP] [--port PORT] [--user USER] [--password PASS]\n"
                "          [--channel N] [--quality 0|1|2] [--out FILE]\n",
                argv[0]);
            return false;
        }
        if (i + 1 >= argc) return false;
        if (key == "--ip") args.host = argv[++i];
        else if (key == "--port") args.port = static_cast<unsigned int>(std::atoi(argv[++i]));
        else if (key == "--user") args.username = argv[++i];
        else if (key == "--password") args.password = argv[++i];
        else if (key == "--channel") args.channel = std::atoi(argv[++i]);
        else if (key == "--quality") args.quality = std::atoi(argv[++i]);
        else if (key == "--out") args.output = argv[++i];
        else return false;
    }
    return args.port > 0 && args.channel >= 0 && args.quality >= 0 && args.quality <= 2;
}

std::string describe_error(int status, uint32_t sdk_error) {
    std::string message = hcnet_thermal_strerror(status);
    if (sdk_error != 0) {
        char suffix[64];
        std::snprintf(suffix, sizeof(suffix), " (SDK error %u)", sdk_error);
        message += suffix;
    }
    return message;
}

}  // namespace

int main(int argc, char *argv[]) {
    Args args;
    if (!parse_args(argc, argv, args)) return 2;

    hcnet_thermal_handle *camera = NULL;
    uint32_t sdk_error = 0;
    int status = hcnet_thermal_login(
        args.host.c_str(), static_cast<uint16_t>(args.port),
        args.username.c_str(), args.password.c_str(), args.channel,
        &camera, &sdk_error);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "login failed: %s\n", describe_error(status, sdk_error).c_str());
        return 1;
    }

    std::vector<uint8_t> jpeg(HCNET_THERMAL_MAX_JPEG_SIZE);
    size_t jpeg_size = 0;
    status = hcnet_thermal_capture_jpeg(
        camera, args.quality, jpeg.data(), jpeg.size(), &jpeg_size, &sdk_error);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "capture failed: %s\n", describe_error(status, sdk_error).c_str());
        hcnet_thermal_logout(camera, NULL);
        return 1;
    }

    FILE *file = std::fopen(args.output.c_str(), "wb");
    if (!file) {
        std::fprintf(stderr, "unable to open output file: %s\n", args.output.c_str());
        hcnet_thermal_logout(camera, NULL);
        return 1;
    }
    const bool written = std::fwrite(jpeg.data(), 1, jpeg_size, file) == jpeg_size;
    std::fclose(file);

    const int logout_status = hcnet_thermal_logout(camera, &sdk_error);
    if (logout_status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "logout failed: %s\n",
                     describe_error(logout_status, sdk_error).c_str());
    }

    if (!written) {
        std::fprintf(stderr, "unable to write output file: %s\n", args.output.c_str());
        return 1;
    }

    std::printf("saved %zu bytes to %s\n", jpeg_size, args.output.c_str());
    return 0;
}
