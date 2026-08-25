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
#include "audio/audio.h"
#include "block/aio.h"
#include "qemu/main-loop.h"
#include "system/gunyah.h"
#include "system/runstate.h"
#include "system/system.h"
#include "ui/agl.h"
#include "ui/console.h"
#include "ui/input.h"

typedef struct LogPipe {
    int saved_stdout;
    int saved_stderr;
    int read_fd;
    pthread_t thread;
    bool redirected;
    bool thread_started;
} LogPipe;

enum { INPUT_POINTER, INPUT_SCROLL, INPUT_KEY };
typedef struct JniInput {
    int type;
    float x, y;
    int value;
    bool down;
    QTAILQ_ENTRY(JniInput) next;
} JniInput;

static struct {
    pthread_mutex_t lock;
    QTAILQ_HEAD(, JniInput) queue;
    bool scheduled;
    uint32_t buttons;
    int x, y;
} input = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .queue = QTAILQ_HEAD_INITIALIZER(input.queue),
};

static pthread_mutex_t jni_lock = PTHREAD_MUTEX_INITIALIZER;
static bool jni_started;
static bool jni_running;
static bool jni_stop_requested;
static __thread sigjmp_buf jni_exit_jump;
static __thread bool jni_exit_armed;
static __thread int jni_exit_status;

static bool jni_active(void)
{
    bool active;

    pthread_mutex_lock(&jni_lock);
    active = jni_running;
    pthread_mutex_unlock(&jni_lock);
    return active;
}

static void input_send(JniInput *event)
{
    static uint32_t map[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT] = 1, [INPUT_BUTTON_RIGHT] = 2,
        [INPUT_BUTTON_MIDDLE] = 4, [INPUT_BUTTON_SIDE] = 8,
        [INPUT_BUTTON_EXTRA] = 16,
    };
    QemuConsole *console = qemu_console_lookup_default();

    if (!console) {
        return;
    }
    if (event->type == INPUT_POINTER) {
        int x, y, width, height;

        if (!agl_map(event->x, event->y, &x, &y, &width, &height)) {
            return;
        }
        qemu_input_update_buttons(console, map, input.buttons, event->value);
        if (qemu_input_is_absolute(console)) {
            qemu_input_queue_abs(console, INPUT_AXIS_X, x, 0, width);
            qemu_input_queue_abs(console, INPUT_AXIS_Y, y, 0, height);
        } else {
            qemu_input_queue_rel(console, INPUT_AXIS_X, x - input.x);
            qemu_input_queue_rel(console, INPUT_AXIS_Y, y - input.y);
        }
        input.buttons = event->value;
        input.x = x;
        input.y = y;
        qemu_input_event_sync();
    } else if (event->type == INPUT_SCROLL) {
        InputButton v = event->y < 0 ? INPUT_BUTTON_WHEEL_UP :
                                       INPUT_BUTTON_WHEEL_DOWN;
        InputButton h = event->x < 0 ? INPUT_BUTTON_WHEEL_LEFT :
                                       INPUT_BUTTON_WHEEL_RIGHT;

        if (event->y) {
            qemu_input_queue_btn(console, v, true);
            qemu_input_event_sync();
            qemu_input_queue_btn(console, v, false);
            qemu_input_event_sync();
        }
        if (event->x) {
            qemu_input_queue_btn(console, h, true);
            qemu_input_event_sync();
            qemu_input_queue_btn(console, h, false);
            qemu_input_event_sync();
        }
    } else {
        int qcode = qemu_input_linux_to_qcode(event->value);

        if (qcode != Q_KEY_CODE_UNMAPPED) {
            qemu_input_event_send_key_qcode(console, qcode, event->down);
        }
    }
}

static void input_bh(void *opaque)
{
    JniInput *event;

    (void)opaque;
    for (;;) {
        pthread_mutex_lock(&input.lock);
        event = QTAILQ_FIRST(&input.queue);
        if (event) {
            QTAILQ_REMOVE(&input.queue, event, next);
        } else {
            input.scheduled = false;
        }
        pthread_mutex_unlock(&input.lock);
        if (!event) {
            return;
        }
        input_send(event);
        g_free(event);
    }
}

