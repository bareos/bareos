#ifndef AUTHENTICATE_H_INCLUDED
#define AUTHENTICATE_H_INCLUDED

class MonitorResource;
class MonitorItem;
class JobControlRecord;

bool authenticate_with_daemon(MonitorItem *item, JobControlRecord *jcr);

#endif // AUTHENTICATE_H_INCLUDED
