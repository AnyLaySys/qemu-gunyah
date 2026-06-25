============
qemu-gunyah
============

🚀 一个面向 AArch64 / Android / Gunyah 场景的精简版 QEMU。

它不是上游 QEMU 的完整发行形态，也不追求覆盖所有平台和设备。这个版本的目标比较
明确：

**在支持 Gunyah 的 Arm64 设备上，把 Linux 虚拟机尽量轻、尽量直接地跑起来。**

它保留图形、网络、音频、键鼠、块设备和串口调试这些常用能力；同时关闭或删除当前
启动路径不需要的组件，让构建更轻，运行路径也更清楚。🙂


Gunyah 是什么 🤔
=================

Gunyah 是 Qualcomm 开源的 Type-1 Hypervisor。

Type-1 Hypervisor 不是跑在普通应用层里的软件模拟器，而是更靠近系统底层的虚拟化
层。它主要面向移动设备、嵌入式、安全隔离和实时性要求更高的场景。

在这个项目里，QEMU 和 Gunyah 的分工大致是：

* QEMU 负责命令行、固件、虚拟设备、镜像、显示、音频等外围工作。
* Gunyah 负责运行 vCPU，并处理虚拟机隔离。
* 宿主内核通过 ``/dev/gunyah`` 向 QEMU 暴露接口。

所以启动参数里核心是：

.. code-block:: shell

  -accel gunyah -cpu host

这个构建已经裁掉 TCG。也就是说，它不会在 Gunyah 不可用时退回纯软件模拟；宿主环境
需要真正支持 Gunyah。


这个版本的定位
================

一句话：**把 ``qemu-system-aarch64`` 做成一个更适合 Android + Gunyah 的专用构建。**

目前只保留：

* ``qemu-system-aarch64``

被移除的主要是当前启动路径不会用到、但会增加依赖、体积和构建时间的组件。


当前支持的主要设备 ✅
=====================

机器 / 加速
------------

* ``-M virt``
* ``-accel gunyah``
* ``-cpu host``
* ``arm-confidential-guest``
* ``iothread``

这是标准 Arm ``virt`` 机器配合 Gunyah 加速，目标集中在 Arm64。


存储 💿
-------

* ``virtio-blk-pci``
* raw 镜像
* qcow2 镜像
* ISO 启动
* ``aio=threads``
* ``discard=unmap``

当前重点是挂载磁盘、挂载 ISO 并启动系统。SCSI 相关设备没有保留。


网络 🌐
-------

* ``-netdev user``
* ``virtio-net-pci``
* ``hostfwd``

例如把 guest 的 SSH 转到宿主 ``2222`` 端口：

.. code-block:: shell

  -netdev user,id=usernet,hostfwd=tcp::2222-:22
  -device virtio-net-pci,netdev=usernet

这样可以满足日常联网和 SSH 调试。


图形 🖥️
-------

* ``virtio-gpu-pci``
* ``-display sdl``
* SDL 走 X11

这里需要说明清楚：``virtio-gpu-pci`` 负责把画面显示出来，但它不是 GPU 直通，也
不是 gfxstream/VirGL。这个构建里 OpenGL / VirGL 是关闭的，所以 guest 里看到
llvmpipe 是正常现象。

运行桌面环境可以；硬件 3D 加速不在当前目标里。


输入 ⌨️
-------

* ``virtio-keyboard-pci``
* ``virtio-tablet-pci``

``virtio-mouse-pci`` 和 ``virtio-multitouch-pci`` 已经移除。当前 SDL 桌面使用
tablet + keyboard 更直接，也减少了不必要的设备类型。


音频 🔊
-------

* ``-audiodev aaudio``
* ``virtio-snd-pci``

AAudio 更贴近 Android 侧的运行环境，不依赖 PulseAudio/ALSA 这类桌面 Linux 音频栈。


串口 / Monitor 🛠️
------------------

* ``-serial mon:stdio``

