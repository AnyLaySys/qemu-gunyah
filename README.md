# QEMU Gunyah
[Gunyah](https://docs.qualcomm.com/doc/80-70029-3SC/topic/virtualization.html)是高通主导并开源的Type-1型虚拟化技术  
QEMU Gunyah是对应的虚拟化管理器,具有如下特点  
✅完整设备支持  
| 核心设备 | 实现设备 | 技术职责 |
| :--- | :--- | :--- |
| <nobr>**存储**</nobr> | `virtio-blk-pci` | 负责根文件系统镜像的挂载与引导，通过独立 I/O 线程与异步 I/O 机制提供极致的块设备读写吞吐。 |
| <nobr>**显示**</nobr> | `ramfb`<br>`virtio-gpu-pci`<br>`virtio-gpu-gl-pci` | 负责图形画面输出：<br>1. `ramfb` 提供最基础的独立线性像素缓冲区（通常用于引导阶段显卡驱动加载前的画面显示）。<br>2. `virtio-gpu-pci` 提供标准的 2D 纯软件渲染显示。<br>3. `virtio-gpu-gl-pci` 提供基于 OpenGL 3D 硬件加速的高性能图形渲染。 |
| <nobr>**输入**</nobr> | `virtio-tablet-pci`<br>`virtio-keyboard-pci` | 负责基础人机交互，虚拟出绝对坐标平板与标准的 PCI 键盘，精准传递键鼠及触摸手势指令。 |
| <nobr>**音频**</nobr> | `virtio-snd-pci` | 负责多媒体音频流的输入与输出，将客体系统的音频信号无缝桥接至宿主机低延迟音频驱动。 |
| <nobr>**网络**</nobr> | `virtio-net-pci` | 负责虚拟机的网络连接，采用高性能 TAP 虚拟网卡架构，提供接近原生的网络吞吐与通信能力。 |
