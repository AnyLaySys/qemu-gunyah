#!/usr/bin/env bash
set -euo pipefail
scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
apiLevel="36"
buildDir="$scriptDir/build"
srcDir="$buildDir/src"
outDir="$buildDir/out"
prefix="$buildDir/sysroot"
qemuBuild="$buildDir/qemu"
qemuDir="$outDir/qemu-gunyah"
qemuFw="$qemuDir/fw"
qemuLib="$qemuDir/lib"
alsDir="${ALS_DIR:-/mnt/c/DESIGN/als}"
jniDir="$alsDir/app/src/main/jniLibs/arm64-v8a"
epoxySrc="$srcDir/libepoxy"
libucontextSrc="$srcDir/libucontext"
liburingSrc="$srcDir/liburing"
virglSrc="$srcDir/virglrenderer"
virglPatch="$scriptDir/patch/virglrenderer_android.patch"
targetTriple="aarch64-linux-android"
cmakeAbi="arm64-v8a"
mesonCpu="aarch64"
glibVer="2.83.0"
libffiVer="3.4.4"
pcre2Ver="10.44"
pixmanVer="0.46.4"
hostOs="$(uname -s | tr '[:upper:]' '[:lower:]')"
nCpu="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
ndkPath="${ANDROID_NDK_ROOT:-$HOME/android-ndk-r30-beta2}"
qemuRawUrl="https://gitlab.com/qemu-project/qemu/-/raw/master"
epoxyGitUrl="https://github.com/anholt/libepoxy.git"
libucontextGitUrl="https://github.com/kaniini/libucontext.git"
liburingGitUrl="https://github.com/axboe/liburing.git"
virglGitUrl="https://github.com/AnyLaySys/virglrenderer.git"
case "$hostOs" in
  linux) hostTag="linux-x86_64" ;;
  darwin) hostTag="darwin-x86_64" ;;
  *) echo "不支持此系统: $hostOs" >&2; exit 1 ;;
esac
toolchain="$ndkPath/toolchains/llvm/prebuilt/$hostTag"
hostCC="${HOST_CC:-$(command -v cc || true)}"
readelf="$toolchain/bin/llvm-readelf"
strip="$toolchain/bin/llvm-strip"
commonCFlags="-O3 -flto=thin -ffunction-sections -fdata-sections -fomit-frame-pointer -mcpu=oryon-1 -fPIC -fno-semantic-interposition -ftls-model=global-dynamic"
libraryCFlags="$commonCFlags -DNDEBUG"
commonLdFlags="-flto=thin -Wl,--lto-O3 -Wl,-O3 -Wl,--gc-sections -Wl,--icf=all"
qemuCFlags="$commonCFlags -fno-unwind-tables -fno-asynchronous-unwind-tables -mbranch-protection=none -Wno-error -I$prefix/include -I$prefix/include/pixman-1"
qemuLdFlags="-L$prefix/lib $commonLdFlags -Wl,-s -lucontext"
for tool in cmake curl git make meson ninja patchelf perl pkg-config python3 tar; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "缺少工具: $tool" >&2
    exit 1
  fi
done
if [ -z "$hostCC" ]; then
  echo "缺少宿主 C 编译器" >&2
  exit 1
fi
if [ ! -x "$toolchain/bin/${targetTriple}${apiLevel}-clang" ]; then
  echo "缺少 Android NDK: $ndkPath" >&2
  exit 1
fi
if [ ! -f "$virglPatch" ]; then
  echo "缺少补丁: $virglPatch" >&2
  exit 1
fi
if [ ! -d "$alsDir/app/src/main" ]; then
  echo "缺少 ALS 工程: $alsDir" >&2
  exit 1