用于启动日志、调试和 QEMU monitor 交互。排查启动问题时，这个参数很有用。


已经裁掉的内容 ✂️
==================

这个版本主要做减法。已经关闭或删除的大项包括：

* TCG
* VNC
* GTK
* VFIO
* CXL
* IOMMUFD
* Xen
* vhost-user / vhost-vdpa / VDUSE
* virtio-scsi
* virtio-serial
* virtio-rng
* virtio-balloon
* virtio-mem
* virtio-pmem
* virtio-crypto
* 多数当前用不到的块格式
* 除 ``aarch64-softmmu`` 以外的 system emulator

这些组件并不是“不好”，只是当前目标不需要。保留它们会增加构建复杂度，也会让调试
路径变得更长。


构建 🔧
=======

在仓库根目录运行：

.. code-block:: shell

  ./2.sh

脚本会准备 Android aarch64 交叉编译环境、SDL2/X11/AAudio 相关依赖，并只构建
``aarch64-softmmu`` 目标。

最终输出目录是 ``qemu-gunyah/``：

.. code-block:: text

  qemu-gunyah/
  ├── qemu-system-aarch64
  ├── fw/
  └── lib/


启动示例 🚀
============

下面是当前比较贴近实际使用的启动命令。路径按自己的设备调整。

.. code-block:: shell

  export DISPLAY=:1
  export XAUTHORITY=/data/data/com.termux/files/home/.Xauthority
  export HOME=/data/data/com.termux/files/home
  export TMPDIR=/data/data/com.termux/files/usr/tmp
  export XDG_RUNTIME_DIR=/data/data/com.termux/files/usr/tmp
  export LD_LIBRARY_PATH=/data/local/tmp/als/qemu-gunyah/lib:/system/lib64:/vendor/lib64
  export SDL_VIDEODRIVER=x11
  export SDL_AUDIODRIVER=aaudio
  export LANG=C
  export LC_ALL=C

  ./qemu-system-aarch64 \
    -L ./fw \
    -bios edk2-aarch64-gunyah.fd \
    -M virt,confidential-guest-support=prot0 \
    -accel gunyah \
    -cpu host \
    -smp 4 \
    -m 4G \
    -object arm-confidential-guest,id=prot0,swiotlb-size=64M \
    -object iothread,id=io0 \
    -drive file=/sdcard/ubuntu-26.04-desktop-arm64.iso,format=raw,if=none,id=dr0,media=cdrom,readonly=on,cache=unsafe,aio=threads,discard=unmap \
    -device virtio-blk-pci,drive=dr0,num-queues=$(nproc),iothread=io0,disable-legacy=on,disable-modern=off,bootindex=1 \
    -netdev user,id=usernet,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=usernet \
    -device virtio-tablet-pci \
    -device virtio-keyboard-pci \
    -device virtio-gpu-pci,xres=2376,yres=1080 \
    -display sdl \
    -audiodev aaudio,id=aa \
    -device virtio-snd-pci,audiodev=aa \
    -serial mon:stdio


优点 😎
=======

* CPU 不走 TCG，而是走 Gunyah。
* 只保留当前实际使用的设备。
* SDL/X11/AAudio 更贴近 Android 侧运行环境。
* 源码树和构建目标更轻，减少无关依赖。
* 排查问题时路径更短，不容易被未启用的后端干扰。

重点是实用：能启动、能显示、能交互、能调试。


限制 ⚠️
=======

* 这不是完整 QEMU。
* 这不是硬件 GPU 加速方案。
* 这不是 Windows 启动方案。
* 这不是跨架构模拟器。
* 这版没有 TCG fallback。

它更像一个专用构建：Arm64 Linux + Gunyah + virtio + SDL + AAudio。范围更小，目标
也更清楚。🙂


License
=======

基于 QEMU，许可证继续按 QEMU 原来的来。看 ``LICENSE``、``COPYING`` 和源码头部。


----

由 Codex 撰写。
