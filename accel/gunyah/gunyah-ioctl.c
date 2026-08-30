static int gunyah_ioctl(int type, ...) {
  void *arg;
  va_list ap;
  GUNYAHState *s = GUNYAH_STATE(current_accel());
  assert(s->fd);
  va_start(ap, type);
  arg = va_arg(ap, void *);
  va_end(ap);
  return ioctl(s->fd, type, arg);
}
int gunyah_vm_ioctl(int type, ...) {
  void *arg;
  va_list ap;
  GUNYAHState *s = GUNYAH_STATE(current_accel());
  assert(s->vmfd);
  va_start(ap, type);
  arg = va_arg(ap, void *);
  va_end(ap);
  return ioctl(s->vmfd, type, arg);
}
static int gunyah_vcpu_ioctl(CPUState *cpu, int type, ...) {
  void *arg;
  va_list ap;
  va_start(ap, type);
  arg = va_arg(ap, void *);
  va_end(ap);
  return ioctl(cpu->accel->fd, type, arg);
}
