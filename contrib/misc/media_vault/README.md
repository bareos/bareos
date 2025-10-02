# Media Vault Management

This utility helps administrators to select and export tapes from an
autoloader for placement into an external vault location.

It is written in Python 3 (minimum version 3.6) and uses the 
``python_bareos`` module to interact with the Director.


## Manual Installation

Install the script into the bareos scripts directory (e.g `/usr/lib/bareos/scripts`

```bash
cp media_vault.py /usr/lib/bareos/scripts/
chmod 0755 /usr/lib/bareos/scripts/media_vault.py
chown root:root /usr/lib/bareos/scripts/media_vault.py
```

Install the helper into the same location `/usr/lib/bareos/scripts/`

```bash
cp media_vault.sh /usr/lib/bareos/scripts/
chmod 0755 /usr/lib/bareos/scripts/media_vault.sh
chown root:root /usr/lib/bareos/scripts/media_vault.sh
```

### Virtualenv

Prepare a Python 3 venv with ``python_bareos`` in it.
It is recommended run this as bareos user.

```bash
su bareos -l -s /bin/bash
python3 -m venv ~/.local/share/virtualenvs/bareos_media_vault
. ~/.local/share/virtualenvs/bareos_media_vault/bin/activate
python3 -m pip install python_bareos ConfigArgParse
```

.. note:
   TLS-PSK native support requires Python version >= 3.12. Alternatively,
   the ``sslpsk`` module can be used for python versions <= 3.9, but it requires gcc.
   gcc installed). It can also be installed from github repository
   to support Python version up to 3.10.

### Without Virtualenv

If python3-bareos and ConfigArgParse are available as native packages,
you can install them directly with your package manager as root.

.. note:
   ``configArgParse`` version >= 1.2 is required for ``media_vault.py``

```bash
dnf install python3-bareos python313-configargparse.noarch
```

## Configuration & installation

### bareos dir dedicated console and acl profile

To protect your director, it is recommended to create a restricted console,
dedicated to the tool, with restricted acls.

#### Example of console

`/etc/bareos/bareos-dir.d/console/media_vault.conf`

```conf
Console {
  Name = "media_vault"
  Password = "" 
  Description = "Restricted console used by media_vault.py script"
  Profile = "media_vault"

  # As python before version 3.12 does not support TLS-PSK without extension (ssl-psk)
  # and the director has TLS enabled by default, we need to either disable TLS or setup
  # TLS with certificates.
  #
  # If your python version is too old you might need to change to No
  TLS Enable = Yes
}
```

Update the password to the on you setup in the configuration ``media_vault.ini`` file.

#### Installation of the console

You can copy the given example

```bash
cp console_media_vault.conf.example /etc/bareos/bareos-dir.d/console/media_vault.conf
chmod 0640 /etc/bareos/bareos-dir.d/console/media_vault.conf
chown bareos:bareos /etc/bareos/bareos-dir.d/console/media_vault.conf
```

#### Example of profile for restricted console

`/etc/bareos/bareos-dir.d/profile/media_vault.conf`

```conf
Profile {
   Name = media_vault
   Description = "Profile allowing Bareos autoloader & tape operations."

   Command ACL = show, umount, unmount, mount, export, update, .api, .sql
   Command ACL = !*all*

   Catalog ACL = *all*
   Client ACL = *all*
   FileSet ACL = *all*
   Job ACL = *all*
   Plugin Options ACL = *all*
   Pool ACL = *all*
   Schedule ACL = *all*
   Storage ACL = *all*
   Where ACL = *all*
}
```

#### installation of dedicated profile for restricted console

```bash
cp profile_media_vault.conf.example /etc/bareos/bareos-dir.d/profile/media_vault.conf
chmod 0640 /etc/bareos/bareos-dir.d/profile/media_vault.conf
chown bareos:bareos /etc/bareos/bareos-dir.d/profile/media_vault.conf
```

### configuration file and parameters for run

The tool is highly configurable with a number of parameters described
in its associated configuration file ``media_vault.ini``

The tool will try to read first an existing configuration file in the default
location ``/etc/bareos/media_vault.ini``. It will then try to find a file in
the current path. 
if ``--config`` parameter is specified: only this file will be considered.
We highly recommend to use the ``--config`` parameter to specify a fully
qualified path for the configuration file. 

Remember to verify that the file is readable by running user ``bareos``.


Any parameter given at run time will overwrite the one defined in configuration file.
Run ```media_vault.py --help``` to see all parameters available.

#### Installation of a default configuration file

```bash
cp media_vault.ini.example.in /etc/bareos/media_vault.ini
chmod 640 /etc/bareos/media_vault.ini
chown root:bareos /etc/bareos/media_vault.ini
```

Then edit and adapt parameters to your needs.

### reload the bareos director

Once the different scripts and configuration are in place, you have
to reload the configuration in Bareos director.

```bash
bconsole <<< "reload"
```


## Run the script manually

To run the script execute the following

```bash
su bareos -l -s /bin/bash
source ~/.local/share/virtualenvs/bareos_media_vault/bin/activate && \
/usr/lib/bareos/scripts/media_vault.py --config /etc/bareos/media_vault.ini && \
deactivate
```

to get more information you may use the ``--debug`` parameter.

## Functionalities

### Build list for ejection

