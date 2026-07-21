# hcnet-thermal-picture

海康威视红外热成像摄像头项目

### 1. SDK 下载

https://open.hikvision.com/download/5cda567cf47ae80dd41a54b3?type=10&id=5cda5902f47ae80dd41a54b7

选择下载对应的 SDK 包（Linux64 或 ArmLinux64）

将目录中头文件和库目录分别拷贝到 include 和 lib 目录下


### 2. 编译

```bash
make
```

生成 `hcnet_capture_cli`。它通过当前目录的 `lib/` 加载海康 SDK。

### 3. Python 调用

```bash
./venv/bin/python -c 'from hcnet_thermal_picture import capture_picture; print(len(capture_picture()))'
```

`capture_picture()` 在临时目录中调用当前项目根目录的 `hcnet_capture_cli`，读取其生成的 JPG 并返回 `bytes`：

```python
from hcnet_thermal_picture import capture_picture
jpeg = capture_picture()
```
