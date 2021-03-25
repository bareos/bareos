/* check-sources:disable-copyright-check */
/* TODO: copyright */

#ifndef BAREOS_DROPLET_UTESTS_TOYCTL_H_
#define BAREOS_DROPLET_UTESTS_TOYCTL_H_

#include "toyserver.h"

extern int toyserver_start(const char* name, struct server_state** statep);
extern void toyserver_stop(struct server_state* state);
extern const char* toyserver_addrlist(struct server_state* state);

#endif  // BAREOS_DROPLET_UTESTS_TOYCTL_H_
