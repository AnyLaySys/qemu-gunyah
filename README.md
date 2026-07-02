# QEMU Gunyah
[Gunyah](https://docs.qualcomm.com/doc/80-70029-3SC/topic/virtualization.html)是高通主导并开源的Type-1型虚拟化技术  
QEMU Gunyah是对应的虚拟化管理器,具有如下特点  
✅完整设备支持  
| 核心设备 | 实现设备 | 技术职责 |
| :--- | :--- | :--- |
| **存储** | `virtio-blk-pci` | 负责存储控制<br>独立I/O线程`Object IOThread`<br>异步I/O机制`AIO IO_Uring` |
| **显示** | `ramfb`<br>`virtio-gpu-pci`<br>`virtio-gpu-gl-pci` | 负责图形画面输出:<br>`ramfb`像素缓冲渲染<br>`virtio-gpu-pci`2D软件渲染<br>`virtio-gpu-gl-pci`OpenGL图形渲染 |
| **输入** | `virtio-tablet-pci`<br>`virtio-keyboard-pci` | 负责基础交互,仿真绝对坐标平板与PCI键盘,传递键鼠指令 |
| **音频** | `virtio-snd-pci` | 负责音频流输入输出,将音频信号桥接至宿主机音频驱动 |
| **网络** | `virtio-net-pci` | 负责网络连接,将虚拟网卡桥接至宿主机以实现原生网络吞吐 |

✅高效后端对接  
| 核心设备 | 对接技术 | 技术职责 |
| :--- | :--- | :--- |
| **显示** | `SDL`+`X11` | 绘图后端对接X11,实现画面低延迟渲染 |
| **音频** | `AAudio` | 对接Android Audio,将音频流直通硬件混音器 |
| **网络** | `TAP` | 采用Linux标准`tap0`虚拟网卡接口,在宿主机创建网络隧道以实现原生网络吞吐 |
