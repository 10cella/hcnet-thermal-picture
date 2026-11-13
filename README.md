# hcnet-thermal-picture

海康威视红外热成像摄像头抓图库。

本项目把设备登录、JPEG 抓图和注销封装成稳定的 C ABI，可构建为：

- 静态库：Linux `libhcnet_thermal.a` / Windows `hcnet_thermal.lib`
- 共享库：Linux `libhcnet_thermal.so` / Windows `hcnet_thermal.dll` + 导入库

Rust 建议静态链接；Python 使用 `ctypes`，必须加载共享库（`.a` / `.lib` 不能被 `ctypes` 直接加载）。

对外只暴露四个函数：

```c
int hcnet_thermal_login(...);          /* 登录，返回 handle */
int hcnet_thermal_capture_jpeg(...);   /* 抓取一帧 JPEG，质量由调用方传入 */
int hcnet_thermal_logout(...);         /* 注销并释放 handle */
const char *hcnet_thermal_strerror(int status);
```

抓图使用 `NET_DVR_CaptureJPEGPicture_NEW`，直接返回 JPEG 字节，不再生成温度数据、CSV 或可见光图片，也不包含 HTTP 服务或设备配置接口。

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

运行时请确保动态库可被找到：Linux 下可将 `lib/` 加入 `LD_LIBRARY_PATH`，Windows 下把 `HCNetSDK.dll` 及 `HCNetSDKCom/` 放在可执行文件或 DLL 同目录。

---

## 2. 编译

### CMake（推荐，Linux / Windows 通用）

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
| `HCNET_THERMAL_BUILD_SHARED` | `ON` | 是否构建共享库（Python 需要） |
| `HCNET_THERMAL_BUILD_EXAMPLE` | `ON` | 是否构建 C++ 示例 |

产物：

```text
build/libhcnet_thermal.a       # 静态库
build/libhcnet_thermal.so      # 共享库（Linux）
build/hcnet_capture_example    # C++ 示例
```

Windows 下共享库同时生成 `hcnet_thermal.dll` 和导入库；静态库与共享库的导入库使用不同文件名，避免冲突。

### Make（仅 Linux）

```bash
make            # 构建静态库和共享库
make example    # 构建示例
make wheel      # 打包 Python wheel（会复制共享库和 SDK 动态库）
```

可通过变量覆盖：

```bash
make SDK_LIB_DIR=/path/to/sdk/lib SDK_LIB_NAME=hcnetsdk
```

---

## 3. C API

完整定义见 `include/hcnet_thermal.h`。

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

### JPEG 质量

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

### 输出缓冲区

库内部使用 8 MB 缓冲抓图，再把结果复制到调用方缓冲区。调用方至少分配 `HCNET_THERMAL_MAX_JPEG_SIZE` 字节。如果实际 JPEG 超过缓冲区，函数返回 `HCNET_THERMAL_ERR_BUFFER_TOO_SMALL`，并通过 `out_size` 给出所需大小。

### 线程安全

- 每个 handle 内部持有一把 `std::mutex`，同一 handle 上的 `capture` 调用会串行执行。
- 不同 handle 之间互不阻塞。
- `NET_DVR_Init` / `NET_DVR_Cleanup` 使用进程级引用计数，多个 handle 不会重复初始化或提前清理。
- 不要在 `hcnet_thermal_logout()` 执行期间对同一 handle 调用 `capture`。正确做法是先停止新请求，等待进行中的抓图结束，再注销。
- 进程内锁不能跨进程生效。单台设备建议单进程、单登录会话。

### 状态码

| 状态码 | 常量 | 说明 |
|---|---|---|
| `0` | `HCNET_THERMAL_OK` | 成功 |
| `-1` | `HCNET_THERMAL_ERR_INVALID_ARGUMENT` | 参数错误 |
| `-2` | `HCNET_THERMAL_ERR_SDK_INIT` | SDK 初始化失败 |
| `-3` | `HCNET_THERMAL_ERR_LOGIN` | 登录失败 |
| `-4` | `HCNET_THERMAL_ERR_CAPTURE` | 抓图失败 |
| `-5` | `HCNET_THERMAL_ERR_BUFFER_TOO_SMALL` | 输出缓冲区太小 |
| `-6` | `HCNET_THERMAL_ERR_UNSUPPORTED_QUALITY` | 质量参数不是 0/1/2 |
| `-7` | `HCNET_THERMAL_ERR_NOT_LOGGED_IN` | handle 已注销 |
| `-8` | `HCNET_THERMAL_ERR_OUT_OF_MEMORY` | 内存不足 |
| `-9` | `HCNET_THERMAL_ERR_LOGOUT` | 设备侧注销失败 |

