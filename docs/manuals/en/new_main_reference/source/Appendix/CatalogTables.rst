Catalog Tables
==============

:index:`[TAG=Catalog] <single: Catalog>`

Bareos stores its information in a database, named Catalog. It is configured by :ref:`DirectorResourceCatalog`.

Job
---

:index:`[TAG=Catalog->Job] <pair: Catalog; Job>` :index:`[TAG=Job->Catalog] <pair: Job; Catalog>`

JobStatus
~~~~~~~~~

:index:`[TAG=Job->JobStatus] <pair: Job; JobStatus>` :index:`[TAG=Catalog->Job->JobStatus] <triple: Catalog; Job; JobStatus>`

The status of a Bareos job is stored as abbreviation in the Catalog database table Job. It is also displayed by some bconsole commands, eg. :strong:`list jobs`.

This table lists the abbreviations together with its description and weight. The weight is used, when multiple states are applicable for a job. In this case, only the status with the highest weight/priority is applied.

========= ====================================================== ==========
**Abbr.** :strong:`:strong:`Description``  **Weight**
========= ====================================================== ==========
C         Created, not yet running                               15
R         Running                                                15
B         Blocked                                                15
T         Completed successfully                                 10
E         Terminated with errors                                 25
e         Non-fatal error                                        20
f         Fatal error                                            100
D         Verify found differences                               15
A         Canceled by user                                       90
I         Incomplete job                                         15
L         Committing data                                        15
W         Terminated with warnings                               20
l         Doing data despooling                                  15
q         Queued waiting for device                              15
F         Waiting for Client                                     15
S         Waiting for Storage daemon                             15
m         Waiting for new media                                  15
M         Waiting for media mount                                15
s         Waiting for storage resource                           15
j         Waiting for job resource                               15
c         Waiting for client resource                            15
d         Waiting on maximum jobs                                15
t         Waiting on start time                                  15
p         Waiting on higher priority jobs                        15
i         Doing batch insert file records                        15
a         SD despooling attributes                               15
========= ====================================================== ==========




