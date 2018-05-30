#ifndef BAREOS_PAM_H
#define BAREOS_PAM_H

#include <string>

bool pam_authenticate_useragent(std::string username, std::string password);

#endif //BAREOS_PAM_H
