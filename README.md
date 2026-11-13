# hcnet-thermal-picture

海康威视红外热成像与普通 UVC 摄像头的抓图库，附带一个命令行程序。

## 功能

- **海康红外**：登录设备、抓取 JPEG（质量由调用方传入）、注销。
- **UVC 摄像头**（可选，依赖 OpenCV）：打开设备、抓取 JPEG（质量 1..100）、关闭。
- 构建为跨平台静态库：Linux `libhcnet_thermal.a`、Windows `hcnet_thermal.lib`。
- 提供稳定的 C ABI，Rust 可静态链接并同时操作两类摄像头。
- 每个 handle 内部带互斥锁，同一 handle 上的抓图调用串行执行。
- 抓图直接返回 JPEG 字节，不生成温度矩阵、CSV 或可见光图片，也不包含 HTTP 服务或设备配置接口。
- `hcnet_thermal_cli` 是库的薄封装，提供 `thermal` 和 `uvc` 两个子命令。

## 1. 准备海康 SDK

SDK 下载地址：

https://open.hikvision.com/download/5cda567cf47ae80dd41a54b3

下载对应平台的设备网络 SDK，将头文件放到 `include/`，库文件放到 `lib/`：

- Linux：`libhcnetsdk.so` 及 `HCNetSDKCom/` 组件库
- Windows：`HCNetSDK.lib`、`HCNetSDK.dll`

运行时确保动态库可被找到：Linux 设置 `LD_LIBRARY_PATH`，Windows 将 DLL 放在可执行文件同目录。

UVC 功能需要 OpenCV 4。Linux 用发行版包管理器安装，macOS 用 Homebrew，Windows 用 vcpkg 或 OpenCV 官方安装包。未安装 OpenCV 时库仍可构建，只是 UVC 不可用。

## 2. 编译

项目只使用 CMake：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

SDK 不在默认的 `include/`、`lib/` 时，通过 `HCNET_SDK_INCLUDE_DIR`、`HCNET_SDK_LIB_DIR`、`HCNET_SDK_LIB_NAME` 指定。UVC 支持可用 `-DHCNET_THERMAL_ENABLE_UVC=OFF` 关闭。

构建产物：

- `build/libhcnet_thermal.a`（Windows 为 `hcnet_thermal.lib`）
- `build/hcnet_thermal_cli`

## 3. 使用

命令行程序提供两个子命令：

- `thermal`：连接海康红外设备并抓取一张 JPEG，可指定地址、端口、账号、密码、通道、质量和输出文件。
- `uvc`：从 UVC 摄像头抓取一张 JPEG，可指定设备索引或路径、分辨率、JPEG 质量和预热帧数。

库的公共头文件是 `include/hcnet_thermal.h`，包含红外和 UVC 两组接口以及状态码。Rust 集成示例见 `examples/rust/`。

## 4. 注意事项

- 设备是否支持 JPEG 抓图及分辨率取决于设备能力集。若返回错误 `23` 或 `29`，可调整编译选项 `HCNET_THERMAL_JPEG_PIC_SIZE`。
- 登录会话可能因网络中断、设备重启或超时失效，长期运行的程序应在外层处理重连。
- Windows 构建需要真实的 MSVC/MinGW 环境和对应的 SDK 库；使用 UVC 时还需将 OpenCV DLL 放在可执行文件同目录。

## 目录结构

```text
include/hcnet_thermal.h   公共 C ABI
src/                      红外与 UVC 实现
cli/                      命令行程序
examples/rust/            Rust 集成示例
CMakeLists.txt            构建配置
```
