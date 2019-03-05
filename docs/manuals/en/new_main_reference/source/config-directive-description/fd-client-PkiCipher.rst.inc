See the :ref:`DataEncryption` chapter of this manual.

Depending on the openssl library version different ciphers are available. To choose the desired cipher, you can use the PKI Cipher option in the filedaemon configuration. Note that you have to set :config:option:`fd/client/Compatible`\  = no:



   .. literalinclude:: /include/config/FdClientPki.conf



The available options (and ciphers) are:

-  aes128

-  aes192

-  aes256

-  camellia128

-  camellia192

-  camellia256

-  aes128hmacsha1

-  aes256hmacsha1

-  blowfish

They depend on the version of the openssl library installed.

For decryption of encrypted data, the right decompression algorithm should be automatically chosen.

