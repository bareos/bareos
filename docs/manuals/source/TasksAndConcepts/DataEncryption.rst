.. _DataEncryption:

Data Encryption
===============

:index:`\ <single: Data Encryption>`\  :index:`\ <single: Encryption; Data>`\ 

Bareos permits file data encryption and signing within the File Daemon (or Client) prior to sending data to the Storage Daemon. Upon restoration, file signatures are validated and any mismatches are reported. At no time does the Director or the Storage Daemon have access to unencrypted file contents.



.. warning::

   These feature is only available, if Bareos is build against OpenSSL.

It is very important to specify what this implementation does NOT do:

-  The implementation does not encrypt file metadata such as file path names, permissions, ownership and extended attributes. However, Mac OS X resource forks are encrypted.

Encryption and signing are implemented using RSA private keys coupled with self-signed x509 public certificates. This is also sometimes known as PKI or Public Key Infrastructure.

Each File Daemon should be given its own unique private/public key pair. In addition to this key pair, any number of "Master Keys" may be specified – these are key pairs that may be used to decrypt any backups should the File Daemon key be lost. Only the Master Key’s public certificate should be made available to the File Daemon. Under no circumstances should the Master Private Key be shared or stored on the Client machine.

The Master Keys should be backed up to a secure location, such as a CD placed in a in a fire-proof safe or bank safety deposit box. The Master Keys should never be kept on the same machine as the Storage Daemon or Director if you are worried about an unauthorized party compromising either machine and accessing your encrypted backups.

While less critical than the Master Keys, File Daemon Keys are also a prime candidate for off-site backups; burn the key pair to a CD and send the CD home with the owner of the machine.



.. warning::

   If you lose your encryption keys, backups will be unrecoverable.
   :strong:`always` store a copy of your master keys in a secure, off-site location.

The basic algorithm used for each backup session (Job) is:

#. The File daemon generates a session key.

#. The FD encrypts that session key via PKE for all recipients (the file daemon, any master keys).

#. The FD uses that session key to perform symmetric encryption on the data.

Encryption Technical Details
----------------------------

:index:`\ <single: Encryption; Technical Details>`

The implementation uses 128bit AES-CBC, with RSA encrypted symmetric session keys. The RSA key is user supplied. If you are running OpenSSL >= 0.9.8, the signed file hash uses SHA-256, otherwise SHA-1 is used.

End-user configuration settings for the algorithms are not currently exposed, only the algorithms listed above are used. However, the data written to Volume supports arbitrary symmetric, asymmetric, and digest algorithms for future extensibility, and the back-end implementation currently supports:

::

   Symmetric Encryption:
       - 128, 192, and 256-bit AES-CBC
       - Blowfish-CBC

   Asymmetric Encryption (used to encrypt symmetric session keys):
       - RSA

   Digest Algorithms:
       - MD5
       - SHA1
       - SHA256
       - SHA512

The various algorithms are exposed via an entirely re-usable, OpenSSL-agnostic API (ie, it is possible to drop in a new encryption backend). The Volume format is DER-encoded ASN.1, modeled after the Cryptographic Message Syntax from RFC 3852. Unfortunately, using CMS directly was not possible, as at the time of coding a free software streaming DER decoder/encoder was not available.

Generating Private/Public Encryption Keys
-----------------------------------------

:index:`\ <single: Encryption; Generating Private/Public Encryption Keypairs>`\ 

Generate a Master Key Pair with:



::

     openssl genrsa -out master.key 2048
     openssl req -new -key master.key -x509 -out master.cert



Generate a File Daemon Key Pair for each FD:



::

     openssl genrsa -out example-fd.key 2048
     openssl req -new -key example-fd.key -x509 -out example-fd.cert
     cat example-fd.key example-fd.cert >example-fd.pem



Note, there seems to be a lot of confusion around the file extensions given to these keys. For example, a .pem file can contain all the following: private keys (RSA and DSA), public keys (RSA and DSA) and (x509) certificates. It is the default format for OpenSSL. It stores data Base64 encoded DER format, surrounded by ASCII headers, so is suitable for text mode transfers between systems. A .pem file may contain any number of keys either public or private. We use it in cases where there is both a
public and a private key.

Above we have used the .cert extension to refer to X509 certificate encoding that contains only a single public key.

Example Data Encryption Configurations (bareos-fd.conf)
-------------------------------------------------------

:index:`\ <single: Example; Data Encryption Configuration File>`\ 



   .. literalinclude:: /include/config/FdClientPki.conf
      :language: bareosconfig



Decrypting with a Master Key
----------------------------

:index:`\ <single: Decrypting with a Master Key>`\  :index:`\ <single: Encryption; Decrypting with a Master Key>`\ 

It is preferable to retain a secure, non-encrypted copy of the client’s own encryption keypair. However, should you lose the client’s keypair, recovery with the master keypair is possible.

You must:

-  Concatenate the master private and public key into a single keypair file, ie:

   ::

      cat master.key master.cert > master.keypair

-  Set the PKI Keypair statement in your bareos configuration file:

   ::

         PKI Keypair = master.keypair

-  Start the restore. The master keypair will be used to decrypt the file data.




