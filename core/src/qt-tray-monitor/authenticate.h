#ifndef AUTHENTICATE_H_INCLUDED
#define AUTHENTICATE_H_INCLUDED

class MONITORRES;
class MonitorItem;
class JCR;

bool authenticate_with_daemon(MonitorItem *item, JCR *jcr);

#endif // AUTHENTICATE_H_INCLUDED
