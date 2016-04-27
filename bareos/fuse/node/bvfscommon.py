"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.base import Base
import logging
import os
import stat

class BvfsCommon(Base):
    XattrKeyRestoreTrigger = "user.bareos.do"
    XattrKeyRestoreJobId = "user.bareos.restore_job_id"
    
    def init(self, root, jobid, path, filename, stat):
        self.jobid = jobid
        self.static = True
        self.restorepath = os.path.normpath("/%s/jobid=%s/" % (self.root.restorepath, self.jobid))
        if filename:
            self.restorepathfull = os.path.normpath("%s%s%s" % (self.restorepath, path, filename))
        else:
            self.restorepathfull = os.path.normpath("%s%s" % (self.restorepath, path))
        self.xattr = {
                'user.bareos.restorepath': self.restorepathfull,
                'user.bareos.restored': 'no',
            }
        if self.root.restoreclient:
            # restore is only possible, if a restoreclient is given
            self.xattr['user.bareos.do_options'] = 'restore'
            self.xattr['user.bareos.do'] = ''

        if stat:
            self.set_stat(stat)            

    def listxattr(self, path):
        # update xattrs
        if self.xattr['user.bareos.restored'] != 'yes':
            if os.access(self.restorepathfull, os.F_OK):
                self.xattr['user.bareos.restored'] = 'yes'
        return super(BvfsCommon, self).listxattr(path)

    # Helpers
    # =======

    def restore(self, path, pathIds, fileIds):
        self.logger.debug( "start" )
        bvfs_restore_id = "b20042"
        dirId=''
        if pathIds:
            dirId='dirid=' + ",".join(map(str, pathIds))
        fileId=''
        if fileIds:
            fileId='fileid=' + ",".join(map(str, fileIds))
        select = self.bsock.call(
            '.bvfs_restore jobid={jobid} {dirid} {fileid} path={bvfs_restore_id}'.format(
                jobid = self.jobid,
                dirid = dirId,
                fileid = fileId,
                bvfs_restore_id = bvfs_restore_id))
        restorejob=''
        if self.root.restorejob:
            restorejob="restorejob={restorejob} ".format(restorejob=self.root.restorejob)
        restore = self.bsock.call(
            'restore file=?{bvfs_restore_id} client={client} restoreclient={restoreclient} {restorejob} where="{where}" yes'.format(
                client = self.job.job['client'],
                restoreclient = self.root.restoreclient,
                restorejob = restorejob,
                where = self.restorepath,
                bvfs_restore_id = bvfs_restore_id))
        try:
            restorejobid=restore['run']['jobid']
            self.setxattr(path, self.XattrKeyRestoreJobId, str(restorejobid), 0)
        except KeyError:
            self.logger.debug("failed to get resulting jobid of run command (maybe old version of Bareos Director)")
        cleanup = self.bsock.call('.bvfs_cleanup path={bvfs_restore_id}'.format(bvfs_restore_id = bvfs_restore_id))
        self.logger.debug( "end" )
