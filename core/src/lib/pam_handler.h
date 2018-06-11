#ifndef BAREOS_PAM_H
#define BAREOS_PAM_H

#include <string>

class BareosSocket;
bool pam_authenticate_useragent(BareosSocket *bs, std::string username);

#endif //BAREOS_PAM_H
