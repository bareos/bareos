.. _PAMDeveloperChapter:

PAM
===
PAM is an authentication method provided by many operating systems to establish a standardized interface for the authorization of users.

The name of the service to be registered with the respective PAM module is "bareos". 

The following sequence diagram shows three options how a user can be authorized on a Bareos Director Daemon: 

* Option 1: No PAM authentication using named console
* Option 2: Interactive PAM authentication
* Option 3: Direct PAM authentication

In this example the complete connection and authorization sequence of a Bareos Console respective Bareos Webui is shown. 

A detailed description on the configuration see this chapter: :ref:`PAMConfigurationChapter`.

.. uml::
  :caption: Console/WebUI connection sequence from Bareos 18.2

  skinparam SequenceMessageAlign reversedirection

  actor "Console\nWebUI" as W
  participant "director\ndaemon" as D

  W <-> D: Initiate TCP connection
  W <-> D: TLS Cert/PSK Handshake
  note right of D: <b>default console</b>: identity *UserAgent*,\npassword/key from director resource\n\n<b>named console</b>: identity <console-name>,\npassword/key from console resource

  W -> D: "Hello <*UserAgent*|console-name> calling"

  W <- D: "auth cram-md5[c] <password-md5> ssl=<0|1|2|4>"
  W -> D: "<password-md5>"
  W <-- D: On Failure [Close TLS connection]
  W <- D: On Success: "1000 OK auth"

  W -> D: "auth cram-md5[c] <password-md5> ssl=<0|1|2|4>"
  W <- D: "<password-md5>"
  W --> D: On Failure [Close TLS connection]
  W -> D: On Success: "1000 OK auth"

  ... ...

  == Option 1: No PAM authentication (Default Console) ==
  ... no further action ...

  == Option 2: Interactive PAM authentication (Console) ==

  note right of D: pam can only be used when connected \nwith a named console (__not__ default console) \nusing EnablePamAuthentication= yes

  note left of W: (__RS__) is the Record Separator \n(ASCII-character 0x1e) 

  W <- D: "1001__RS__" (Pam Authentication required)
  W -> D: "4001__RS__" (Interactive Pam (i.e. pam_unix))
  W <- D: "0x2" (type = PAM_PROMPT_ECHO_ON)

  note left of W:  type as bcd: \n0x0 (PAM_SUCCESS)\n0x1 (PAM_PROMPT_ECHO_OFF) \n0x2 (PAM_PROMPT_ECHO_ON)

  W <- D: "Login:"
  W -> D: "<cleartext pam-username>"
  W <- D: "0x1" (type = PAM_PROMPT_ECHO_OFF)
  W <- D: "Password:"
  W -> D: "<cleartext pam-password>"
  W <- D: On Success: "0x0" (PAM_SUCCESS)
  W <- D: On Success: "0x0" (empty message)

  == Option 3: Direct PAM authentication (WebUI) ==
  W <- D: "1001__RS__" (Pam Authentication required)
  W -> D: "4002__RS__Username__RS__Password" (PAM credentials)
  ... ...

  == On any failure ==
  W <--> D: [Close TLS connection]
  W <--> D: Close TCP connection

  == On success == 
  W <- D: 1000__RS__OK:__RS__<director-name> Version: <version> (<date>)
  W <- D: 1002__RS__<You are logged in as: <username>|You are connected using the default console>

  ... run some console commands ...

  W <-> D: [Close TLS connection]
  W <-> D: Close TCP connection

