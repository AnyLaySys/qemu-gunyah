# QEMU Gunyah
## 🟢已发布最终版(26.7.8)
[Gunyah](https://docs.qualcomm.com/doc/80-70029-3SC/topic/virtualization.html)是高通主导并开源的Type-1型虚拟化技术  
**QEMU Gunyah**是对应虚拟化管理器,具有如下特点
### ✅完整设备支持
| 核心设备 | 实现设备 | 技术职责 |
| :--- | :--- | :--- |
| **存储** | `virtio-blk-pci` | 负责存储控制<br>异步I/O机制`AIO IO_Uring`<br>独立I/O线程`Object IOThread` |
| **显示** | `virtio-gpu-gl-pci` | 负责图形画面输出与OpenGL/VirGL图形渲染 |
| **输入** | `virtio-tablet-pci`<br>`virtio-keyboard-pci` | 负责基础交互,仿真绝对坐标平板与PCI键盘,传递键鼠指令 |
| **音频** | `virtio-snd-pci` | 负责音频流输入输出,将音频信号桥接至宿主机音频驱动 |
| **网络** | `virtio-net-pci` | 负责网络连接,将虚拟网卡桥接至宿主机以实现原生网络吞吐 |
### 🚀高效后端对接
| 核心设备 | 对接实现 | 技术职责 |
| :--- | :--- | :--- |
| **显示** | `X11` | SDL对接X11,实现画面低延迟渲染 |
| **音频** | `AAudio` | 对接Android Audio,将音频流直通硬件混音器 |
| **网络** | `TAP` | 采用Linux标准`tap0`虚拟网卡接口,在宿主机创建网络隧道以实现原生网络吞吐 |
### 👍体积小巧轻量
工欲善其事,必先利其器.作为Gunyah专用虚拟化管理器,我们秉承'取其精华,去其糟粕',通过深度精简,qemu-system-aarch64本体小到令人惊讶的**1.6M**,整体(包括fw,lib)小到**17.7M**
# 使用
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
printf "\033[2J\033[3J\033[H" && cd /data/local/tmp/als/qemu-gunyah && export DISPLAY=:1 XAUTHORITY=/data/data/com.termux/files/home/.Xauthority HOME=/data/data/com.termux/files/home TMPDIR=/data/data/com.termux/files/usr/tmp XDG_RUNTIME_DIR=/data/data/com.termux/files/usr/tmp LD_LIBRARY_PATH=/data/local/tmp/als/qemu-gunyah/lib:/system/lib64:/vendor/lib64 LD_PRELOAD=/data/local/tmp/als/qemu-gunyah/lib/libX11-dir.so X11_TMPDIR=/data/data/com.termux/files/usr/tmp SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=aaudio LANG=C LC_ALL=C && taskset 3f ./qemu-system-aarch64 -L ./fw -bios edk2-aarch64-gunyah.fd -M virt,confidential-guest-support=prot0 -accel gunyah -cpu host -smp 6 -m 4276M -object arm-confidential-guest,id=prot0,swiotlb-size=192M -object iothread,id=io0 -drive file=/data/local/tmp/ubuntu-26.10-s2-gnome-arm64.img,format=raw,if=none,id=dr0,media=disk,cache=unsafe,aio=io_uring,discard=unmap -device virtio-blk-pci,drive=dr0,num-queues=$(nproc),iothread=io0,disable-legacy=on,disable-modern=off,bootindex=1 -netdev tap,id=usernet,ifname=tap0,script=no,downscript=no -device virtio-net-pci,netdev=usernet -device virtio-tablet-pci -device virtio-keyboard-pci -device virtio-gpu-gl-pci,`wm size | awk -F'[:x ]+' '{print "xres="$4",yres="$3"}'` -audiodev aaudio,id=aa -device virtio-snd-pci,audiodev=aa -display sdl -serial mon:stdio
```
命令内容(包括但不限于产物解压到路径,光盘路径,磁盘路径等)需据实修改.  
命令会获取屏幕分辨率,可修改(如`-device virtio-gpu-gl-pci,xres=2376,yres=1080`)
# 沿革
丙午三月十六日,时**wasdwasd0105**先生隐于市,穷数月之功推演.大功告成之际,其慨然发一语:`我这算是前无古人了`.  
此语既出,石破天惊.时余闻之,自负深入此道已久,遂以古人自居,连番逼问,语带机锋.先自暗忖不过是chroot/proot凡相,后听闻非也,又窃以为不过crosvm而已.惊闻其为`qemu直接调用gunyah`,如遭雷轰,方知井外有天,遂紧密配合先生开展协同测试.先生倾囊相授,余受益匪浅,心自感激.  
然造化弄人,因工作缠身,精力渐分,此惊世之作暂缓更新.余一则志趣所向,二则使命所驱,每念及此,不忍明珠蒙尘.想此'前无古人'之薪火,岂可无'薪火相传'之拓土者?故本项目应运而生.  
成立以来,屡遭波折.幸天眷顾,团队同好不遗余力,建言献策,助余绘制蓝图与统筹推进,乃有下文.先刮臃去肿,又提优增效...先生依旧拨冗看望,在瓶颈之时指点迷津,助项目稳中求进.  
云山苍苍,江水泱泱,先生之风,山高水长.  
回首过往,展望未来.当前虽有初步成效,然任重道远,不容懈怠.未来团队将保持'一张蓝图绘到底,一以贯之抓落实'的战略定力,坚信'锲而不舍,金石可镂',持续努力,久久为功,让qemu-gunyah于方寸屏幕纵横捭阖的光辉理想照进现实!
### 鸣谢
[wasdwasd0105](https://github.com/wasdwasd0105):草创之际,全赖君光.若无先生,必无此章  
**以下排名不分先后,按首字母顺序排列**  
[Alhkxsj](https://github.com/Alhkxsj):建言献策,协同测试  
[Goldzxcbug](https://github.com/Goldzxcbug):匡扶危局,协同测试  
[loeveo](https://github.com/loeveo):匡扶危局,协同测试  
[longhanlop](https://github.com/longhanlop):协同测试  
[qeqenn](https://github.com/qeqenn):QALS前端,贡献代码,建言献策,协同测试  
[XiaoChen515144](https://github.com/XiaoChen515144):协同测试
