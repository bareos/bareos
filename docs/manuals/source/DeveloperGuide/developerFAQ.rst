Developer FAQ
=============

How to run a job that automatically fails?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

I want to run a job that automatically fails to simulate failing jobs.

To do so, you can use the directive :config:option:`dir/job/ClientRunBeforeJob`.
This directive launches a script on the client-side (file daemon) by default before running the job.
set the script to be “/bin/false”

.. code-block:: bareosconfig
   :caption: Job with client run before job directive

   Job {
       Name = "backup-bareos-fd"
       JobDefs = "DefaultJob"
       Client = "bareos-fd"
       Client Run Before Job = /bin/false
   }

or, if you want a job to work properly but fail at the end (e.g: simulate a full backup that did not go well), you can do the following

.. code-block:: bareosconfig
   :caption: Job with failing runscript at the end

   Job {
     Name = "backup"
     JobDefs = DefaultJob
     RunScript {
       Command = "/bin/false"
       failjobonerror = yes
       RunsWhen = After
     }
    }



I have changes that affect tapes and autochangers, how can I test them?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to test tapes, we use :command:`mhvtl` to simulate the presence of tape and autochanger devices.

Documentation is available at https://www.mhvtl.com/

Some distributions, like openSUSE, offer mhvtl packages that you can download and install using your package manager.
Otherwise you need to clone the mhvtl repository from https://github.com/markh794/mhvtl.git.
Then you should compile and install the mhvtl kernel module

.. code-block:: shell-session

   $ git clone https://github.com/markh794/mhvtl.git
   $ cd mhvtl/kernel
   $ make
   $ sudo make install

After that you have to install the mhvtl user tools

.. code-block:: shell-session

   $ cd mhvtl
   $ make
   $ sudo make install


Now that mhvtl is installed, you can enable and start the mhvtl services

.. code-block:: shell-session

   $ sudo systemctl enable --now mhvtl-load-modules.service
   $ sudo systemctl enable --now mhvtl.target


In order to check if your installation was done correctly, you can run :command:`lsscsi` (install it if you don't have it) or :command:`ls /dev/tape/by-id` which should give you something similar to the following output

.. code-block:: shell-session

   $ lsscsi
   [1:0:0:0]    disk    ATA      SAMSUNG MZ7TY256 3L6Q  /dev/sda
   [2:0:0:0]    mediumx STK      L700             0107  /dev/sch1
   [2:0:1:0]    tape    IBM      ULT3580-TD8      0107  /dev/st1
   [2:0:2:0]    tape    IBM      ULT3580-TD8      0107  /dev/st2
   [2:0:3:0]    tape    IBM      ULT3580-TD8      0107  /dev/st0
   [2:0:4:0]    tape    IBM      ULT3580-TD8      0107  /dev/st6
   [2:0:8:0]    mediumx STK      L80              0107  /dev/sch0
   [2:0:9:0]    tape    STK      T10000B          0107  /dev/st7
   [2:0:10:0]   tape    STK      T10000B          0107  /dev/st4
   [2:0:11:0]   tape    STK      T10000B          0107  /dev/st3
   [2:0:12:0]   tape    STK      T10000B          0107  /dev/st5

   $ ls /dev/tape/by-id/
   scsi-350223344ab000100      scsi-350223344ab001000      scsi-XYZZY_A3
   scsi-350223344ab000100-nst  scsi-350223344ab001000-nst  scsi-XYZZY_A3-nst
   scsi-350223344ab000200      scsi-350223344ab001100      scsi-XYZZY_A4
   scsi-350223344ab000200-nst  scsi-350223344ab001100-nst  scsi-XYZZY_A4-nst
   scsi-350223344ab000300      scsi-SSTK_L700_XYZZY_A      scsi-XYZZY_B1
   scsi-350223344ab000300-nst  scsi-SSTK_L80_XYZZY_B       scsi-XYZZY_B1-nst
   scsi-350223344ab000400      scsi-XYZZY_A1               scsi-XYZZY_B2
   scsi-350223344ab000400-nst  scsi-XYZZY_A1-nst           scsi-XYZZY_B2-nst
   scsi-350223344ab000900      scsi-XYZZY_A2               scsi-XYZZY_B3
   scsi-350223344ab000900-nst  scsi-XYZZY_A2-nst           scsi-XYZZY_B3-nst

Now that the installation is done, you will have to let bareos know that you are building tape and autochanger tests by adding the needed devices as cmake parameters to your exisiting cmake configuration:

.. code-block:: shell-session

   -Dchanger-device=/dev/tape/by-id/scsi-SSTK_L700_XYZZY_A
   -Dtape-devices="/dev/tape/by-id/scsi-350223344ab000100-nst;/dev/tape/by-id/scsi-350223344ab000200-nst;/dev/tape/by-id/scsi-350223344ab000300-nst;/dev/tape/by-id/scsi-350223344ab000400-nst"

The selected devices are the ones used in our tests.

A machine reboot can solve most of issues like missing devices or other incoherences.

For **any update** of kernel and kernel headers, you will have to rebuild and reinstall mhvtl, otherwise mhvtl kernel module loading will fail, :command:`mhvtl` will not work properly and bareos will not build.

:command:`mhvtl` updates in certain rare cases can change tape names causing tests to fail. In that case, check :file:`/etc/mhvtl/device.conf` and modify the device names accordingly.
