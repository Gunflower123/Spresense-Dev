#include <nuttx/config.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
//#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <arch/chip/gnss.h>

//Pre-processor Definitions
#define GNSS_POLL_FD_NUM          1
#define GNSS_POLL_TIMEOUT_FOREVER -1
#define MY_GNSS_SIG               18

struct cxd56_gnss_dms_s {
  int8_t   sign;
  uint8_t  degree;
  uint8_t  minute;
  uint32_t frac;
};

static uint32_t posfixflag;
static struct cxd56_gnss_positiondata_s posdat;

static void double_to_dmf(double x, struct cxd56_gnss_dms_s * dmf) {
  int    b,d,m;
  double f,t;

  if (x < 0) {
      b = 1;
      x = -x;
    }
  else
    b = 0;
  
  d = (int)x; /* = floor(x), x is always positive */
  t = (x - d) * 60;
  m = (int)t; /* = floor(t), t is always positive */
  f = (t - m) * 10000;

  dmf->sign   = b;
  dmf->degree = d;
  dmf->minute = m;
  dmf->frac   = f;
}

static int read_and_print(int f){
  int ret;
  struct cxd56_gnss_dms_s dmf;

  //Read POS data
  ret = read(fd, &posdat, sizeof(posdat));
  ret = OK;
  
  //Print POS data - time
  printf(">Hour:%d, minute:%d, sec:%d, usec:%ld\n", posdat.receiver.time.hour,
         posdat.receiver.time.minute,posdat.receiver.time.sec, posdat.receiver.time.usec);
  if (posdat.receiver.pos_fixmode != CXD56_GNSS_PVT_POSFIX_INVALID) {
    posfixflag = 1;
    double_to_dmf(posdat.receiver.latitude, &dmf);
    printf(">LAT %d.%d.%04ld\n", dmf.degree, dmf.minute, dmf.frac);
    
    double_to_dmf(posdat.receiver.longitude, &dmf);
    printf(">LNG %d.%d.%04ld\n", dmf.degree, dmf.minute, dmf.frac);
  }
  else
    printf(">No Positioning Data\n");
}

static int gnss_setparams(int fd) {
  int      ret = 0;
  uint32_t set_satellite;
  struct cxd56_gnss_ope_mode_param_s set_opemode;

  //Set GNSS operation interval
  set_opemode.mode     = 1;     //Normal(Default) operational mode
  set_opemode.cycle    = 1000;  //Position notify cycle(ms step)

  ret = ioctl(fd, CXD56_GNSS_IOCTL_SET_OPE_MODE, (uint32_t)&set_opemode);

  //Set satellite system type
  set_satellite = CXD56_GNSS_SAT_GPS | CXD56_GNSS_SAT_GLONASS;
  ret = ioctl(fd, CXD56_GNSS_IOCTL_SELECT_SATELLITE_SYSTEM, set_satellite);
}


int main(int argc, FAR char *argv[])
{
  int      fd, ret, posperiod;
  sigset_t mask;
  struct cxd56_gnss_signal_setting_s setting;
  
  printf("Hello, GNSS(USE_SIGNAL) SAMPLE!!\n");
  fd = open("/dev/gps", O_RDONLY);

  //Mask configuration for GNSS signal
  sigemptyset(&mask);
  sigaddset(&mask, MY_GNSS_SIG);
  ret = sigprocmask(SIG_BLOCK, &mask, NULL);
  
  //Signal notifies GNSS events
  setting.fd      = fd;
  setting.enable  = 1;
  setting.gnsssig = CXD56_GNSS_SIG_GNSS;
  setting.signo   = MY_GNSS_SIG;
  setting.data    = NULL;

  ret = ioctl(fd, CXD56_GNSS_IOCTL_SIGNAL_SET, (unsigned long)&setting);

  //Set GNSS parameters
  ret = gnss_setparams(fd);
  
  //Cold vs Hot start - Ephemeris
  posperiod  = 200;
  posfixflag = 0;
  
  //GNSS start
  ret = ioctl(fd, CXD56_GNSS_IOCTL_START, CXD56_GNSS_STMOD_HOT);
  printf("start GNSS OK\n");
  
  do {
    ret = sigwaitinfo(&mask, NULL);
    if (ret != MY_GNSS_SIG) {
      printf("sigwaitinfo error %d\n", ret);
      break;
    }
    
    /* Read and print POS data. */
    ret = read_and_print(fd);
    if (ret < 0)
      break;
    if (posfixflag)
      //Countdown after POS fixed
      posperiod--;
  } while(posperiod > 0);
  
  //Stop GNSS
  ret = ioctl(fd, CXD56_GNSS_IOCTL_STOP, 0);
  printf("stop GNSS OK\n");

  //GNSS firmware disable signal after positioning
  setting.enable = 0;
  ret = ioctl(fd, CXD56_GNSS_IOCTL_SIGNAL_SET, (unsigned long)&setting);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  //Release GNSS fd
  ret = close(fd);
  printf("End of GNSS Sample:%d\n", ret);
  return ret;
}