fi
export AR="$toolchain/bin/llvm-ar"
export CC="$toolchain/bin/${targetTriple}${apiLevel}-clang"
export CXX="$toolchain/bin/${targetTriple}${apiLevel}-clang++"
export LD="$toolchain/bin/ld.lld"
export NM="$toolchain/bin/llvm-nm"
export OBJCOPY="$toolchain/bin/llvm-objcopy"
export RANLIB="$toolchain/bin/llvm-ranlib"
export STRIP="$strip"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
fetch() {
  local url="$1"
  local out="$2"
  if [ ! -f "$out" ]; then
    echo "下载 $url"
    curl -L --fail --retry 3 -o "$out" "$url"
  fi
}
fetchGit() {
  local url="$1"
  local dir="$2"
  shift 2
  if [ ! -d "$dir" ]; then
    echo "克隆 $url -> $dir"
    git clone --depth 1 --single-branch --no-tags --filter=blob:none --recurse-submodules --shallow-submodules --also-filter-submodules --jobs "$nCpu" "$@" "$url" "$dir"
  fi
}
applyPatch() {
  local repo="$1"
  local patchFile="$2"
  local label="$3"
  if git -C "$repo" apply --reverse --check "$patchFile" 2>/dev/null; then
    return 0
  fi
  if ! git -C "$repo" apply --check "$patchFile"; then
    echo "$label 无法应用" >&2
    exit 1
  fi
  git -C "$repo" apply "$patchFile"
}
writeMesonCross() {
  local file="$1"
  local extraC="${2:-}"
  local extraLink="${3:-}"
  cat > "$file" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = '${PKG_CONFIG:-pkg-config}'
[built-in options]
c_args = ['-O3','-DNDEBUG','-flto=thin','-ffunction-sections','-fdata-sections','-fomit-frame-pointer','-mcpu=oryon-1','-fPIC','-fno-semantic-interposition','-ftls-model=global-dynamic','-Wno-error','-I$prefix/include'$extraC]
cpp_args = ['-O3','-DNDEBUG','-flto=thin','-ffunction-sections','-fdata-sections','-fomit-frame-pointer','-mcpu=oryon-1','-fPIC','-fno-semantic-interposition','-ftls-model=global-dynamic','-Wno-error','-I$prefix/include'$extraC]
c_link_args = ['-L$prefix/lib','-flto=thin','-Wl,--lto-O3','-Wl,-O3','-Wl,--gc-sections','-Wl,--icf=all'$extraLink]
cpp_link_args = ['-L$prefix/lib','-flto=thin','-Wl,--lto-O3','-Wl,-O3','-Wl,--gc-sections','-Wl,--icf=all'$extraLink]
[host_machine]
system = 'linux'
cpu_family = '$mesonCpu'
cpu = '$mesonCpu'
endian = 'little'
EOF
}
neededLibs() {
  "$readelf" -d "$1" | awk '
    index($0, "Shared library: [") {
      name = $0
      sub(/^.*Shared library: [[]/, "", name)
      sub(/[]].*$/, "", name)
      print name
    }'
}
isSystemLib() {
  case "$1" in
    libc.so|libm.so|libdl.so|liblog.so|libz.so|libandroid.so|libaaudio.so|libOpenSLES.so|libEGL.so|libGLESv2.so|libGLESv3.so) return 0 ;;
    *) return 1 ;;
  esac
}
findLib() {
  local neededName="$1"
  local baseName="$neededName"
  if [ -f "$prefix/lib/$neededName" ]; then
    echo "$prefix/lib/$neededName"
    return 0
  fi
  while [[ "$baseName" == *.so.* ]]; do
    baseName="${baseName%.*}"
    if [ -f "$prefix/lib/$baseName" ]; then
      echo "$prefix/lib/$baseName"
      return 0
    fi
  done
  return 1
}
copyRuntimeLib() {
  local neededName="$1"
  local sourcePath
  local destPath="$qemuLib/$neededName"
  if isSystemLib "$neededName"; then
    return 0
  fi
  if ! sourcePath="$(findLib "$neededName")"; then
    echo "缺少依赖: $neededName" >&2
    return 1
  fi
  if [ ! -f "$destPath" ]; then
    cp -Lf "$sourcePath" "$destPath"
    "$strip" --strip-all "$destPath"
    patchelf --set-soname "$neededName" "$destPath"
    patchelf --set-rpath '$ORIGIN' "$destPath"
    runtimeQueue+=("$destPath")
  fi
}
collectRuntime() {
  local elfPath
  local neededName
  local index=0
  runtimeQueue=("$@")
  while [ "$index" -lt "${#runtimeQueue[@]}" ]; do
    elfPath="${runtimeQueue[$index]}"
    index=$((index + 1))
    while IFS= read -r neededName; do
      [ -n "$neededName" ] && copyRuntimeLib "$neededName"
    done < <(neededLibs "$elfPath")
  done
}
jniName() {
  local name="$1"
  if [[ "$name" == *.so.* ]]; then
    echo "${name%%.so.*}.so"
  else
    echo "$name"
  fi
}
collectJni() {
  local elfPath
  local neededName
  local sourcePath
  local packedName
  local destPath
  local index=0
  local jniSeen="|"
  jniQueue=("$@")
  for elfPath in "${jniQueue[@]}"; do
    jniSeen+="$elfPath|"
  done
  while [ "$index" -lt "${#jniQueue[@]}" ]; do
    elfPath="${jniQueue[$index]}"
    index=$((index + 1))
    while IFS= read -r neededName; do
      [ -z "$neededName" ] && continue
      if isSystemLib "$neededName"; then
        continue
      fi
      if ! sourcePath="$(findLib "$neededName")"; then
        echo "缺少 JNI 依赖: $neededName" >&2
        return 1
      fi
      packedName="$(jniName "$neededName")"
      destPath="$jniDir/$packedName"
      if [ "$neededName" != "$packedName" ]; then
        patchelf --replace-needed "$neededName" "$packedName" "$elfPath"
      fi
      if [[ "$jniSeen" != *"|$destPath|"* ]]; then
        if [ ! -f "$destPath" ]; then
          cp -Lf "$sourcePath" "$destPath"
          "$strip" --strip-all "$destPath"
          patchelf --set-soname "$packedName" "$destPath"
          patchelf --set-rpath '$ORIGIN' "$destPath"
        fi
        jniSeen+="$destPath|"
        jniQueue+=("$destPath")
      fi
    done < <(neededLibs "$elfPath")
  done
}
fetchSources() {
  mkdir -p "$srcDir" "$outDir" "$prefix/lib" "$prefix/include"
  fetchGit "$libucontextGitUrl" "$libucontextSrc"
  fetchGit "$liburingGitUrl" "$liburingSrc" --branch liburing-2.8
  fetchGit "$epoxyGitUrl" "$epoxySrc"
  fetchGit "$virglGitUrl" "$virglSrc"
  fetch "https://github.com/libffi/libffi/releases/download/v${libffiVer}/libffi-${libffiVer}.tar.gz" "$srcDir/libffi-${libffiVer}.tar.gz"
  fetch "https://github.com/PhilipHazel/pcre2/releases/download/pcre2-${pcre2Ver}/pcre2-${pcre2Ver}.tar.bz2" "$srcDir/pcre2-${pcre2Ver}.tar.bz2"
  fetch "https://download.gnome.org/sources/glib/${glibVer%.*}/glib-${glibVer}.tar.xz" "$srcDir/glib-${glibVer}.tar.xz"
  fetch "https://www.cairographics.org/releases/pixman-${pixmanVer}.tar.gz" "$srcDir/pixman-${pixmanVer}.tar.gz"
  if [ ! -f "$scriptDir/subprojects/dtc/meson.build" ] || [ ! -f "$scriptDir/subprojects/keycodemapdb/README" ]; then
    meson subprojects download --sourcedir "$scriptDir" dtc keycodemapdb
  fi
}
buildLibffi() {
  if [ -f "$prefix/lib/libffi.so" ]; then
    return 0
  fi
  [ -d "$srcDir/libffi-${libffiVer}" ] || tar -C "$srcDir" -xf "$srcDir/libffi-${libffiVer}.tar.gz"
  mkdir -p "$outDir/libffi"
  pushd "$outDir/libffi"
  if [ -f Makefile ]; then
    make distclean
  fi
  local configureArgs=(
    --host="$targetTriple"
    --prefix="$prefix"
    --enable-shared
    --disable-static
    --disable-exec-static-tramp
  )
  env CFLAGS="$libraryCFlags" LDFLAGS="$commonLdFlags" "$srcDir/libffi-${libffiVer}/configure" "${configureArgs[@]}"
  make -j"$nCpu"
  make install
  popd
}
buildPcre2() {
  if [ -f "$prefix/lib/libpcre2-8.so" ]; then
    return 0
  fi
  [ -d "$srcDir/pcre2-${pcre2Ver}" ] || tar -C "$srcDir" -xf "$srcDir/pcre2-${pcre2Ver}.tar.bz2"
  rm -rf "$outDir/pcre2"
  local cmakeArgs=(
    -G Ninja
    -S "$srcDir/pcre2-${pcre2Ver}"
    -B "$outDir/pcre2"
    -DCMAKE_TOOLCHAIN_FILE="$ndkPath/build/cmake/android.toolchain.cmake"
    -DANDROID_ABI="$cmakeAbi"
    -DANDROID_PLATFORM="android-${apiLevel}"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="$prefix"
    -DCMAKE_C_FLAGS="$libraryCFlags"
    -DCMAKE_SHARED_LINKER_FLAGS="$commonLdFlags"
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_STATIC_LIBS=OFF
    -DPCRE2_BUILD_PCRE2_8=ON
    -DPCRE2_BUILD_PCRE2_16=OFF
    -DPCRE2_BUILD_PCRE2_32=OFF
    -DPCRE2_BUILD_PCRE2GREP=OFF
    -DPCRE2_BUILD_TESTS=OFF
    -DPCRE2_SUPPORT_JIT=OFF
  )
  cmake "${cmakeArgs[@]}"
  cmake --build "$outDir/pcre2" --target pcre2-8-shared -j"$nCpu"
  mkdir -p "$prefix/include" "$prefix/lib/pkgconfig"
  install -m 755 "$outDir/pcre2/libpcre2-8.so" "$prefix/lib/libpcre2-8.so"
  install -m 644 "$outDir/pcre2/pcre2.h" "$prefix/include/pcre2.h"
  install -m 644 "$outDir/pcre2/libpcre2-8.pc" "$prefix/lib/pkgconfig/libpcre2-8.pc"
}
buildGlib() {
  if [ -f "$prefix/lib/libglib-2.0.so" ]; then
    return 0
  fi
  [ -d "$srcDir/glib-${glibVer}" ] || tar -C "$srcDir" -xf "$srcDir/glib-${glibVer}.tar.xz"
  sed -i "/  'lchmod',/d" "$srcDir/glib-${glibVer}/meson.build"
  sed -i "s/if cc.has_header_symbol('pthread.h', 'pthread_getaffinity_np', prefix : pthread_prefix)/if false and cc.has_header_symbol('pthread.h', 'pthread_getaffinity_np', prefix : pthread_prefix)/" "$srcDir/glib-${glibVer}/meson.build"
  writeMesonCross "$outDir/glib.cross"
  rm -rf "$outDir/glib"
  local mesonArgs=(
    --cross-file "$outDir/glib.cross"
    --prefix "$prefix"
    -Ddefault_library=shared
    -Doptimization=3
    -Ddebug=false
    -Dglib_debug=disabled
    -Dglib_assert=false
    -Dglib_checks=false
    -Dtests=false
    -Dman-pages=disabled
    -Ddocumentation=false
    -Dselinux=disabled
    -Dlibmount=disabled
    -Dnls=disabled
  )
  meson setup "$outDir/glib" "$srcDir/glib-${glibVer}" "${mesonArgs[@]}"
  meson compile -C "$outDir/glib" -j"$nCpu"
  meson install -C "$outDir/glib"
}
buildPixman() {
  if [ -f "$prefix/lib/libpixman-1.so" ]; then
    return 0
  fi
  [ -d "$srcDir/pixman-${pixmanVer}" ] || tar -C "$srcDir" -xf "$srcDir/pixman-${pixmanVer}.tar.gz"
  writeMesonCross "$outDir/pixman.cross"
  rm -rf "$outDir/pixman"
  local mesonArgs=(
    --cross-file "$outDir/pixman.cross"
    --prefix "$prefix"
    -Ddefault_library=shared
    -Dbuildtype=release
    -Dtests=disabled
    -Ddemos=disabled
    -Dgtk=disabled
    -Dlibpng=disabled
    -Dopenmp=disabled
    -Darm-simd=disabled
    -Dneon=disabled
    -Da64-neon=enabled
  )
  meson setup "$outDir/pixman" "$srcDir/pixman-${pixmanVer}" "${mesonArgs[@]}"
  meson compile -C "$outDir/pixman" -j"$nCpu"
  meson install -C "$outDir/pixman"
}
buildLibucontext() {
  local bitsInstalled="$prefix/include/libucontext/bits.h"
  local libucontextH="$prefix/include/libucontext/libucontext.h"
  if [ ! -f "$prefix/lib/libucontext.a" ]; then
    pushd "$libucontextSrc"
    make ARCH=aarch64 clean
    make ARCH=aarch64 CC="$CC" AR="$AR" RANLIB="$RANLIB" CFLAGS="$libraryCFlags" LDFLAGS="$commonLdFlags" FREESTANDING=yes EXPORT_UNPREFIXED=yes -j"$nCpu" libucontext.a libucontext.pc
    mkdir -p "$prefix/lib" "$prefix/lib/pkgconfig" "$prefix/include/libucontext"
    cp -f libucontext.a "$prefix/lib/"
    cp -f libucontext.pc "$prefix/lib/pkgconfig/"
    cp -f include/libucontext/libucontext.h "$prefix/include/libucontext/"
    popd
  fi
  mkdir -p "$prefix/include/libucontext"
  cat > "$prefix/include/ucontext.h" <<'EOF'
#ifndef _ANDROID_UCONTEXT_SHIM_H
#define _ANDROID_UCONTEXT_SHIM_H
#include <sys/ucontext.h>
#include <libucontext/libucontext.h>
#endif
EOF
  cat > "$bitsInstalled" <<'EOF'
#ifndef LIBUCONTEXT_BITS_H
#define LIBUCONTEXT_BITS_H
#include <stddef.h>
typedef struct {
    unsigned long long fault_address;
    unsigned long long regs[31];
    unsigned long long sp;
    unsigned long long pc;
    unsigned long long pstate;
    unsigned char __reserved[4096] __attribute__((__aligned__(16)));
} libucontext_mcontext_t;
typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} libucontext_stack_t;
typedef struct libucontext_ucontext {
    unsigned long uc_flags;
    struct libucontext_ucontext *uc_link;
    libucontext_stack_t uc_stack;
    unsigned char __pad[136];
    libucontext_mcontext_t uc_mcontext;
} libucontext_ucontext_t;
#endif
EOF
  if grep -Fq 'void (*)()' "$libucontextH"; then
    perl -0pi -e 's[void [(][*][)][(][)]][void (*)(void)]g' "$libucontextH"
  fi
}
buildLiburing() {
  if [ -f "$prefix/lib/liburing.a" ] && [ -f "$prefix/lib/pkgconfig/liburing.pc" ]; then
    return 0
  fi
  pushd "$liburingSrc"
  make clean
  env CFLAGS="$libraryCFlags" LDFLAGS="$commonLdFlags" ./configure --prefix="$prefix" --cc="$CC" --cxx="$CXX"
  make -C src CFLAGS="$libraryCFlags" LDFLAGS="$commonLdFlags" liburing.a -j"$nCpu"
  make liburing.pc
  install -D -m 644 src/include/liburing.h "$prefix/include/liburing.h"
  install -D -m 644 src/include/liburing/io_uring.h "$prefix/include/liburing/io_uring.h"
  install -D -m 644 src/include/liburing/compat.h "$prefix/include/liburing/compat.h"
  install -D -m 644 src/include/liburing/barrier.h "$prefix/include/liburing/barrier.h"
  install -D -m 644 src/include/liburing/sanitize.h "$prefix/include/liburing/sanitize.h"
  install -D -m 644 src/include/liburing/io_uring_version.h "$prefix/include/liburing/io_uring_version.h"
  install -D -m 644 src/liburing.a "$prefix/lib/liburing.a"
  install -D -m 644 liburing.pc "$prefix/lib/pkgconfig/liburing.pc"
  popd
}
preparePkgConfig() {
  local wrapper="$outDir/android-pkg-config"
  {
    echo '#!/usr/bin/env bash'
    echo "export PKG_CONFIG_PATH='$prefix/lib/pkgconfig:$prefix/share/pkgconfig'"
    echo "export PKG_CONFIG_LIBDIR='$prefix/lib/pkgconfig:$prefix/share/pkgconfig'"
    echo 'exec pkg-config "$@"'
  } > "$wrapper"
  chmod +x "$wrapper"
  export PKG_CONFIG="$wrapper"
}
buildEpoxy() {
  if [ -f "$prefix/lib/pkgconfig/epoxy.pc" ] && [ -f "$prefix/lib/libepoxy.so" ]; then
    return 0
  fi
  writeMesonCross "$outDir/epoxy.cross"
  rm -rf "$outDir/epoxy"
  local mesonArgs=(
    --cross-file "$outDir/epoxy.cross"
    --prefix "$prefix"
    -Ddefault_library=shared
    -Doptimization=3
    -Ddebug=false
    -Degl=yes
    -Dglx=no
    -Dx11=false
    -Dtests=false
  )
  meson setup "$outDir/epoxy" "$epoxySrc" "${mesonArgs[@]}"
  meson compile -C "$outDir/epoxy" -j"$nCpu"
  meson install -C "$outDir/epoxy"
}
buildVirglrenderer() {
  local compatDir="$prefix/include/compat"
  applyPatch "$virglSrc" "$virglPatch" "VirGLRenderer Android 补丁"
  rm -f "$prefix/bin/virgl_test_server"
  if [ -f "$prefix/lib/pkgconfig/virglrenderer.pc" ] && [ -f "$prefix/lib/libvirglrenderer.so" ]; then
    return 0
  fi
  mkdir -p "$compatDir/log" "$compatDir/cutils"
  cat > "$compatDir/log/log.h" <<'EOF'
#ifndef _COMPAT_LOG_LOG_H
#define _COMPAT_LOG_LOG_H
#include <android/log.h>
#ifndef LOG_PRI
#define LOG_PRI(priority, tag, ...) __android_log_print(priority, tag, __VA_ARGS__)
#endif
#endif
EOF
  cat > "$compatDir/cutils/properties.h" <<'EOF'
#ifndef _COMPAT_CUTILS_PROPERTIES_H
#define _COMPAT_CUTILS_PROPERTIES_H
#include <string.h>
#ifndef PROPERTY_VALUE_MAX
#define PROPERTY_VALUE_MAX 92
#endif
#ifndef PROPERTY_KEY_MAX
#define PROPERTY_KEY_MAX 32
#endif
static inline int property_get(const char *key, char *value, const char *def)
{
    (void)key;
    if (def) {
        strncpy(value, def, PROPERTY_VALUE_MAX - 1);
        value[PROPERTY_VALUE_MAX - 1] = 0;
        return strlen(value);
    }
    *value = 0;
    return 0;
}
#endif
EOF
  writeMesonCross "$outDir/virgl.cross" ",'-I$compatDir'" ",'-llog'"
  rm -rf "$outDir/virglrenderer"
  local mesonArgs=(
    --cross-file "$outDir/virgl.cross"
    --prefix "$prefix"
    -Ddefault_library=shared
    -Doptimization=3
    -Ddebug=false
    -Dtests=false
    '-Dplatforms=[]'
    -Dcheck-gl-errors=false
  )
  meson setup "$outDir/virglrenderer" "$virglSrc" "${mesonArgs[@]}"
  meson compile -C "$outDir/virglrenderer" -j"$nCpu"
  meson install -C "$outDir/virglrenderer"
}
buildQemu() {
  export CFLAGS="$qemuCFlags"
  export CXXFLAGS="$qemuCFlags"
  export LDFLAGS="$qemuLdFlags"
  local configureArgs=(
    --prefix="$prefix"
    --host-cc="$hostCC"
    --cross-prefix="${targetTriple}-"
    --cc="$CC"
    --cxx="$CXX"
    --target-list=aarch64-softmmu
    --audio-drv-list=aaudio
    --disable-download
    --disable-docs
    --disable-guest-agent
    --disable-cocoa
    --disable-curses
    --disable-capstone
    --disable-gnutls
    --disable-gcrypt
    --disable-libusb
    --disable-usb-redir
    --disable-tpm
    --disable-vhost-kernel
    --disable-vhost-net
    --disable-vhost-vdpa
    --disable-vhost-user
    --disable-virtfs
    --disable-slirp
    --enable-fdt=internal
    --enable-pixman
    -Dtools=disabled
    -Doptimization=3
    -Ddebug=false
    -Db_ndebug=false
    -Dqom_cast_debug=false
    -Db_staticpic=true
    -Dcoroutine_pool=true
    -Dcoroutine_backend=sigaltstack
    -Dvirglrenderer=enabled
    -Ddbus_display=disabled
    -Dgunyah=enabled
    -Dlinux_io_uring=enabled
    -Dmalloc_trim=disabled
    -Dxen=disabled
    -Dxen_pci_passthrough=disabled
    -Dmultiprocess=disabled
    -Dreplication=disabled
    -Dbochs=disabled
    -Dcloop=disabled
    -Ddmg=disabled
    -Dqcow1=disabled
    -Dvdi=disabled
    -Dvhdx=disabled
    -Dvmdk=disabled
    -Dvpc=disabled
    -Dvvfat=disabled
    -Dqed=disabled
    -Dparallels=disabled
    -Dzstd=disabled
    -Dl2tpv3=disabled
    -Dattr=disabled
    -Dxkbcommon=disabled
    -Dpng=disabled
    -Dspice=disabled
    -Dspice_protocol=disabled
    -Dopengl=enabled
  )
  rm -rf "$qemuBuild"
  mkdir -p "$qemuBuild"
  pushd "$qemuBuild"
  STRIP="$strip" "$scriptDir/configure" "${configureArgs[@]}"
  popd
  local mesonBin="$qemuBuild/pyvenv/bin/meson"
  if [ ! -x "$mesonBin" ]; then
    mesonBin="$(command -v meson)"
  fi
  "$mesonBin" compile -C "$qemuBuild" qemu-system-aarch64 qemu-gunyah -j"$nCpu"
}
packageQemu() {
  local qemuBinary="$qemuBuild/qemu-system-aarch64"
  local jniBinary="$qemuBuild/libqemu-gunyah.so"
  if [ ! -f "$qemuBinary" ] || [ ! -f "$jniBinary" ]; then
    echo "QEMU 构建产物不完整" >&2
    exit 1
  fi
  rm -rf "$qemuDir"
  mkdir -p "$qemuLib" "$qemuFw"
  fetch "$qemuRawUrl/pc-bios/efi-virtio.rom" "$srcDir/efi-virtio.rom"
  cp -f "$srcDir/efi-virtio.rom" "$qemuFw/efi-virtio.rom"
  "$strip" --strip-all "$qemuBinary" -o "$qemuDir/qemu-system-aarch64"
  "$strip" --strip-all "$jniBinary" -o "$qemuDir/libqemu-gunyah.so"
  patchelf --set-rpath '$ORIGIN/lib' "$qemuDir/qemu-system-aarch64"
  patchelf --set-rpath '$ORIGIN/lib' "$qemuDir/libqemu-gunyah.so"
  collectRuntime "$qemuDir/qemu-system-aarch64" "$qemuDir/libqemu-gunyah.so"
  mkdir -p "$jniDir"
  "$strip" --strip-all "$jniBinary" -o "$jniDir/libqemu-gunyah.so"
  patchelf --set-rpath '$ORIGIN' "$jniDir/libqemu-gunyah.so"
  collectJni "$jniDir/libqemu-gunyah.so"
  echo "QEMU: $qemuDir"
  echo "JNI: $jniDir/libqemu-gunyah.so"
}
mkdir -p "$buildDir" "$srcDir" "$outDir" "$prefix"
fetchSources
buildLibffi
buildPcre2
buildGlib
buildPixman
buildLibucontext
buildLiburing
preparePkgConfig
buildEpoxy
buildVirglrenderer
buildQemu
packageQemu
