#ifndef AUTHENTICATE_H_INCLUDED
#define AUTHENTICATE_H_INCLUDED

class MONITORRES;
class MonitorItem;
class JCR;

int authenticate_daemon(MonitorItem* item, JCR *jcr);

#endif // AUTHENTICATE_H_INCLUDED