当 `sdk_error` 非 0 时，其值为 `NET_DVR_GetLastError()` 的返回。常见错误：

- `7/8/9/10`：网络连接、发送、接收、超时
- `47`：用户不存在（会话可能已失效）
- `73`：socket 关闭
- `23/29`：设备不支持该 JPEG 抓图能力或参数

`23/29` 属于能力或参数问题，重试没有意义；网络类错误可以注销后重新登录再试。

---

## 4. Python 集成

Python 通过 `ctypes` 加载共享库，因此必须先构建共享库。

```bash
pip install -e .
```

或直接使用源码目录。默认会从项目根目录、`build/`、`build/Release/`、`build/Debug/` 以及包目录查找共享库。也可以用环境变量指定：

```bash
export HCNET_THERMAL_LIBRARY=/path/to/libhcnet_thermal.so
```

### 一次性抓图

```python
from hcnet_thermal_picture import capture_picture, QUALITY_BEST

jpeg = capture_picture(
    ip="192.168.1.64",
    port=8000,
    user="admin",
    password="abcd1234",
    channel=1,
    quality=QUALITY_BEST,
)
open("thermal.jpg", "wb").write(jpeg)
```

### 复用登录会话

```python
from hcnet_thermal_picture import ThermalCamera, QUALITY_BETTER

with ThermalCamera(
    ip="192.168.1.64",
    port=8000,
    user="admin",
    password="abcd1234",
    channel=1,
) as camera:
    jpeg = camera.capture(QUALITY_BETTER)
    open("thermal.jpg", "wb").write(jpeg)
```

`ThermalCamera` 内部复用同一个 handle，适合连续抓图；`capture_picture()` 每次都会登录、抓图、注销。

错误统一抛出 `ThermalCameraError`，包含 `status` 和 `sdk_error` 属性。

### UVC 普通摄像头（可选）

UVC 功能依赖 OpenCV，未安装时不影响热成像模块导入：

```bash
pip install -e '.[uvc]'
```

```python
from hcnet_thermal_picture import capture_uvc_picture

png = capture_uvc_picture(device=0, width=640, height=480)
```

---

## 5. Rust 集成

示例位于 `examples/rust/`，演示静态链接和 FFI 调用：

```bash
cd examples/rust
cargo run
```

默认从 `../../build` 查找 `libhcnet_thermal.a`，从 `../../lib` 查找海康 SDK。可通过环境变量覆盖：

```bash
HCNET_THERMAL_LIB_DIR=/path/to/build \
HCNET_SDK_LIB_DIR=/path/to/sdk/lib \
HCNET_SDK_LIB_NAME=hcnetsdk \
cargo run
```

`build.rs` 会：

- `cargo:rustc-link-search` 添加本项目静态库和 SDK 库目录
- `cargo:rustc-link-lib=static=hcnet_thermal` 静态链接本项目
- 动态链接海康 SDK（`HCNetSDK` 本体通常只提供动态库）
- Linux 额外链接 `stdc++`，Windows 额外链接 `ws2_32`、`winmm`、`iphlpapi`、`shlwapi`

FFI 声明直接对应 `include/hcnet_thermal.h`，示例代码见 `examples/rust/src/main.rs`。

---

## 6. 目录结构

```text
include/hcnet_thermal.h          # 公共 C ABI
src/hcnet_thermal.cpp            # 库实现
examples/capture_cli.cpp         # C++ 示例
examples/rust/                   # Rust 静态链接示例
hcnet_thermal_picture/           # Python 包（ctypes）
CMakeLists.txt                   # 跨平台构建
Makefile                         # Linux 构建
```

---

## 7. 注意事项

- 本库只返回 JPEG，不解析温度矩阵。需要温度数据请使用海康 SDK 的其他接口。
- 设备是否支持 JPEG 抓图、支持哪些分辨率，取决于设备能力集。若返回 `23` 或 `29`，请调整 `HCNET_THERMAL_JPEG_PIC_SIZE` 或确认设备能力。
- 登录会话可能因网络中断、设备重启或超时失效。长期运行的服务应在外层判断网络类错误并重新登录。
- Windows 构建需要在真实的 MSVC 或 MinGW 环境中链接对应的 `HCNetSDK.lib`，本仓库不包含 SDK 二进制文件。
