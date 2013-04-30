#
# Bacula Python interface script for the Storage Daemon
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
     job.JobReport="Python SD JobStart: JobId=%d Client=%s \n" % (jobid,client)
     return 1

  # Bacula Job is going to terminate
  def JobEnd(self, job):    
     jobid = job.JobId
     client = job.Client 
     job.JobReport="Python SD JobEnd output: JobId=%d Client=%s.\n" % (jobid, client)
#    print "Python SD JobEnd\n"  
     

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
