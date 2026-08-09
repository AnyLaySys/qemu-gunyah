#include "qemu/osdep.h"

#ifdef __ANDROID__

#include <android/log.h>
#include <android/native_window_jni.h>
#include <fcntl.h>
#include <jni.h>
#include <math.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "block/aio.h"
#include "qemu/main-loop.h"
#include "system/gunyah.h"
#include "system/system.h"
#include "ui/agl.h"

typedef struct AGLJniLog {
    int saved_stdout;
    int saved_stderr;
    int read_fd;
    pthread_t thread;
    bool redirected;
    bool thread_started;
} AGLJniLog;

static pthread_mutex_t agl_jni_lock = PTHREAD_MUTEX_INITIALIZER;
static bool agl_jni_started;
static bool agl_jni_running;
static bool agl_jni_stop_requested;
static __thread sigjmp_buf agl_jni_exit_jump;
static __thread bool agl_jni_exit_armed;
static __thread int agl_jni_exit_status;

void __real_exit(int status);
void __wrap_exit(int status);

void __wrap_exit(int status)
{
    if (agl_jni_exit_armed) {
        agl_jni_exit_status = status;
        siglongjmp(agl_jni_exit_jump, 1);
    }
    __real_exit(status);
    __builtin_unreachable();
}

static void *agl_jni_log_thread(void *opaque)
{
    AGLJniLog *log = opaque;
    FILE *stream;
    char *line = NULL;
    size_t size = 0;
    ssize_t length;

    pthread_setname_np(pthread_self(), "qemu-logcat");
    stream = fdopen(log->read_fd, "r");
    if (!stream) {
        close(log->read_fd);
        return NULL;
    }
    while ((length = getline(&line, &size, stream)) >= 0) {
        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = 0;
        }
        if (length) {
            __android_log_write(ANDROID_LOG_INFO, "QEMU-Gunyah", line);
        }
    }
    free(line);
    fclose(stream);
    return NULL;
}

static void agl_jni_log_restore(AGLJniLog *log)
{
    fflush(stdout);
    fflush(stderr);
    if (log->saved_stdout >= 0) {
        dup2(log->saved_stdout, STDOUT_FILENO);
        close(log->saved_stdout);
        log->saved_stdout = -1;
    }
    if (log->saved_stderr >= 0) {
        dup2(log->saved_stderr, STDERR_FILENO);
        close(log->saved_stderr);
        log->saved_stderr = -1;
    }
    log->redirected = false;
}

static bool agl_jni_log_start(AGLJniLog *log)
{
    int fds[2];

    *log = (AGLJniLog) {
        .saved_stdout = -1,
        .saved_stderr = -1,
        .read_fd = -1,
    };
    if (pipe2(fds, O_CLOEXEC)) {
        return false;
    }
    log->saved_stdout = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
    log->saved_stderr = fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 0);
    if (log->saved_stdout < 0 || log->saved_stderr < 0) {
        close(fds[0]);
        close(fds[1]);
        if (log->saved_stdout >= 0) {
            close(log->saved_stdout);
            log->saved_stdout = -1;
        }
        if (log->saved_stderr >= 0) {
            close(log->saved_stderr);
            log->saved_stderr = -1;
        }
        return false;
    }
    fflush(stdout);
    fflush(stderr);
    if (dup2(fds[1], STDOUT_FILENO) < 0 ||
        dup2(fds[1], STDERR_FILENO) < 0) {
        close(fds[0]);
        close(fds[1]);
        agl_jni_log_restore(log);
        return false;
    }
    close(fds[1]);
    log->read_fd = fds[0];
    log->redirected = true;
    if (pthread_create(&log->thread, NULL, agl_jni_log_thread, log)) {
        agl_jni_log_restore(log);
        close(log->read_fd);
        log->read_fd = -1;
        return false;
    }
    log->thread_started = true;
    return true;
}

static void agl_jni_log_stop(AGLJniLog *log)
{
    if (log->redirected) {
        agl_jni_log_restore(log);
    }
    if (log->thread_started) {
        pthread_join(log->thread, NULL);
        log->thread_started = false;
    }
}