static void input_push(int type, float x, float y, int value, bool down)
{
    JniInput *event = g_new(JniInput, 1);
    bool schedule;

    *event = (JniInput) { type, x, y, value, down };
    pthread_mutex_lock(&input.lock);
    QTAILQ_INSERT_TAIL(&input.queue, event, next);
    schedule = !input.scheduled;
    input.scheduled = true;
    pthread_mutex_unlock(&input.lock);
    if (schedule) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(), input_bh, NULL);
    }
}

static void input_clear(void)
{
    JniInput *event;

    pthread_mutex_lock(&input.lock);
    while ((event = QTAILQ_FIRST(&input.queue))) {
        QTAILQ_REMOVE(&input.queue, event, next);
        g_free(event);
    }
    input.scheduled = false;
    input.buttons = input.x = input.y = 0;
    pthread_mutex_unlock(&input.lock);
}

void __real_exit(int status);
void __wrap_exit(int status);

void __wrap_exit(int status)
{
    if (jni_exit_armed) {
        jni_exit_status = status;
        siglongjmp(jni_exit_jump, 1);
    }
    __real_exit(status);
    __builtin_unreachable();
}

static void *jni_log_thread(void *opaque)
{
    LogPipe *log = opaque;
    FILE *stream;
    char *line = NULL;
    size_t size = 0;
    ssize_t length;

    pthread_setname_np(pthread_self(), "logcat");
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

static void jni_log_restore(LogPipe *log)
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

static bool jni_log_start(LogPipe *log)
{
    int fds[2];

    *log = (LogPipe) {
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
        jni_log_restore(log);
        return false;
    }
    close(fds[1]);
    log->read_fd = fds[0];
    log->redirected = true;
    if (pthread_create(&log->thread, NULL, jni_log_thread, log)) {
        jni_log_restore(log);
        close(log->read_fd);
        log->read_fd = -1;
        return false;
    }
    log->thread_started = true;
    return true;
}

static void jni_log_stop(LogPipe *log)
{
    if (log->redirected) {
        jni_log_restore(log);
    }
    if (log->thread_started) {
        pthread_join(log->thread, NULL);
        log->thread_started = false;
    }
}

static uint32_t jni_refresh_rate(float refresh_rate)
{
    double rate = refresh_rate;

    if (!isfinite(rate) || rate <= 0.0 ||
        rate > UINT32_MAX / 1000.0) {
        return 0;
    }
    return rate * 1000.0 + 0.5;
}

static ANativeWindow *jni_window(JNIEnv *env, jobject surface)
{
    return surface ? ANativeWindow_fromSurface(env, surface) : NULL;
}

static jint jni_grant_root(JNIEnv *env, jobject self)
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

static jint jni_run(JNIEnv *env, jobject self, jstring work_dir,
                    jobjectArray args, jint audio_fd)
{
    const char *work_dir_utf = NULL;
    char *saved_dir = NULL;
    char **argv = NULL;
    LogPipe log;
    jsize argc;
    int status = -EINVAL;
    int i;
    bool stop_requested;
    bool log_started = false;

    if (!work_dir || !args) {
        return -EINVAL;
    }

    pthread_mutex_lock(&jni_lock);
    if (jni_started) {
        pthread_mutex_unlock(&jni_lock);
        return -EBUSY;
    }
    jni_started = true;
    jni_running = true;
    pthread_mutex_unlock(&jni_lock);

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

    aaudio_set_input_fd(audio_fd);
    log_started = jni_log_start(&log);
    __android_log_print(ANDROID_LOG_INFO, "QEMU-Gunyah",
                        "starting with %d arguments", argc);
    aio_context_set_fdmon_io_uring_enabled(false);
    g_setenv("GUNYAH_CHAIN_SIGNALS", "1", true);
    g_setenv("GUNYAH_NO_SIGSEGV_HANDLER", "1", true);
    jni_exit_status = EXIT_FAILURE;
    jni_exit_armed = true;
    if (sigsetjmp(jni_exit_jump, 1) == 0) {
        qemu_init(argc, argv);
        pthread_mutex_lock(&jni_lock);
        stop_requested = jni_stop_requested;
        pthread_mutex_unlock(&jni_lock);
        if (stop_requested) {
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
        }
        bql_unlock();
        bql_lock();
        status = qemu_main_loop();
        pthread_mutex_lock(&jni_lock);
        jni_running = false;
        pthread_mutex_unlock(&jni_lock);
        qemu_cleanup(status);
        gunyah_embedded_cleanup();
        bql_unlock();
    } else {
        status = jni_exit_status;
        __android_log_print(ANDROID_LOG_ERROR, "QEMU-Gunyah",
                            "QEMU requested exit(%d)", status);
        gunyah_embedded_cleanup();
        if (bql_locked()) {
            bql_unlock();
        }
    }
    jni_exit_armed = false;
    input_clear();
    agl_cleanup();
    if (log_started) {
        jni_log_stop(&log);
    }

done:
    aaudio_set_input_fd(-1);
    if (saved_dir) {
        chdir(saved_dir);
    }
    if (work_dir_utf) {
        (*env)->ReleaseStringUTFChars(env, work_dir, work_dir_utf);
    }
    g_free(saved_dir);
    g_strfreev(argv);
    pthread_mutex_lock(&jni_lock);
    jni_running = false;
    jni_started = false;
    jni_stop_requested = false;
    pthread_mutex_unlock(&jni_lock);
    return status;
}

static void jni_set_surface(JNIEnv *env, jobject self, jobject surface,
                            jfloat refresh_rate)
{
    ANativeWindow *window = jni_window(env, surface);

    if (surface && !window) {
        return;
    }
    agl_set_window(window, jni_refresh_rate(refresh_rate));
    if (window) {
        ANativeWindow_release(window);
    }
}

static void jni_pointer(JNIEnv *env, jobject self, jfloat x, jfloat y,
                        jint buttons)
{
    if (jni_active()) {
        input_push(INPUT_POINTER, x, y, buttons, false);
    }
}

static void jni_scroll(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
    if (jni_active()) {
        input_push(INPUT_SCROLL, x, y, 0, false);
    }
}

static void jni_key(JNIEnv *env, jobject self, jint scan_code,
                    jboolean down)
{
    if (jni_active()) {
        input_push(INPUT_KEY, 0, 0, scan_code, down);
    }
}

static void jni_stop(JNIEnv *env, jobject self)
{
    pthread_mutex_lock(&jni_lock);
    jni_stop_requested = true;
    if (jni_running) {
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
    }
    pthread_mutex_unlock(&jni_lock);
}

static const JNINativeMethod vm_methods[] = {
    { "grantRoot", "()I", (void *)jni_grant_root },
    { "run", "(Ljava/lang/String;[Ljava/lang/String;I)I",
      (void *)jni_run },
    { "pointer", "(FFI)V", (void *)jni_pointer },
    { "scroll", "(FF)V", (void *)jni_scroll },
    { "key", "(IZ)V", (void *)jni_key },
    { "stop", "()V", (void *)jni_stop },
};

static const JNINativeMethod agl_methods[] = {
    { "setSurface", "(Landroid/view/Surface;F)V", (void *)jni_set_surface },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass cls;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    cls = (*env)->FindClass(env, "sui/k/als/vm/VMNative");
    if (!cls) {
        return JNI_ERR;
    }
    if ((*env)->RegisterNatives(env, cls, vm_methods,
                               ARRAY_SIZE(vm_methods)) != JNI_OK) {
        (*env)->DeleteLocalRef(env, cls);
        return JNI_ERR;
    }
    (*env)->DeleteLocalRef(env, cls);
    cls = (*env)->FindClass(env, "sui/k/als/vm/AGL");
    if (!cls || (*env)->RegisterNatives(env, cls, agl_methods,
                                       ARRAY_SIZE(agl_methods)) != JNI_OK) {
        return JNI_ERR;
    }
    (*env)->DeleteLocalRef(env, cls);
    return JNI_VERSION_1_6;
}

#endif
