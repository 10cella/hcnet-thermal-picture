# hcnet-thermal-picture

海康威视红外热成像与普通 UVC 摄像头的抓图库，以及一个命令行程序。

- 核心以稳定的 C ABI 提供两类摄像头的操作，构建为跨平台静态库：
  - Linux：`libhcnet_thermal.a`
  - Windows：`hcnet_thermal.lib`
- 两类摄像头共用同一套库：
  - **海康红外**：登录 → 抓 JPEG → 注销
  - **UVC 普通摄像头**（可选，依赖 OpenCV）：打开 → 抓 JPEG → 关闭
- Rust 通过静态链接直接调用 C ABI，可同时操作两种摄像头，示例见 `examples/rust/`。
- `hcnet_thermal_cli` 是库的薄封装，提供 `thermal` 和 `uvc` 两个子命令。
- 不再包含 Python 包、HTTP 服务、温度矩阵解析或设备配置接口。

对外 C ABI：

```c
/* 海康红外 */
int hcnet_thermal_login(...);
int hcnet_thermal_capture_jpeg(...);
int hcnet_thermal_logout(...);

/* UVC */
int hcnet_thermal_uvc_available(void);
int hcnet_thermal_uvc_open(...);
int hcnet_thermal_uvc_capture_jpeg(...);
int hcnet_thermal_uvc_close(...);

/* 通用 */
const char *hcnet_thermal_strerror(int status);
```

红外抓图使用 `NET_DVR_CaptureJPEGPicture_NEW`，UVC 抓图使用 OpenCV，两者都直接返回 JPEG 字节。

---

## 1. 准备海康 SDK

从海康开放平台下载对应平台的设备网络 SDK：

https://open.hikvision.com/download/5cda567cf47ae80dd41a54b3?type=10&id=5cda5902f47ae80dd41a54b7

把 SDK 的头文件放到 `include/`，库文件放到 `lib/`：

```text
include/HCNetSDK.h          # 以及 SDK 依赖的其他头文件
lib/libhcnetsdk.so          # Linux
lib/HCNetSDKCom/*.so        # Linux 组件库（运行时需要）
lib/HCNetSDK.lib            # Windows 导入库
lib/HCNetSDK.dll            # Windows 运行时
```

`.gitignore` 已忽略 SDK 头文件和二进制库，只跟踪本项目自己的 `include/hcnet_thermal.h`。

运行时请确保动态库可被找到：Linux 下可将 `lib/` 加入 `LD_LIBRARY_PATH`，Windows 下把 `HCNetSDK.dll` 及 `HCNetSDKCom/` 放在可执行文件同目录。

---

## 2. 编译

### CMake（推荐，Linux / Windows / macOS 通用）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

如果 SDK 不在默认的 `include/`、`lib/`：

```bash
cmake -S . -B build \
  -DHCNET_SDK_INCLUDE_DIR=/path/to/sdk/include \
  -DHCNET_SDK_LIB_DIR=/path/to/sdk/lib \
  -DHCNET_SDK_LIB_NAME=hcnetsdk
```

常用选项：

| 选项 | 默认值 | 说明 |
|---|---|---|
| `HCNET_SDK_INCLUDE_DIR` | `include/` | `HCNetSDK.h` 所在目录 |
| `HCNET_SDK_LIB_DIR` | `lib/` | SDK 库目录 |
| `HCNET_SDK_LIB_NAME` | Linux `hcnetsdk` / Windows `HCNetSDK` | SDK 库名 |
| `HCNET_THERMAL_JPEG_PIC_SIZE` | `0xff` | `NET_DVR_JPEGPARA.wPicSize` |
| `HCNET_THERMAL_ENABLE_UVC` | `ON` | 库是否包含 UVC 支持（需要 OpenCV） |
| `HCNET_THERMAL_BUILD_CLI` | `ON` | 是否构建 `hcnet_thermal_cli` |

产物：

```text
build/libhcnet_thermal.a       # 静态库（Rust 等原生调用方使用）
build/hcnet_thermal_cli        # 命令行程序
```

Windows 下对应 `hcnet_thermal.lib` 和 `hcnet_thermal_cli.exe`。

