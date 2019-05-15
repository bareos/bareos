.. code-block:: shell-session
   :caption: Access the local PostgreSQL database
    
   root@host:~# cd /etc/bareos
   root@host:/etc/bareos# ls -1
   bareos-fd.d
   bconsole.conf
   tray-monitor.d
   root@host:/etc/bareos# cd
   root@host:~# ssh root@bareos-dir.example.com
   root@bareos-dir:~# su - postgres
   postgres@bareos-dir:~> psql bareos
   Welcome to psql 8.3.23, the PostgreSQL interactive terminal.
   ...
