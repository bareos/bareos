#
# Bacula Python interface script for the Director
#

# You must import both sys and bacula
import sys, bacula

# This is the list of Bacula daemon events that you
#  can receive.
class BaculaEvents(object):
  def __init__(self):
     # Called here when a new Bacula Events class is
     #  is created. Normally not used 
     noop = 1

  def JobStart(self, job):
     """
       Called here when a new job is started. If you want
       to do anything with the Job, you must register
       events you want to receive.
     """
     events = JobEvents()         # create instance of Job class
     events.job = job             # save Bacula's job pointer
     job.set_events(events)       # register events desired
     sys.stderr = events          # send error output to Bacula
     sys.stdout = events          # send stdout to Bacula
     jobid = job.JobId; client = job.Client
     numvols = job.NumVols 
     job.JobReport="Python Dir JobStart: JobId=%d Client=%s NumVols=%d\n" % (jobid,client,numvols) 

  # Bacula Job is going to terminate
  def JobEnd(self, job):    
     jobid = job.JobId
     client = job.Client 
     job.JobReport="Python Dir JobEnd output: JobId=%d Status=%s Client=%s.\n" % (jobid, job.JobStatus, client) 

  # Called here when the Bacula daemon is going to exit
  def Exit(self, job):
      print "Daemon exiting."
     
bacula.set_events(BaculaEvents()) # register daemon events desired

"""
  There are the Job events that you can receive.
"""
class JobEvents(object):
  def __init__(self):
     # Called here when you instantiate the Job. Not
     # normally used
     noop = 1

  def JobInit(self, job):
     noop = 1
     if (job.JobId < 2):
        startid = job.run("run kernsave")
        job.JobReport = "Python started new Job: jobid=%d\n" % startid
     print "name=%s version=%s conf=%s working=%s" % (bacula.Name, bacula.Version, bacula.ConfigFile, bacula.WorkingDir)

  def JobRun(self, job):
     noop = 1

  def NewVolume(self, job):
     jobid = job.JobId
     client = job.Client 
     numvol = job.NumVols;
     print job.CatalogRes
     job.JobReport = "JobId=%d Client=%s NumVols=%d" % (jobid, client, numvol)
     job.JobReport="Python before New Volume set for Job.\n"
     Vol = "TestA-%d" % numvol
     job.JobReport = "Exists=%d TestA-%d" % (job.DoesVolumeExist(Vol), numvol)
     job.VolumeName="TestA-%d" % numvol 
     job.JobReport="Python after New Volume set for Job.\n"  
     return 1

  def VolumePurged(self, job):
     noop = 1

  # Pass output back to Bacula
  def write(self, text):
     self.job.write(text)

  # Open file to be backed up. file is the filename
  #  NOT YET IMPLEMENTED
  def open(self, file):
     print "Open %s called" % file
     self.fd = open('m.py', 'rb')
     jobid = self.job.JobId
     print "Open: JobId=%d" % jobid

  # Read file data into Bacula memory buffer (mem)
  #  return length read. 0 => EOF, -1 => error
  #  NOT YET IMPLEMENTED
  def read(self, mem):
     print "Read called\n"
     len = self.fd.readinto(mem)
     print "Read %s bytes into mem.\n" % len
     return len

  # Close file
  #  NOT YET IMPLEMENTED
  def close(self):
     self.fd.close()
