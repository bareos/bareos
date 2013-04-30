#ifndef _SYSLOG_H
#define _SYSLOG_H

#define LOG_DAEMON  0
#define LOG_ERR     1
#define LOG_CRIT    2
#define LOG_ALERT   3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_LOCAL0  10
#define LOG_LOCAL1  11
#define LOG_LOCAL2  12
#define LOG_LOCAL3  13
#define LOG_LOCAL4  14
#define LOG_LOCAL5  15
#define LOG_LOCAL6  16
#define LOG_LOCAL7  17
#define LOG_LPR     20
#define LOG_MAIL    21
#define LOG_NEWS    22
#define LOG_UUCP    23
#define LOG_USER    24
#define LOG_CONS    0
#define LOG_PID     0


extern "C" void syslog(int type, const char *fmt, ...);
void openlog(const char *app, int, int);
void closelog(void);

#endif /* _SYSLOG_H */
