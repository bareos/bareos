.. _SecurityChapter:

Bareos Security Issues
======================

:index:`\ <single: Security>`\

-  Security means being able to restore your files, so read the :ref:`Critical Items Chapter <Critical>` of this manual.

-  The clients (bareos-fd) must run as root to be able to access all the system files.

-  It is not necessary to run the Director as root.

-  It is not necessary to run the Storage daemon as root, but you must ensure that it can open the tape drives, which are often restricted to root access by default. In addition, if you do not run the Storage daemon as root, it will not be able to automatically set your tape drive parameters on most OSes since these functions, unfortunately require root access.

-  You should restrict access to the Bareos configuration files, so that the passwords are not world-readable. The Bareos daemons are password protected using CRAM-MD5 (i.e. the password is not sent across the network). This will ensure that not everyone can access the daemons. It is a reasonably good protection, but can be cracked by experts.

-  If you are using the recommended ports 9101, 9102, and 9103, you will probably want to protect these ports from external access using a firewall.

-  You should ensure that the Bareos working directories are readable and writable only by the Bareos daemons.

-  Don’t forget that Bareos is a network program, so anyone anywhere on the network with the console program and the Director’s password can access Bareos and the backed up data.

-  You can restrict what IP addresses Bareos will bind to by using the appropriate Address records in the respective daemon configuration files.

-  The new systemd service uses the systemd default service type 'Simple', which will log startup errors to systemd-journal. This is particularly useful to debug starting errors. However this could also leak some sensitive information to the journal. Though the access to the systemd journal is sensitive and as such per default restricted, you might want to verify that your installation is strict enough.


.. _section-SecureEraseCommand:

Secure Erase Command
--------------------

From https://en.wikipedia.org/w/index.php?title=Data_erasure&oldid=675388437:

   Strict industry standards and government regulations are in place that force organizations to mitigate the risk of unauthorized exposure of confidential corporate and government data. Regulations in the United States include HIPAA (Health Insurance Portability and Accountability Act); FACTA (The Fair and Accurate Credit Transactions Act of 2003); GLB (Gramm-Leach Bliley); Sarbanes-Oxley Act (SOx); and Payment Card Industry Data Security Standards (PCI DSS) and the Data Protection Act in the
   United Kingdom. Failure to comply can result in fines and damage to company reputation, as well as civil and criminal liability.

Bareos supports the secure erase of files that usually are simply deleted. Bareos uses an external command to do the secure erase itself.

This makes it easy to choose a tool that meets the secure erase requirements.

To configure this functionality, a new configuration directive with the name :strong:`Secure Erase Command`\  has been introduced.

This directive is optional and can be configured in:

-

   :config:option:`dir/director/SecureEraseCommand`\

-

   :config:option:`sd/storage/SecureEraseCommand`\

-

   :config:option:`fd/client/SecureEraseCommand`\

This directive configures the secure erase command globally for the daemon it was configured in.

If set, the secure erase command is used to delete files instead of the normal delete routine.

If files are securely erased during a job, the secure delete command output will be shown in the job log.

.. code-block:: bareoslog
   :caption: bareos.log

   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_fd_consts.py"
   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_sd_consts.py"
   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bpipe-fd.dll"

The current status of the secure erase command is also shown in the output of status director, status client and status storage.

If the secure erase command is configured, the current value is printed.

Example:

.. code-block:: bconsole

   * <input>status dir</input>
   backup1.example.com-dir Version: 15.3.0 (24 August 2015) x86_64-suse-linux-gnu suse openSUSE 13.2 (Harlequin) (x86_64)
   Daemon started 08-Sep-15 12:50. Jobs: run=0, running=0 mode=0 db=postgresql
    Heap: heap=290,816 smbytes=89,166 max_bytes=89,166 bufs=334 max_bufs=335
    secure erase command='/usr/bin/wipe -V'

Example for Secure Erase Command Settings:

Linux:
   :strong:`Secure Erase Command = "/usr/bin/wipe -V"`\

Windows:
   :strong:`Secure Erase Command = "C:/cygwin64/bin/shred.exe"`\

Our tests with the :command:`sdelete` command was not successful, as :command:`sdelete` seems to stay active in the background.


.. _section-FIPS:

FIPS Mode
---------

The acronym :strong:`FIPS` stands for **Federal Information Processing Standards** and defines among others, security requirements for cryptography modules.

