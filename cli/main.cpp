#include "hcnet_thermal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct ThermalOptions {
    std::string host = "192.168.1.64";
    unsigned int port = 8000;
    std::string username = "admin";
    std::string password = "abcd1234";
    int channel = 1;
    int quality = HCNET_THERMAL_QUALITY_BEST;
    std::string output = "thermal.jpg";
};

struct UvcOptions {
    std::string device = "0";
    int width = 640;
    int height = 480;
    int jpeg_quality = 95;
    int warmup_frames = 0;
    std::string output = "uvc.jpg";
};

bool parse_int(const char *text, int *value) {
    if (!text || !*text) return false;
    char *end = NULL;
    const long parsed = std::strtol(text, &end, 10);
    if (!end || *end != '\0') return false;
    *value = static_cast<int>(parsed);
    return true;
}

void print_usage(const char *program) {
    std::printf(
        "usage: %s <command> [options]\n"
        "\n"
        "commands:\n"
        "  thermal   capture a JPEG from a Hikvision thermal camera\n"
        "  uvc       capture a JPEG from a UVC camera%s\n"
        "\n"
        "Run '%s <command> --help' for command specific options.\n",
        program,
        hcnet_thermal_uvc_available() ? "" : " (unavailable: built without OpenCV)",
        program);
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

bool write_file(const std::string &path, const std::vector<uint8_t> &data,
                size_t size) {
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) {
        std::fprintf(stderr, "unable to open output file: %s\n", path.c_str());
        return false;
    }
    const bool written = std::fwrite(data.data(), 1, size, file) == size;
    std::fclose(file);
    if (!written) {
        std::fprintf(stderr, "unable to write output file: %s\n", path.c_str());
    }
    return written;
}

void print_thermal_usage(const char *program) {
    std::printf(
        "usage: %s thermal [options]\n"
        "\n"
        "options:\n"
        "  --ip IP            device address (default 192.168.1.64)\n"
        "  --port PORT        device port (default 8000)\n"
        "  --user USER        login user (default admin)\n"
        "  --password PASS    login password (default abcd1234)\n"
        "  --channel N        camera channel (default 1)\n"
        "  --quality 0|1|2    JPEG quality: 0 best, 1 better, 2 normal\n"
        "  --out FILE         output file (default thermal.jpg)\n"
        "  -h, --help         show this help\n",
        program);
}

int run_thermal(int argc, char *argv[], const char *program) {
    ThermalOptions options;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "-h" || key == "--help") {
            print_thermal_usage(program);
            return 0;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "missing value for %s\n", key.c_str());
            return 2;
        }
        const char *value = argv[++i];
        if (key == "--ip") {
            options.host = value;
        } else if (key == "--port") {
            int port = 0;
            if (!parse_int(value, &port) || port <= 0 || port > 65535) {
                std::fprintf(stderr, "invalid port: %s\n", value);
                return 2;
            }
            options.port = static_cast<unsigned int>(port);
        } else if (key == "--user") {
            options.username = value;
        } else if (key == "--password") {
            options.password = value;
        } else if (key == "--channel") {
            if (!parse_int(value, &options.channel) || options.channel < 0) {
                std::fprintf(stderr, "invalid channel: %s\n", value);
                return 2;
            }
        } else if (key == "--quality") {
            if (!parse_int(value, &options.quality) || options.quality < 0 ||
                options.quality > 2) {
                std::fprintf(stderr, "invalid quality: %s (expected 0, 1 or 2)\n",
                             value);
                return 2;
            }
        } else if (key == "--out") {
            options.output = value;
        } else {
            std::fprintf(stderr, "unknown option: %s\n", key.c_str());
            return 2;
        }
    }

    hcnet_thermal_handle *camera = NULL;
    uint32_t sdk_error = 0;
    int status = hcnet_thermal_login(
        options.host.c_str(), static_cast<uint16_t>(options.port),
        options.username.c_str(), options.password.c_str(), options.channel,
        &camera, &sdk_error);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "login failed: %s\n",
                     describe_error(status, sdk_error).c_str());
        return 1;
    }

    std::vector<uint8_t> jpeg(HCNET_THERMAL_MAX_JPEG_SIZE);
    size_t jpeg_size = 0;
    status = hcnet_thermal_capture_jpeg(camera, options.quality, jpeg.data(),
                                        jpeg.size(), &jpeg_size, &sdk_error);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "capture failed: %s\n",
                     describe_error(status, sdk_error).c_str());
        hcnet_thermal_logout(camera, NULL);
        return 1;
    }

    const bool written = write_file(options.output, jpeg, jpeg_size);

    const int logout_status = hcnet_thermal_logout(camera, &sdk_error);
    if (logout_status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "logout failed: %s\n",
                     describe_error(logout_status, sdk_error).c_str());
    }

    if (!written) return 1;

    std::printf("saved %zu bytes to %s\n", jpeg_size, options.output.c_str());
    return 0;
}

