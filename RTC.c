```
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

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_alarm_daemon_started;
static pid_t g_alarm_daemon_pid;
static bool g_alarm_received[CONFIG_RTC_NALARMS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Name: alarm_handler*/
static void alarm_handler(int signo, FAR siginfo_t *info, FAR void *ucontext)
{
  int almndx = info->si_value.sival_int;
  if (almndx >= 0 && almndx < CONFIG_RTC_NALARMS)
    {
      g_alarm_received[almndx] = true;
    }
}

/*Name: alarm_daemon*/
static int alarm_daemon(int argc, FAR char *argv[])
{
  struct sigaction act;
  sigset_t set;
  int ret;
  int i;

  //Indicate that we are running
  g_alarm_daemon_started = true;
  printf("alarm_daemon: Running\n");

  //Make sure that the alarm signal is unmasked
  sigemptyset(&set);
  sigaddset(&set, CONFIG_EXAMPLES_ALARM_SIGNO);
  ret = sigprocmask(SIG_UNBLOCK, &set, NULL);
  if (ret != OK)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: sigprocmask failed: %d\n", errcode);
      goto errout;
    }

  //Register alarm signal handler
  act.sa_sigaction = alarm_handler;
  act.sa_flags     = SA_SIGINFO;

  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, CONFIG_EXAMPLES_ALARM_SIGNO);

  ret = sigaction(CONFIG_EXAMPLES_ALARM_SIGNO, &act, NULL);
  if (ret < 0)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: sigaction failed: %d\n", errcode);
      goto errout;
    }
    
  //Now loop forever, waiting for alarm signals
  for (; ; )
    {
      /* Check if any alarms fired.
       *
       * NOTE that there are race conditions here... if we missing an alarm,
       * we will just report it a half second late.
       */

      for (i = 0; i < CONFIG_RTC_NALARMS; i++)
        {
          if (g_alarm_received[i])
            {
              printf("alarm_daemon: alarm %d received\n", i)  ;
              g_alarm_received[i] = false;
            }
        }

      /* Now wait a little while and poll again.  If a signal is received
       * this should cause us to awken earlier.
       */

      usleep(500 * 1000L);
    }

errout:
  g_alarm_daemon_started = false;

  printf("alarm_daemon: Terminating\n");
  return EXIT_FAILURE;
}

/* Name: start_daemon*/
static int start_daemon(void)
{
  if (!g_alarm_daemon_started)
    {
      g_alarm_daemon_pid =
        task_create("alarm_daemon", CONFIG_EXAMPLES_ALARM_PRIORITY,
                    CONFIG_EXAMPLES_ALARM_STACKSIZE, alarm_daemon, NULL);
      if (g_alarm_daemon_pid < 0)
        {
          int errcode = errno;
          fprintf(stderr, "ERROR: Failed to start alarm_daemon: %d\n", errcode);
          return -errcode;
        }
      printf("alarm_daemon started\n");
      usleep(500 * 1000L);
    }

  return OK;
}


/****************************************************************************
 * Public Functions
 ****************************************************************************/
//alarm_main
int main(int argc, FAR char *argv[])
{
  unsigned long seconds = 0;
  bool badarg = false;
  bool readmode = false;
  bool cancelmode = false;
  bool setmode;
  int opt;
  int alarmid = 0;
  int fd;
  int ret;

  //Make sure that the alarm daemon is running
  ret = start_daemon();
  if (ret < 0)
    {return EXIT_FAILURE;}

  /* Parse commandline parameters. NOTE: getopt() is not thread safe nor re-entrant. */
  //Both -r and -c can be given at the same time, setting is exclusive
  setmode = !readmode && !cancelmode;

  if (setmode)
    {
      if (optind >= argc)
        {badarg = true;}
    }
  else if (optind < argc)
  if (badarg)
    {
      show_usage(argv[0]);
      return EXIT_FAILURE;
    }

  if (setmode)
    {
      //Get the number of seconds until the alarm expiration
      seconds = strtoul(argv[optind], NULL, 10);

      //Make sure valid time selected
      if (seconds < 1)
        {
          fprintf(stderr, "ERROR: Invalid number of seconds: %lu\n", seconds);
          return EXIT_FAILURE;
        }
    }

//Open the RTC driver
  printf("Opening %s\n", CONFIG_EXAMPLES_ALARM_DEVPATH);
  fd = open(CONFIG_EXAMPLES_ALARM_DEVPATH, O_WRONLY);
  if (fd < 0)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: Failed to open %s: %d\n",
              CONFIG_EXAMPLES_ALARM_DEVPATH, errcode);
      return EXIT_FAILURE;
    }

  if (readmode)
    {
      struct rtc_rdalarm_s rd = {0};

      long timeleft;
      time_t now;

      rd.id = alarmid;
      time(&now);

      ret = ioctl(fd, RTC_RD_ALARM, (unsigned long)((uintptr_t)&rd));
      if (ret < 0)
        {
          int errcode = errno;
          fprintf(stderr, "ERROR: RTC_RD_ALARM ioctl failed: %d\n", errcode);
          close(fd);
          return EXIT_FAILURE;
        }
  /* Some of the NuttX RTC implementations do not support alarms
       * longer than one month. There RTC_RD_ALARM can return partial
       * calendar values without month and year fields.
       * TODO: fix this in lower layers?
       */

      if (rd.time.tm_year > 0)
        {
          /* Normal sane case, assuming we did not actually request an
           * alarm expiring in year 1900.
           */

          timeleft = mktime((struct tm *)&rd.time) - now;
        }
      else
        {
          struct tm now_tm;

          /* Periodic extend "partial" alarms by "unfolding" months,
           * until we get alarm that is in future. Note that mktime()
           * normalizes fields that are out of their valid values,
           * so we don't have to handle carry to tm_year by ourselves.
           */

          gmtime_r(&now, &now_tm);
          rd.time.tm_mon = now_tm.tm_mon;
          rd.time.tm_year = now_tm.tm_year;

          do
          {
            timeleft = mktime((struct tm *)&rd.time) - now;
            if (timeleft < 0)
              {

rd.time.tm_mon++;
              }
          }
          while (timeleft < 0);
        }

      printf("Alarm %d is %s with %ld seconds to expiration\n", alarmid,
             rd.active ? "active" : "inactive", timeleft);
    }

  if (cancelmode)
    {
      ret = ioctl(fd, RTC_CANCEL_ALARM, (unsigned long)alarmid);
      if (ret < 0)
        {
          int errcode = errno;

          fprintf(stderr, "ERROR: RTC_CANCEL_ALARM ioctl failed: %d\n",
                  errcode);

          close(fd);
          return EXIT_FAILURE;
        }

      printf("Alarm %d has been canceled\n", alarmid);
    }

if (setmode)
    {
      struct rtc_setrelative_s setrel;

      /* Set the alarm */

      setrel.id      = alarmid;
      setrel.pid     = g_alarm_daemon_pid;
      setrel.reltime = (time_t)seconds;

      setrel.event.sigev_notify = SIGEV_SIGNAL;
      setrel.event.sigev_signo  = CONFIG_EXAMPLES_ALARM_SIGNO;
      setrel.event.sigev_value.sival_int = alarmid;

      ret = ioctl(fd, RTC_SET_RELATIVE, (unsigned long)((uintptr_t)&setrel));
      if (ret < 0)
        {
          int errcode = errno;

          fprintf(stderr, "ERROR: RTC_SET_RELATIVE ioctl failed: %d\n",
                  errcode);

          close(fd);
          return EXIT_FAILURE;
        }
      printf("Alarm %d set in %lu seconds\n", alarmid, seconds);
    }

  close(fd);
  return EXIT_SUCCESS;
}
```