The script will prepare the candidate list of volumes ready for
ejection from the specified auto-changer.

That list of volume is build against a number of constraints:

- minimum size that the tape needs to have (for example 4TB)
- min_age and max_age define a range of days.
  only tapes that were last written to in this range of days are considered
  for vaulting. The current day is day 0.
  Default values max_age=21, min_age=1.
- pools in which volume are searched, as a comma separated list of
  pools names.
- presence in autochanger.
- volume status being 'Full' or 'Used' (default list) that can be
  extended to 'Append'.
- prepare and create a picking list for your vaulting provider.

The whole list has detailed informations described in the distributed
``.ini`` template file.

#### example query generated by ``media_vault``

```sql
select slot,storageid,volumename,volbytes,lastwritten,volretention,poolid
from media m
where
m.poolid in (select poolid from pool where name ~* 'Offsite')
and
m.storageid in (select storageid from storage where name ~* 'Tapes')
and
m.inchanger = 1
and
m.volbytes >= 419430400
and
m.volstatus in ('Full','Used')
and
m.lastwritten between '2023-10-30 06:20:02' and '2023-11-19 05:20:02'
;
```


### Manage the ejection

With the help of the previous candidate volumes list, the program will
prepare as much as available export slots, a list of volume to export.

The script will try to export/eject this list of volumes.

- If the export succeed the optional report file is prepared in its 
  dedicated location and specific format.
- If the export failed for any reason, none of the volume will
  be ejected.

If the list of volumes to be ejected exceed the number of available
export slots, the operator can run the script again as long as 
candidate are found by the script, of course after emptying manually
each time the export slots.

### Reporting

The script will print in the job log the list of candidates, and also
the list of exported volumes. An additional logging mechanism exists
and will trace the informal information in its own log related to the
``log_dir`` parameter.

A debug flag ``--debug`` can be used to display more detailed 
informations for debugging purpose.

## Run as admin job in bareos

Once all the previous described steps installation and configuration
are done. You will be able to run the job manually or adapt its 
configuration to match your needs.

The typical record for this admin job will be similar to

```log
list jobs

| 16878 | admin-media_vault | bareos-fd | 2024-07-31 15:34:07 | 00:00:00 | D | | 0 | 0 | T |
```

The ``backup_type`` is ``D``, it have to be manually purged from the database from time to time.

The job log with optional vault report activated will look like:

```log
*list joblog jobid=16880
 2024-07-31 16:33:08 bareos-dir JobId 16880: shell command: run BeforeJob "/var/lib/bareos/media_vault.sh"
 2024-07-31 16:33:08 bareos-dir JobId 16880: BeforeJob: UserWarning: Connection encryption via TLS-PSK is not available, as the module sslpsk is not installed.
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: Bareos consoles connected
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: 4 candidate(s) volume(s) found
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: #  VolumeName    Slot    StorageId       VolBytes            LastWritten        VolRetention   ReturnDate
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS000L8            11          19       2,345,066,496   2024-07-11 14:49:24         7776000   10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS001L8            12          19       2,345,066,496   2024-07-11 14:49:26         7776000   10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS002L8            13          19       2,345,066,496   2024-07-11 14:49:26         7776000   10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS003L8            14          19       3,126,733,824   2024-07-11 15:21:33         7776000   10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: Created ftp file: /var/tmp/media_vault/iron_mtn_ftp/123456_IronSimulationautochanger-0_20240731-163308_V.IM
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: StartHeaderText~D~123456~IronSimulation-autochanger-0~EndHeaderText
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS000L8    10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS001L8    10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS002L8    10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: DIS003L8    10092024
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: StartFooterText~D~123456~IronSimulation-autochanger-0-0~EndFooterText
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob: Volstatus updated to 'Archive'
 2024-07-31 16:33:09 bareos-dir JobId 16880: BeforeJob:
 2024-07-31 16:33:09 bareos-dir JobId 16880: Start Admin JobId 16880, Job=admin-media_vault.2024-07-31_16.33.06_26
 2024-07-31 16:33:09 bareos-dir JobId 16880: BAREOS 23.0.3 (04Jun24): 31-Jul-2024 16:33:09
  JobId:                  16880
  Job:                    admin-media_vault.2024-07-31_16.33.06_26
  Scheduled time:         31-Jul-2024 16:33:06
  Start time:             31-Jul-2024 16:33:09
  End time:               31-Jul-2024 16:33:09
  Bareos binary info:     Bareos subscription release
  Job triggered by:       User
  Termination:            Admin OK
```

## How can multiple autochangers be managed?

If you have a director managing multiple autochangers on the same or different bareos-sd.
You will have to create one configuration INI file per autochanger.

Then you can create as many jobs and wrapper shell scripts as needed. You can then run them
separately at a convenient time, or add a new call to the wrapper shell script with the desired
configuration.

For example:

```bash
su bareos -l -s /bin/bash
source ~/.local/share/virtualenvs/bareos_media_vault/bin/activate && \
/usr/lib/bareos/scripts/media_vault.py --config /etc/bareos/media_vault_autochanger-0.ini && \
/usr/lib/bareos/scripts/media_vault.py --config /etc/bareos/media_vault_autochanger-1.ini && \
deactivate
```

- Pros: only one job and one wrapper script to maintain.
- Cons: if one of the calls fails, the job will have a failed status.

