.. code-block:: bconsole
   :caption: Example: adding a client and a job resource during runtime

    *<input>configure add client name=client2-fd address=192.168.0.2 password=secret</input>
    Created resource config file "/etc/bareos/bareos-dir.d/client/client2-fd.conf":
    Client {
      Name = client2-fd
      Address = 192.168.0.2
      Password = <strong>secret</strong>
    }
    *<input>configure add job name=client2-job client=client2-fd jobdefs=DefaultJob</input>
    Created resource config file "/etc/bareos/bareos-dir.d/job/client2-job.conf":
    Job {
      Name = client2-job
      Client = client2-fd
      JobDefs = DefaultJob
    }