如果 CMake 找不到 OpenCV，库仍会构建，但 UVC 函数会返回
`HCNET_THERMAL_ERR_UVC_UNAVAILABLE`，`hcnet_thermal_uvc_available()` 返回 `0`。

### Make（仅 Linux）

```bash
make            # 静态库 + CLI（自动检测 OpenCV）
make UVC=0      # 强制关闭 UVC 支持
make UVC=1      # 强制要求 OpenCV 4
```

可通过变量覆盖 SDK 路径：

```bash
make SDK_INCLUDE_DIR=/path/to/sdk/include \
     SDK_LIB_DIR=/path/to/sdk/lib \
     SDK_LIB_NAME=hcnetsdk
```

### 安装 OpenCV（可选，UVC 需要）

三个平台都很方便：

```bash
# Debian / Ubuntu
sudo apt install libopencv-dev

# Fedora / RHEL
sudo dnf install opencv-devel

# macOS
brew install opencv
```

Windows 推荐使用 vcpkg：

```powershell
vcpkg install opencv4
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
```

也可以使用 OpenCV 官方 Windows 安装包，并在 CMake 中通过 `-DOpenCV_DIR=...` 指定 `OpenCVConfig.cmake` 所在目录。

macOS 上如果 CMake 找不到 Homebrew 安装的 OpenCV，可显式指定：

```bash
cmake -S . -B build \
  -DOpenCV_DIR="$(brew --prefix opencv)/lib/cmake/opencv4"
```

---

## 3. CLI 用法

### 抓取红外设备 JPEG

```bash
build/hcnet_thermal_cli thermal \
  --ip 192.168.1.64 \
  --port 8000 \
  --user admin \
  --password abcd1234 \
  --channel 1 \
  --quality 0 \
  --out thermal.jpg
```

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--ip` | `192.168.1.64` | 设备地址 |
| `--port` | `8000` | 设备端口 |
| `--user` | `admin` | 登录用户 |
| `--password` | `abcd1234` | 登录密码 |
| `--channel` | `1` | 通道号 |
| `--quality` | `0` | `0` 最好、`1` 较好、`2` 一般 |
| `--out` | `thermal.jpg` | 输出文件 |

### 抓取 UVC 摄像头 JPEG

```bash
build/hcnet_thermal_cli uvc \
  --device 0 \
  --width 640 \
  --height 480 \
  --jpeg-quality 95 \
  --warmup 3 \
  --out uvc.jpg
```

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--device` | `0` | 摄像头索引（如 `0`）或设备路径（如 `/dev/video0`） |
| `--width` | `640` | 请求宽度，驱动可能选择其他分辨率 |
| `--height` | `480` | 请求高度 |
| `--jpeg-quality` | `95` | JPEG 质量，`1..100` |
| `--warmup` | `0` | 打开时丢弃的帧数，用于等待曝光稳定 |
| `--out` | `uvc.jpg` | 输出 JPEG 文件 |

如果库在构建时没有找到 OpenCV，`uvc` 子命令会提示不可用并返回 `2`。

---

## 4. C API

完整定义见 `include/hcnet_thermal.h`。

### 4.1 海康红外

```c
#include "hcnet_thermal.h"

hcnet_thermal_handle *camera = NULL;
uint32_t sdk_error = 0;

int status = hcnet_thermal_login(
    "192.168.1.64", 8000, "admin", "abcd1234", 1,
    &camera, &sdk_error);
if (status != HCNET_THERMAL_OK) {
    /* sdk_error 为 NET_DVR_GetLastError() 的值 */
    return 1;
}

uint8_t *jpeg = malloc(HCNET_THERMAL_MAX_JPEG_SIZE);
size_t jpeg_size = 0;

status = hcnet_thermal_capture_jpeg(
    camera,
    HCNET_THERMAL_QUALITY_BEST,   /* 0 最好，1 较好，2 一般 */
    jpeg, HCNET_THERMAL_MAX_JPEG_SIZE,
    &jpeg_size, &sdk_error);
if (status != HCNET_THERMAL_OK) {
    /* 处理错误 */
}

/* jpeg[0 .. jpeg_size) 就是 JPEG 字节 */
hcnet_thermal_logout(camera, &sdk_error);
free(jpeg);
```

`quality` 直接映射到 `NET_DVR_JPEGPARA.wPicQuality`：

