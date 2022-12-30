#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>

#include <nuttx/timers/rtc.h>

static bool g_alarm_daemon_started;
static pid_t g_alarm_daemon_pid;
static bool g_alarm_received[CONFIG_RTC_NALARMS];

static void alarm_handler(int signo, FAR siginfo_t *info, FAR void *ucontext) {
  int almndx = info->si_value.sival_int;
  if (almndx >= 0 && almndx < CONFIG_RTC_NALARMS)
  {g_alarm_received[almndx] = true;}
}

static int alarm_daemon(int argc, FAR char *argv[]) {
  struct sigaction act;
  sigset_t set;
  int ret;
  int i;

  g_alarm_daemon_started = true;
  sigemptyset(&set);                    //Make sure alarm signal is unmasked
  sigaddset(&set, CONFIG_EXAMPLES_ALARM_SIGNO);
  ret = sigprocmask(SIG_UNBLOCK, &set, NULL);
  
  //Register alarm signal handler
  act.sa_sigaction = alarm_handler;
  act.sa_flags     = SA_SIGINFO;

  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, CONFIG_EXAMPLES_ALARM_SIGNO);

  ret = sigaction(CONFIG_EXAMPLES_ALARM_SIGNO, &act, NULL);

  //Now loop forever, waiting for alarm signals
  for (; ; ) {
    for (i = 0; i < CONFIG_RTC_NALARMS; i++) {
      if (g_alarm_received[i])
      {g_alarm_received[i] = false;}
    }
    usleep(500 * 1000L);
  }
}

static int start_daemon(void) {
  if (!g_alarm_daemon_started) {
    g_alarm_daemon_pid = task_create("alarm_daemon", CONFIG_EXAMPLES_ALARM_PRIORITY,
                                     CONFIG_EXAMPLES_ALARM_STACKSIZE, alarm_daemon, NULL);
    usleep(500 * 1000L);}
  return OK;
}


int main(int argc, FAR char *argv[]) {
  unsigned long seconds = 100000;
  // bool readmode = false;
  // bool cancelmode = false;
  bool setmode;
  int alarmid = 0;
  int fd;
  int ret;
  
  ret = start_daemon(); //Start Alarm Daemon
  fd = open(CONFIG_EXAMPLES_ALARM_DEVPATH, O_WRONLY);
  if (readmode) {
    struct rtc_rdalarm_s rd = {0};
    
    long timeleft;
    time_t now;
    
    rd.id = alarmid;
    time(&now);
    
    ret = ioctl(fd, RTC_RD_ALARM, (unsigned long)((uintptr_t)&rd));
    if (rd.time.tm_year > 0)
      timeleft = mktime((struct tm *)&rd.time) - now;
    else {
      struct tm now_tm;
      gmtime_r(&now, &now_tm);
      rd.time.tm_mon = now_tm.tm_mon;
      rd.time.tm_year = now_tm.tm_year;
      
      do {
        timeleft = mktime((struct tm *)&rd.time) - now;
        if (timeleft < 0)
          rd.time.tm_mon++;
      } while(timeleft < 0);
    }
  }
  
  if (cancelmode)
    ret = ioctl(fd, RTC_CANCEL_ALARM, (unsigned long)alarmid);
  
  if (setmode) {
    struct rtc_setrelative_s setrel;
    
    setrel.id      = alarmid;
    setrel.pid     = g_alarm_daemon_pid;
    setrel.reltime = (time_t)seconds;
    
    setrel.event.sigev_notify = SIGEV_SIGNAL;
    setrel.event.sigev_signo  = CONFIG_EXAMPLES_ALARM_SIGNO;
    
    ret = ioctl(fd, RTC_SET_RELATIVE, (unsigned long)((uintptr_t)&setrel));
    printf("Alarm %d set in %lu seconds\n", alarmid, seconds);
  }
  
  close(fd);
  return EXIT_SUCCESS;
}
