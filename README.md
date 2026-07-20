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

生成 `libhcnet_thermal_picture.so`。

### 3. Python 安装

```bash
uv pip install .
```

或者构建 wheel：

```bash
make wheel
```

安装后可直接：

```python
from hcnet_thermal_picture import capture_picture
result = capture_picture()
```