Some `Enterprise grade` distributions like RHEL or SLES can be run in FIPS mode, which then enforces the standards defined by `FIPS`.

To run Bareos on an OS that is running in `FIPS` mode, some adjustment need to be made so that Bareos only uses algorithms and protocols that are available in the `FIPS` mode.


RedHat RHEL 8
^^^^^^^^^^^^^

For RedHat RHEL 8 follow the editor instructions located at:
https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/8/html/security_hardening/switching-rhel-to-fips-mode_security-hardening

For RedHat RHEL 9 follow the editor instructions located at:
https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/9/html/security_hardening/switching-rhel-to-fips-mode_security-hardening#proc_installing-the-system-with-fips-mode-enabled_assembly_installing-the-system-in-fips-mode

The above test procedure should work out of the box, and Bareos daemons should select only FIPS approved protocols.


SUSE Linux Enterprise 15
^^^^^^^^^^^^^^^^^^^^^^^^

To install and activate FIPS mode under SLES you have to follow the documentation located at
https://documentation.suse.com/sles/15-SP4/html/SLES-all/cha-security-fips.html

At writing time, by default we will have to add some more configurations.
A good idea is to check with the test procedure below before doing manual changes, maybe
the default configuration has been fixed since then.

If the test procedure result in a FAILED connection you will have to do the following operations.

Create a file `/etc/ssl/openssl.fips` with this content:

.. code-block:: ini
   :caption: openssl.fips config file

   # /etc/ssl/openssl.fips
   # Coming for RHEL 8 fips enabled mode
   openssl_conf = default_modules

   [ default_modules ]
   ssl_conf = ssl_module

   [ ssl_module ]
   system_default = crypto_policy

   [ crypto_policy ]
   CipherString = @SECLEVEL=2:kEECDH:kEDH:kPSK:kDHEPSK:kECDHEPSK:-kRSA:-aDSS:-CHACHA20-POLY1305:-3DES:!DES:!RC4:!RC2:!IDEA:-SEED:!eNULL:!aNULL:!MD5:-SHA384:-CAMELLIA:-ARIA:-AESCCM8
   Ciphersuites = TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_SHA256
   MinProtocol = TLSv1.2
   MaxProtocol = TLSv1.3
   SignatureAlgorithms = ECDSA+SHA256:ECDSA+SHA384:ECDSA+SHA512:rsa_pss_pss_sha256:rsa_pss_rsae_sha256:rsa_pss_pss_sha384:rsa_pss_rsae_sha384:rsa_pss_pss_sha512:rsa_pss_rsae_sha512:RSA+SHA256:RSA+SHA384:RSA+SHA512:ECDSA+SHA224:RSA+SHA224

Then edit the `/etc/ssl/openssl.conf` and use the instruction on top to include the previous prepared openssl.fips file.

.. code-block:: ini
   :caption: openssl.conf config file

   .include /etc/ssl/openssl.fips

Once done, openSUSE/SLE system binaries using OpenSSL will by default used only FIPS validated mechanism.


FIPS Test procedure
^^^^^^^^^^^^^^^^^^^

An enabled `FIPS` computer can be checked with the following procedure

1. Ensure fips_enabled is on

.. code-block:: shell-session
   :caption: Check FIPS enabled

   sysctl -a | grep fips


**crypto.fips_enabled = 0** shows that fips is not running. If **crypto.fips_enabled = 1**, then fips is running.
to enable it refer to OS documentation (mostly adding fips=1 on boot line)


2. Run openssl server part

.. code-block:: shell-session
   :caption: run openssl server fips

   OPENSSL_FORCE_FIPS_MODE=1 openssl s_server -tls1_3 -nocert -psk 1234567890


3. Run openssl client part

.. code-block:: shell-session
   :caption: run openssl client fips

   OPENSSL_FORCE_FIPS_MODE=1 openssl s_client -tls1_3 -psk 1234567890


4. Expected result, connection is established with a FIPS-140 2 compatible mechanism.

