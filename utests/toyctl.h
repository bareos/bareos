/* TODO: copyright */

#ifndef __TOYCTL_H__
#define __TOYCTL_H__ 1

#include "toyserver.h"

extern int toyserver_start(const char *name, struct server_state **statep);
extern void toyserver_stop(struct server_state *state);
extern const char *toyserver_addrlist(struct server_state *state);

#endif /* __TOYCTL_H__ */