void print_uvc_usage(const char *program) {
    std::printf(
        "usage: %s uvc [options]\n"
        "\n"
        "options:\n"
        "  --device DEV       camera index or device path (default 0)\n"
        "  --width N          requested frame width (default 640)\n"
        "  --height N         requested frame height (default 480)\n"
        "  --jpeg-quality N   JPEG quality 1..100 (default 95)\n"
        "  --warmup N         frames to discard when opening (default 0)\n"
        "  --out FILE         output JPEG file (default uvc.jpg)\n"
        "  -h, --help         show this help\n",
        program);
}

int run_uvc(int argc, char *argv[], const char *program) {
    UvcOptions options;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "-h" || key == "--help") {
            print_uvc_usage(program);
            return 0;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "missing value for %s\n", key.c_str());
            return 2;
        }
        const char *value = argv[++i];
        if (key == "--device") {
            options.device = value;
        } else if (key == "--width") {
            if (!parse_int(value, &options.width) || options.width <= 0) {
                std::fprintf(stderr, "invalid width: %s\n", value);
                return 2;
            }
        } else if (key == "--height") {
            if (!parse_int(value, &options.height) || options.height <= 0) {
                std::fprintf(stderr, "invalid height: %s\n", value);
                return 2;
            }
        } else if (key == "--jpeg-quality") {
            if (!parse_int(value, &options.jpeg_quality) ||
                options.jpeg_quality < 1 || options.jpeg_quality > 100) {
                std::fprintf(stderr, "invalid jpeg quality: %s\n", value);
                return 2;
            }
        } else if (key == "--warmup") {
            if (!parse_int(value, &options.warmup_frames) ||
                options.warmup_frames < 0) {
                std::fprintf(stderr, "invalid warmup frame count: %s\n", value);
                return 2;
            }
        } else if (key == "--out") {
            options.output = value;
        } else {
            std::fprintf(stderr, "unknown option: %s\n", key.c_str());
            return 2;
        }
    }

    if (!hcnet_thermal_uvc_available()) {
        std::fprintf(stderr,
                     "UVC support is not available; rebuild the library with "
                     "OpenCV installed.\n");
        return 2;
    }

    hcnet_thermal_uvc_handle *camera = NULL;
    int status = hcnet_thermal_uvc_open(options.device.c_str(), options.width,
                                        options.height, options.warmup_frames,
                                        &camera);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "unable to open UVC camera: %s\n",
                     hcnet_thermal_strerror(status));
        return 1;
    }

    std::vector<uint8_t> jpeg(HCNET_THERMAL_MAX_JPEG_SIZE);
    size_t jpeg_size = 0;
    status = hcnet_thermal_uvc_capture_jpeg(camera, options.jpeg_quality,
                                            jpeg.data(), jpeg.size(),
                                            &jpeg_size);
    if (status != HCNET_THERMAL_OK) {
        std::fprintf(stderr, "capture failed: %s\n",
                     hcnet_thermal_strerror(status));
        hcnet_thermal_uvc_close(camera);
        return 1;
    }

    const bool written = write_file(options.output, jpeg, jpeg_size);
    hcnet_thermal_uvc_close(camera);

    if (!written) return 1;

    std::printf("saved %zu bytes to %s\n", jpeg_size, options.output.c_str());
    return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
    const char *program = argc > 0 ? argv[0] : "hcnet_thermal_cli";
    if (argc < 2) {
        print_usage(program);
        return 2;
    }

    const std::string command = argv[1];
    if (command == "-h" || command == "--help") {
        print_usage(program);
        return 0;
    }
    if (command == "thermal") return run_thermal(argc, argv, program);
    if (command == "uvc") return run_uvc(argc, argv, program);

    std::fprintf(stderr, "unknown command: %s\n\n", command.c_str());
    print_usage(program);
    return 2;
}