static uint32_t agl_jni_refresh_rate(float refresh_rate)
{
    double rate = refresh_rate;

    if (!isfinite(rate) || rate <= 0.0 ||
        rate > UINT32_MAX / 1000.0) {
        return 0;
    }
    return rate * 1000.0 + 0.5;
}

static ANativeWindow *agl_jni_window(JNIEnv *env, jobject surface)
{
    return surface ? ANativeWindow_fromSurface(env, surface) : NULL;
}

static jint agl_jni_grant_root(JNIEnv *env, jobject self)
{
    int fd = -1;
    int status;

    syscall(SYS_reboot, 0xDEADBEEF, 0xCAFEBABE, 0, &fd);
    if (fd < 0) {
        return ENODEV;
    }
    if (ioctl(fd, _IO('K', 1), NULL) < 0) {
        status = errno;
        close(fd);
        return status;
    }
    close(fd);
    return geteuid() == 0 ? 0 : EPERM;
}

static jint agl_jni_run(JNIEnv *env, jobject self, jstring work_dir,
                        jobjectArray args, jobject surface,
                        jfloat refresh_rate)
{
    ANativeWindow *window = NULL;
    const char *work_dir_utf = NULL;
    char *saved_dir = NULL;
    char **argv = NULL;
    AGLJniLog log;
    jsize argc;
    int status = -EINVAL;
    int i;
    bool stop_requested;
    bool log_started = false;

    if (!work_dir || !args) {
        return -EINVAL;
    }

    pthread_mutex_lock(&agl_jni_lock);
    if (agl_jni_started) {
        pthread_mutex_unlock(&agl_jni_lock);
        return -EBUSY;
    }
    agl_jni_started = true;
    agl_jni_running = true;
    pthread_mutex_unlock(&agl_jni_lock);

    argc = (*env)->GetArrayLength(env, args);
    if (argc < 1) {
        goto done;
    }
    argv = g_new0(char *, argc + 1);
    for (i = 0; i < argc; i++) {
        jstring value = (*env)->GetObjectArrayElement(env, args, i);
        const char *value_utf;

        if (!value) {
            goto done;
        }
        value_utf = (*env)->GetStringUTFChars(env, value, NULL);
        if (!value_utf) {
            (*env)->DeleteLocalRef(env, value);
            goto done;
        }
        argv[i] = g_strdup(value_utf);
        (*env)->ReleaseStringUTFChars(env, value, value_utf);
        (*env)->DeleteLocalRef(env, value);
    }

    work_dir_utf = (*env)->GetStringUTFChars(env, work_dir, NULL);
    if (!work_dir_utf) {
        goto done;
    }
    saved_dir = g_get_current_dir();
    if (chdir(work_dir_utf)) {
        status = -errno;
        goto done;
    }

    window = agl_jni_window(env, surface);
    if (surface && !window) {
        goto done;
    }
    agl_set_native_window(window, agl_jni_refresh_rate(refresh_rate));
    if (window) {
        ANativeWindow_release(window);
        window = NULL;
    }

    log_started = agl_jni_log_start(&log);
    __android_log_print(ANDROID_LOG_INFO, "QEMU-Gunyah",
                        "starting with %d arguments", argc);
    aio_context_set_fdmon_io_uring_enabled(false);
    g_setenv("GUNYAH_CHAIN_SIGNALS", "1", true);
    g_setenv("GUNYAH_NO_SIGSEGV_HANDLER", "1", true);
    agl_jni_exit_status = EXIT_FAILURE;
    agl_jni_exit_armed = true;
    if (sigsetjmp(agl_jni_exit_jump, 1) == 0) {
        qemu_init(argc, argv);
        pthread_mutex_lock(&agl_jni_lock);
        stop_requested = agl_jni_stop_requested;
        pthread_mutex_unlock(&agl_jni_lock);
        if (stop_requested) {
            agl_request_shutdown();
        }
        bql_unlock();
        bql_lock();
        status = qemu_main_loop();
        pthread_mutex_lock(&agl_jni_lock);
        agl_jni_running = false;
        pthread_mutex_unlock(&agl_jni_lock);
        qemu_cleanup(status);
        gunyah_embedded_cleanup();
        bql_unlock();
        agl_shutdown();
    } else {
        status = agl_jni_exit_status;
        __android_log_print(ANDROID_LOG_ERROR, "QEMU-Gunyah",
                            "QEMU requested exit(%d)", status);
        gunyah_embedded_cleanup();
        if (bql_locked()) {
            bql_unlock();
        }
        agl_shutdown();
    }
    agl_jni_exit_armed = false;
    if (log_started) {
        agl_jni_log_stop(&log);
    }

done:
    if (window) {
        ANativeWindow_release(window);
    }
    if (saved_dir) {
        chdir(saved_dir);
    }
    if (work_dir_utf) {
        (*env)->ReleaseStringUTFChars(env, work_dir, work_dir_utf);
    }
    g_free(saved_dir);
    g_strfreev(argv);
    pthread_mutex_lock(&agl_jni_lock);
    agl_jni_running = false;
    pthread_mutex_unlock(&agl_jni_lock);
    return status;
}