| 值 | 含义 |
|---|---|
| `0` (`HCNET_THERMAL_QUALITY_BEST`) | 最好 |
| `1` (`HCNET_THERMAL_QUALITY_BETTER`) | 较好 |
| `2` (`HCNET_THERMAL_QUALITY_NORMAL`) | 一般 |

分辨率由 `wPicSize` 控制，默认 `0xff`，表示使用当前码流分辨率。如果设备不支持，可在编译时覆盖：

```bash
cmake -S . -B build -DHCNET_THERMAL_JPEG_PIC_SIZE=6   # 640x480
```

常用值：`0` CIF、`2` 4CIF/D1、`6` VGA 640x480、`0xff` 自动。

### 4.2 UVC 摄像头

```c
#include "hcnet_thermal.h"

if (!hcnet_thermal_uvc_available()) {
    /* 库在构建时没有找到 OpenCV */
    return 1;
}

hcnet_thermal_uvc_handle *camera = NULL;
int status = hcnet_thermal_uvc_open("0", 640, 480, 3, &camera);
if (status != HCNET_THERMAL_OK) {
    return 1;
}

uint8_t *jpeg = malloc(HCNET_THERMAL_MAX_JPEG_SIZE);
size_t jpeg_size = 0;

status = hcnet_thermal_uvc_capture_jpeg(
    camera, 95, jpeg, HCNET_THERMAL_MAX_JPEG_SIZE, &jpeg_size);
if (status != HCNET_THERMAL_OK) {
    /* 处理错误 */
}

hcnet_thermal_uvc_close(camera);
free(jpeg);
```

- `device` 可以是十进制摄像头索引（`"0"`），也可以是设备路径（`/dev/video0`）。
- `width` / `height` 是请求值，驱动可能返回其他分辨率。
- `warmup_frames` 是打开时丢弃的帧数，用于等待自动曝光稳定。
- `jpeg_quality` 范围 `1..100`，`100` 最好。

### 4.3 输出缓冲区

两类抓图函数都先在库内部生成完整 JPEG，再复制到调用方缓冲区。调用方至少分配 `HCNET_THERMAL_MAX_JPEG_SIZE` 字节。如果实际 JPEG 超过缓冲区，函数返回 `HCNET_THERMAL_ERR_BUFFER_TOO_SMALL`，并通过 `out_size` 给出所需大小，调用方可以扩容后重试。

### 4.4 线程安全

- 每个 handle（红外和 UVC）内部各持有一把 `std::mutex`，同一 handle 上的 `capture` 调用会串行执行。
- 不同 handle 之间互不阻塞。
- `NET_DVR_Init` / `NET_DVR_Cleanup` 使用进程级引用计数。
- 不要在 `logout` / `uvc_close` 执行期间对同一 handle 调用 `capture`。正确做法是先停止新请求，等待进行中的抓图结束，再释放。
- 进程内锁不能跨进程生效。单台设备建议单进程、单登录会话。

### 4.5 状态码

| 状态码 | 常量 | 说明 |
|---|---|---|
| `0` | `HCNET_THERMAL_OK` | 成功 |
| `-1` | `HCNET_THERMAL_ERR_INVALID_ARGUMENT` | 参数错误 |
| `-2` | `HCNET_THERMAL_ERR_SDK_INIT` | SDK 初始化失败 |
| `-3` | `HCNET_THERMAL_ERR_LOGIN` | 登录失败 |
| `-4` | `HCNET_THERMAL_ERR_CAPTURE` | 红外抓图失败 |
| `-5` | `HCNET_THERMAL_ERR_BUFFER_TOO_SMALL` | 输出缓冲区太小 |
| `-6` | `HCNET_THERMAL_ERR_UNSUPPORTED_QUALITY` | 红外质量参数不是 0/1/2 |
| `-7` | `HCNET_THERMAL_ERR_NOT_LOGGED_IN` | handle 已注销 |
| `-8` | `HCNET_THERMAL_ERR_OUT_OF_MEMORY` | 内存不足 |
| `-9` | `HCNET_THERMAL_ERR_LOGOUT` | 设备侧注销失败 |
| `-10` | `HCNET_THERMAL_ERR_UVC_UNAVAILABLE` | 库未启用 UVC（构建时缺少 OpenCV） |
| `-11` | `HCNET_THERMAL_ERR_UVC_OPEN` | 无法打开 UVC 摄像头 |
| `-12` | `HCNET_THERMAL_ERR_UVC_CAPTURE` | UVC 读取帧失败 |
| `-13` | `HCNET_THERMAL_ERR_UVC_ENCODE` | UVC JPEG 编码失败 |

