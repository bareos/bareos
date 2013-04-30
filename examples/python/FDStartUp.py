#
# Bacula Python interface script for the File Daemon
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
     jobid = job.JobId
     client = job.Client
     job.JobReport="Python FD JobStart: JobId=%d Client=%s \n" % (jobid,client)
     return 1

  # Bacula Job is going to terminate
  def JobEnd(self, job):    
     jobid = job.JobId
     client = job.Client 
     job.JobReport="Python FD JobEnd output: JobId=%d Client=%s.\n" % (jobid, client)
     

  # Called here when the Bacula daemon is going to exit
  def Exit(self):
      noop = 1
     
bacula.set_events(BaculaEvents()) # register daemon events desired

"""
  There are the Job events that you can receive.
"""
class JobEvents(object):
  def __init__(self):
     # Called here when you instantiate the Job. Not
     # normally used
     noop = 1

  # Pass output back to Bacula
  def write(self, text):
     self.job.write(text)

  # Open file to be backed up. file is the filename
  def Python_open(self, file):
     print "Open %s called" % file
     self.fd = open(file, 'rb')
     jobid = self.job.JobId
     print "Open: %s" % file
 
  # Read file data into Bacula memory buffer (mem)
  #  return length read. 0 => EOF, -1 => error
  def Python_read(self, mem):
     print "Read called\n"
     len = self.fd.readinto(mem)
     print "Read %s bytes into mem.\n" % len
     return len

  # Close file
  def Python_close(self):
     self.fd.close()