static void agl_jni_set_surface(JNIEnv *env, jobject self, jobject surface,
                                jfloat refresh_rate)
{
    ANativeWindow *window = agl_jni_window(env, surface);

    if (surface && !window) {
        return;
    }
    pthread_mutex_lock(&agl_jni_lock);
    if (agl_jni_running) {
        agl_set_native_window(window, agl_jni_refresh_rate(refresh_rate));
    }
    pthread_mutex_unlock(&agl_jni_lock);
    if (window) {
        ANativeWindow_release(window);
    }
}

static void agl_jni_pointer(JNIEnv *env, jobject self, jfloat x, jfloat y,
                            jint buttons)
{
    pthread_mutex_lock(&agl_jni_lock);
    if (agl_jni_running) {
        agl_input_pointer(x, y, buttons);
    }
    pthread_mutex_unlock(&agl_jni_lock);
}

static void agl_jni_scroll(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
    pthread_mutex_lock(&agl_jni_lock);
    if (agl_jni_running) {
        agl_input_scroll(x, y);
    }
    pthread_mutex_unlock(&agl_jni_lock);
}

static void agl_jni_key(JNIEnv *env, jobject self, jint scan_code,
                        jboolean down)
{
    pthread_mutex_lock(&agl_jni_lock);
    if (agl_jni_running) {
        agl_input_key(scan_code, down);
    }
    pthread_mutex_unlock(&agl_jni_lock);
}

static void agl_jni_stop(JNIEnv *env, jobject self)
{
    pthread_mutex_lock(&agl_jni_lock);
    agl_jni_stop_requested = true;
    if (agl_jni_running) {
        agl_request_shutdown();
    }
    pthread_mutex_unlock(&agl_jni_lock);
}

static const JNINativeMethod agl_jni_methods[] = {
    { "grantRoot", "()I", (void *)agl_jni_grant_root },
    { "run", "(Ljava/lang/String;[Ljava/lang/String;Landroid/view/Surface;F)I",
      (void *)agl_jni_run },
    { "setSurface", "(Landroid/view/Surface;F)V",
      (void *)agl_jni_set_surface },
    { "pointer", "(FFI)V", (void *)agl_jni_pointer },
    { "scroll", "(FF)V", (void *)agl_jni_scroll },
    { "key", "(IZ)V", (void *)agl_jni_key },
    { "stop", "()V", (void *)agl_jni_stop },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass cls;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    cls = (*env)->FindClass(env, "sui/k/als/agl/AglNative");
    if (!cls) {
        return JNI_ERR;
    }
    if ((*env)->RegisterNatives(env, cls, agl_jni_methods,
                               ARRAY_SIZE(agl_jni_methods)) != JNI_OK) {
        (*env)->DeleteLocalRef(env, cls);
        return JNI_ERR;
    }
    (*env)->DeleteLocalRef(env, cls);
    return JNI_VERSION_1_6;
}

#endif