.. code-block:: shell-session
   :caption: validated FIPS session

   server
   Using default temp DH parameters
   ACCEPT
   -----BEGIN SSL SESSION PARAMETERS-----
   MHICAQECAgMEBAITAQQgE1lsBGi7miykHRTXHy8vDoXUX0MgjtawEn1KSTk7bwoE
   IJLA8nOUxpX1M1wliy9H27NOVT/WXEG6wfY2FmKWdOeeoQYCBGFe6tiiBAICATCk
   BgQEAQAAAKUDAgEBrgYCBGAN6gQ=
   -----END SSL SESSION PARAMETERS-----
   Shared ciphers:TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_SHA256
   Supported Elliptic Groups: P-256:P-521:P-384
   Shared Elliptic groups: P-256:P-521:P-384
   CIPHER is TLS_AES_128_GCM_SHA256
   Reused session-id
   Secure Renegotiation IS supported
   ERROR
   shutting down SSL
   CONNECTION CLOSED


   client
   CONNECTED(00000003)
   ---
   no peer certificate available
   ---
   No client certificate CA names sent
   Server Temp Key: ECDH, P-256, 256 bits
   ---
   SSL handshake has read 258 bytes and written 384 bytes
   Verification: OK
   ---
   Reused, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256
   Secure Renegotiation IS NOT supported
   Compression: NONE
   Expansion: NONE
   No ALPN negotiated
   Early data was not sent
   Verify return code: 0 (ok)
   ---
   ---
   Post-Handshake New Session Ticket arrived:
   SSL-Session:
      Protocol  : TLSv1.3
      Cipher    : TLS_AES_128_GCM_SHA256
      Session-ID: 0A2B486C5B7D9DC18546161DE8A8DE2457A260C2038B15EA18C826B0F6186B9A
      Session-ID-ctx:
      Resumption PSK: 92C0F27394C695F5335C258B2F47DBB34E553FD65C41BAC1F63616629674E79E
      PSK identity: None
      PSK identity hint: None
      SRP username: None
      TLS session ticket lifetime hint: 304 (seconds)
      TLS session ticket:
      0000 - 19 af 09 90 78 d2 23 5d-41 b9 60 b8 b5 3a 20 e4   ....x.#]A.`..: .
      0010 - f9 d1 e5 84 ca e0 71 7f-31 b2 c9 78 ae ff de a0   ......q.1..x....
      0020 - 99 45 59 bf 8f bc 8d 65-25 42 7c 0b 37 6c 87 f5   .EY....e%B|.7l..
      0030 - f3 6e c7 6d 72 60 1e 69-b1 80 61 78 57 51 95 45   .n.mr`.i..axWQ.E
      0040 - 89 2b b9 c6 cc 3d 1b bd-bf af cb 3c ab f1 4b 70   .+...=.....<..Kp
      0050 - 6e 4c e2 6c 12 fc 4d 95-a9 24 7e 66 9e 4e 39 1a   nL.l..M..$~f.N9.
      0060 - 7e 22 76 d5 c1 24 c9 24-7d b7 35 52 13 66 28 73   ~"v..$.$}.5R.f(s
      0070 - b3 72 68 e8 7a 91 a9 7f-9b 75 fb e3 5b 54 9d 06   .rh.z....u..[T..
      0080 - 79 de 6e 2e 35 79 dd 20-ed ab cf f0 0a da 11 a1   y.n.5y. ........
      0090 - 41 a0 50 28 63 c2 cc 4e-21 68 35 f3 80 ec 6f 94   A.P(c..N!h5...o.
      00a0 - 65 98 6d cc 8c 1f 16 a4-5b b0 ae 98 f2 8e f8 91   e.m.....[.......
      00b0 - d4 a0 3a e3 c5 fe 56 cf-40 b8 b2 42 3c 8e fe 98   ..:...V.@..B<...

      Start Time: 1633610456
      Timeout   : 304 (sec)
      Verify return code: 1 (unspecified certificate verification error)
      Extended master secret: no
      Max Early Data: 0
   ---
   read R BLOCK

Fileset Signature Algorithm
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The default signature algorithm to verify the integrity of the files is `MD5`.
As all MD5 related function are disabled in `FIPS` mode, Bareos emits errors
like the following when trying to calculate MD5 signatures on a FIPS system:

.. code-block:: none

   Warning: MD5 digest digest initialization failed
   Error: OpenSSL digest initialization failed: ERR=error:060800C8:digital envelope routines:EVP_DigestInit_ex:disabled for FIPS

To solve this problem, the **Signature** option in your fileset to be changed to something stronger than MD5 or SHA1, for example **SHA256**:

.. code-block:: bareosconfig

   FileSet {
      Name = "File"
      Description = "Backup FIPS MODE"
      Include {
         Options {
            Signature = "SHA256"
   ...

With these modifications, Bareos can be run on a FIPS enabled Operating System.