红外接口的 `sdk_error` 非 0 时，其值为 `NET_DVR_GetLastError()` 的返回。常见错误：

- `7/8/9/10`：网络连接、发送、接收、超时
- `47`：用户不存在（会话可能已失效）
- `73`：socket 关闭
- `23/29`：设备不支持该 JPEG 抓图能力或参数

`23/29` 属于能力或参数问题，重试没有意义；网络类错误可以注销后重新登录再试。

---

## 5. Rust 集成

示例位于 `examples/rust/`，是一个同时演示两种摄像头的 crate：

```text
examples/rust/src/lib.rs          # 安全的 Rust 封装
examples/rust/src/bin/thermal.rs  # 红外示例
examples/rust/src/bin/uvc.rs      # UVC 示例
```

```bash
cd examples/rust
cargo run --bin thermal   # 写 thermal.jpg
cargo run --bin uvc       # 写 uvc.jpg
```

默认从 `../../build` 查找 `libhcnet_thermal.a`，从 `../../lib` 查找海康 SDK。可通过环境变量覆盖：

| 变量 | 说明 |
|---|---|
| `HCNET_THERMAL_LIB_DIR` | `libhcnet_thermal.a` 所在目录 |
| `HCNET_SDK_LIB_DIR` | 海康 SDK 库目录 |
| `HCNET_SDK_LIB_NAME` | 海康 SDK 库名 |
| `HCNET_OPENCV_LIBS` | 显式指定 OpenCV 链接参数，如 `-L/path -lopencv_core4` |
| `HCNET_THERMAL_WITH_UVC` | 设为 `0` 时不链接 OpenCV（库未启用 UVC 时使用） |

`build.rs` 会：

- 添加本项目静态库和 SDK 库目录
- 静态链接 `hcnet_thermal`
- 动态链接海康 SDK（`HCNetSDK` 本体通常只提供动态库）
- 当库启用 UVC 时，通过 `pkg-config --libs opencv4` 或 `HCNET_OPENCV_LIBS` 链接 OpenCV
- Linux 链接 `stdc++`，macOS 链接 `c++`，Windows 链接 `ws2_32`、`winmm`、`iphlpapi`、`shlwapi`

如果构建静态库时没有 OpenCV，请给 Rust 加上 `HCNET_THERMAL_WITH_UVC=0`，否则 `build.rs` 会尝试链接 OpenCV。

---

## 6. 目录结构

```text
include/hcnet_thermal.h          # 公共 C ABI（红外 + UVC）
src/hcnet_thermal.cpp            # 红外实现
src/uvc_capture.cpp              # UVC 实现（OpenCV，可选）
cli/main.cpp                     # CLI 入口与参数解析
examples/rust/                   # Rust 静态链接示例（两种摄像头）
CMakeLists.txt                   # 跨平台构建
Makefile                         # Linux 构建
```

---

## 7. 注意事项

- 本库只返回 JPEG，不解析温度矩阵。需要温度数据请使用海康 SDK 的其他接口。
- 设备是否支持 JPEG 抓图、支持哪些分辨率，取决于设备能力集。若返回 `23` 或 `29`，请调整 `HCNET_THERMAL_JPEG_PIC_SIZE` 或确认设备能力。
- 登录会话可能因网络中断、设备重启或超时失效。长期运行的服务应在外层判断网络类错误并重新登录。
- UVC 支持依赖 OpenCV。没有 OpenCV 时，库、CLI 的红外功能和 Rust 集成仍然可用，只有 UVC 函数返回 `-10`。
- Windows 构建需要在真实的 MSVC 或 MinGW 环境中链接对应的 `HCNetSDK.lib`，本仓库不包含 SDK 二进制文件。使用 OpenCV 时还需要把对应的 OpenCV DLL 放在可执行文件或 DLL 同目录。
