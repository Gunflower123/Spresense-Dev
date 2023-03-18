/* Reduction made possible by Haashim */
#include <nuttx/config.h>
#include <nuttx/timers/pwm.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

//Must have configured pwm to be on. Not on by default

int main(int argc, FAR char *argv[]) {
  struct pwm_info_s info;
  info.frequency = 5;
  info.duty = (50 << 16)/100;
  int fd = open("/dev/pwm0", O_RDONLY);
  
  int red = ioctl(fd, PWMIOC_SETCHARACTERISTICS, (unsigned long) ((uintptr_t)&info));
  ret = ioctl(fd, PWMIOC_START, 0);
  
  close(fd);
  return 0;
}
