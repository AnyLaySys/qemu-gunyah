# QEMU Gunyah
[Gunyah](https://docs.qualcomm.com/doc/80-70029-3SC/topic/virtualization.html)是高通主导并开源的Type-1型虚拟化技术  
**QEMU Gunyah**是对应虚拟化管理器,具有如下特点  
## ✅完整设备支持  
| 核心设备 | 实现设备 | 技术职责 |
| :--- | :--- | :--- |
| **存储** | `virtio-blk-pci` | 负责存储控制<br>异步I/O机制`AIO IO_Uring`<br>独立I/O线程`Object IOThread` |
| **显示** | `ramfb`<br>`virtio-gpu-pci`<br>`virtio-gpu-gl-pci` | 负责图形画面输出:<br>`ramfb`像素缓冲渲染<br>`virtio-gpu-pci`2D软件渲染<br>`virtio-gpu-gl-pci`OpenGL图形渲染 |
| **输入** | `virtio-tablet-pci`<br>`virtio-keyboard-pci` | 负责基础交互,仿真绝对坐标平板与PCI键盘,传递键鼠指令 |
| **音频** | `virtio-snd-pci` | 负责音频流输入输出,将音频信号桥接至宿主机音频驱动 |
| **网络** | `virtio-net-pci` | 负责网络连接,将虚拟网卡桥接至宿主机以实现原生网络吞吐 |

## 🚀高效后端对接  
| 核心设备 | 对接实现 | 技术职责 |
| :--- | :--- | :--- |
| **显示** | `X11` | SDL对接X11,实现画面低延迟渲染 |
| **音频** | `AAudio` | 对接Android Audio,将音频流直通硬件混音器 |
| **网络** | `TAP` | 采用Linux标准`tap0`虚拟网卡接口,在宿主机创建网络隧道以实现原生网络吞吐 |

## 👍体积小巧轻量
工欲善其事,必先利其器.作为Gunyah专用虚拟化管理器,我们秉承'取其精华,去其糟粕',通过深度精简,qemu-system-aarch64本体小到令人惊讶的**1.5M**,整体(包括fw,lib)小到**24M**
# 如何使用
1 安装[Termux](https://github.com/termux/termux-app/releases)与[Termux:X11](https://github.com/termux/termux-x11/releases)  
2 打开Termux执行
```bash
pkg update && pkg install x11-repo -y && pkg update && pkg install termux-x11-nightly xfce4 xfce4-terminal -y
```
3 打开Termux:X11(建议浮窗显示),Termux执行
```bash
termux-x11 :1
```
4 解压产物到任何目录(如/data/local/tmp/als/qemu-gunyah),任意终端(如`adb shell`),依次执行
```bash
su
```
```bash
ip link show tap0 >/dev/null 2>&1 || ip tuntap add dev tap0 mode tap; ip addr flush dev tap0; ip addr add 100.99.99.1/24 dev tap0; ip link set tap0 up; sysctl -w net.ipv4.ip_forward=1; while ip rule del from 100.99.99.0/24 lookup wlan0 2>/dev/null; do :; done; while ip rule del to 100.99.99.0/24 lookup main 2>/dev/null; do :; done; ip rule add pref 100 from 100.99.99.0/24 lookup wlan0; ip rule add pref 101 to 100.99.99.0/24 lookup main; while iptables -D INPUT -i tap0 -j ACCEPT 2>/dev/null; do :; done; while iptables -D FORWARD -i tap0 -o wlan0 -j ACCEPT 2>/dev/null; do :; done; while iptables -D FORWARD -i wlan0 -o tap0 -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null; do :; done; iptables -I INPUT 1 -i tap0 -j ACCEPT; iptables -I FORWARD 1 -i tap0 -o wlan0 -j ACCEPT; iptables -I FORWARD 1 -i wlan0 -o tap0 -m state --state ESTABLISHED,RELATED -j ACCEPT; while iptables -t nat -D POSTROUTING -s 100.99.99.0/24 -o wlan0 -j MASQUERADE 2>/dev/null; do :; done; iptables -t nat -I POSTROUTING 1 -s 100.99.99.0/24 -o wlan0 -j MASQUERADE
```
```bash
printf "\033[2J\033[3J\033[H" && cd /data/local/tmp/als/qemu-gunyah && export DISPLAY=:1 XAUTHORITY=/data/data/com.termux/files/home/.Xauthority HOME=/data/data/com.termux/files/home TMPDIR=/data/data/com.termux/files/usr/tmp XDG_RUNTIME_DIR=/data/data/com.termux/files/usr/tmp LD_LIBRARY_PATH=/data/local/tmp/als/qemu-gunyah/lib:/system/lib64:/vendor/lib64 LD_PRELOAD=/data/local/tmp/als/qemu-gunyah/lib/libX11-dir.so X11_TMPDIR=/data/data/com.termux/files/usr/tmp SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=aaudio LANG=C LC_ALL=C && taskset 7f ./qemu-system-aarch64 -L ./fw -bios edk2-aarch64-gunyah.fd -M virt,confidential-guest-support=prot0 -accel gunyah -cpu host -smp 7 -m 4276M -object arm-confidential-guest,id=prot0,swiotlb-size=180M -object iothread,id=io0 -drive file=/sdcard/ubuntu-26.10-snapshot2-preinstalled-server-arm64.img,format=raw,if=none,id=dr0,media=disk,cache=unsafe,aio=io_uring,discard=unmap -device virtio-blk-pci,drive=dr0,num-queues=$(nproc),iothread=io0,disable-legacy=on,disable-modern=off,bootindex=1 -netdev tap,id=usernet,ifname=tap0,script=no,downscript=no -device virtio-net-pci,netdev=usernet -device virtio-tablet-pci -device virtio-keyboard-pci -device virtio-gpu-gl-pci,`wm size | awk -F'[:x ]+' '{print "xres="$4",yres="$3"}'` -audiodev aaudio,id=aa -device virtio-snd-pci,audiodev=aa -display sdl -serial mon:stdio
```
命令会自动获取设备分辨率
