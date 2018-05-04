#ifndef AUTHENTICATE_H_INCLUDED
#define AUTHENTICATE_H_INCLUDED

class MonitorResource;
class MonitorItem;
class JobControlRecord;

bool AuthenticateWithDaemon(MonitorItem *item, JobControlRecord *jcr);

#endif // AUTHENTICATE_H_INCLUDED
