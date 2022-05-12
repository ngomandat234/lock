#ifdef _SNTP_LOCAL_
#define _SNTP_LOCAL_

#include "DisplayUI.h"


extern char *TAG_SNTP;
extern char strftime_buf[64];
extern Time _now;

extern void obtain_time(void);
extern void initialize_sntp(void);
extern void sntp_sync_time(struct timeval *tv);
extern void time_sync_notification_cb(struct timeval *tv);
extern void SplitData(void);
extern void task_SNTP(void);
#endif