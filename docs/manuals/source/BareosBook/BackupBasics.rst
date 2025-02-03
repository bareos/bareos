
Backup Basics
=============

In this chapter I would like to look at the basics of backups. This should
initially be done independently of Bareos to familiarize yourself with the
topic.

Let us start with the basic question: What is backup?

  *Backup is the regular copying of computer data taken and stored elsewhere so
  that it may be used to restore the original after a data loss event.*


That sounds very simple at first glance. Let us have a look at the different parts of the definition.
The following mindmap shows the parts of the definition:

.. uml:: BackupBasics0.puml


.. Additionally, a backup should use the required resources intelligently to
.. minimize the resource consumption regarding storage, cpu, memory and network
.. consumption. This is possible by taking advantage of the properties of the data
.. to be backed up.

.. Usually, only a small percentage of the data that needs to be backed up changes
.. in every time period.

regular
  The most important thing about backup is that it is carried out regularly.
  Anyone who has ever lost data could have prevented this loss very easily by
  simply copying the data. Since humans are usually pretty bad at performing
  daily backups, it is strongly recommended to perform backups automatically
  and to monitor that the backups were carried out without errors.

copy of computer data 
  All forms of computer data are important and represent a value that must be
  protected. It does not matter if the data exists of in form of regular files,
  file access rights, filesystem blocks or any other form.

store elsewhere 
  The location where the backup is stored is of great importance, as this has a
  direct impact on the quality of the backup. This includes both the physical
  location and the technology on which the backup data is stored. Different storage
  locations and technologies have different characteristics and can therefore
  offer different levels of protection against the various threats to which the
  data is exposed.

restore the original
  All efforts to back up the data are in vain if the data cannot be restored if
  necessary. Making sure that the data is restored exactly as it was copied can
  be ensured with calculating and veryfing checksums both during backup and restore.

data loss
  Data loss means no longer being able to access your primary data. Data
  availability is the goal of a successful backup strategy. 
 
.. note::  Data loss in the sense of unauthorized data access is not meant here. Nevertheless, data security in this sense must also be taken into account during backup.



Let's take a closer look at the part of the definition below.

.. uml:: BackupBasics1.puml

Automatic and regular backups
-----------------------------

Regarding regular backups, the following items need to be determined:
  * At what time of day should the backup be carried out?
  * At what intervals should the backup be carried out?
Usually, backups are carried out once a day (usually in the night). This ensures
that daily production operations are not affected by the backup.
The interval also determines what is the maximum age of a new datum before it is protected
by a backup.


Copying the data
---------------

Computer **data that needs to be backed up** exists in a lot of different ways today, including:
  * Files
  * Metadata (like access rights, ownership ...)
  * Databases
  * Virtual Machines
  * Other

To determine which data needs to be backed up (and which not) is very important.
Often also there exist multiple ways to back up certain data.

Store the backups in a safe place
---------------------------------

The **location** where backed up data is stored is very important:
The closer the backup data is to the original data, the faster usually backup
and restore operations are. At the same time, the probability that the backup
data will be lost at the same time as the original data also is higher the
closer the backup data and the original data are.

Restore the original data
-------------------------
Restoring the original data is of course the main goal of all the backup
operation. Being able to restore the original data is only possible if the data
integrity is guaranteed. This is usually done by securing the data with
checksums. As the recovery operation is crucial but usually relatively seldom
required, it is good practice to regularly test the recovery operation.
Depending on the recovery objective, the restore speed is also a very important
parameter that needs to be taken into account.


Copying over all original data again and again will quickly require a multiple
of the original space as backup space. Also, copying over all data every time
will result in a very high load on all involved components like CPU, Memory,
Network and Disk I/O.


Data loss
---------





Data backup in modern environments has many dimensions. To successfully operate
a backup system, these must be taken into account and integrated into a backup
concept.

The first thing that needs to be determined is what is to be backed up.
What kind of data is it? 
Which files need to be backed up? 
Which files do not need to be backed up?
Not backing up data that does not need to be backed up is very sensible and saves valuable resources.

Next we determine how often we need to run the backups.
The interval depends on the requirements on the one hand, but also on the technical possibilities on the other.
A common interval is a nightly backup, as this is often a good compromise between data availability and resource consumption.

Every storage technology has its own properties which influence the overall system performance and cost.
The simplest solution usually is storing the backups on disk.
Depending on the requirements on which data loss event should be recoverable, also the storage technology
decision is a different one.
Depending on the requirements, also different technologies can be combined so fullfill the requirements.

How does data loss happen? What are the main reasons for data loss?
We have three main reasons for data loss: 

Accidental data loss: Unintentional destruction of data.
Hardware failure: The hardware that is used to store data is malfunctioning and can partly or completely stop working.
Software failure: A software error can destroy data so that it cannot be read anymore.
Human failure: Data might be deleted or destroyed unintentionally by an user or administrator.

Attack: Intentional destruction of data:
Hacking or insider activity can intentionally destroy data.
Malicious software like ransomware or a virus destroy data.

Higher violence:
Fire, flooding and power outage






All of the things presented here must be taken into account for a successful
backup solution. If you include these things in your backup planning, you can
set up a successful backup scheme.

How to create a backup scheme:

* What types of data loss should be recoverable?
  * Accident?
  * Attack?
  * Higher Violence?

* How much time do I have to recover all my data?

* What is the time that my data can stay unprotected?

* How long do I want to be able to go back in time?

* How detailed I want to be able to go back in time?

* Which data needs to be backed up?

* How much data needs to be backed up?

