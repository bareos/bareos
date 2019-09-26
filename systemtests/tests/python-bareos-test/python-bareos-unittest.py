#!/usr/bin/env python

# -*- coding: utf-8 -*-

from __future__ import print_function
import json
import logging
import os
import unittest

import bareos.bsock
from bareos.bsock.constants import Constants
import bareos.exceptions


class PythonBareosBase(unittest.TestCase):
    director_address = 'localhost'
    director_port = 9101
    director_root_password = 'secret'
    director_operator_username = 'admin'
    director_operator_password = 'secret'
    client = 'bareos-fd'
    #restorefile = '/usr/sbin/bconsole'
    # path to store logging files
    backup_directory = 'tmp/data/'
    debug = False
    logpath = '{}/PythonBareosTest.log'.format(os.getcwd())

    def setUp(self):
        # Configure the logger, for information about the timings set it to INFO
        logging.basicConfig(
            filename=self.logpath,
            format='%(levelname)s %(module)s.%(funcName)s: %(message)s',
            level=logging.INFO)
        logger = logging.getLogger()
        if self.debug:
            logger.setLevel(logging.DEBUG)
        logger.debug('setUp')

    def tearDown(self):
        logger = logging.getLogger()
        logger.debug('tearDown\n\n\n')

    def get_name_of_test(self):
        return self.id().split('.', 1)[1]


class PythonBareosExceptionsTest(PythonBareosBase):
    def test_exception_connection_error(self):
        '''
        Test if the ConnectionError exception deliveres the expected result.
        '''
        logger = logging.getLogger()

        message = 'my test message'
        try:
            raise bareos.exceptions.ConnectionError('my test message')
        except bareos.exceptions.ConnectionError as e:
            logger.debug('signal: str={}, repr={}, args={}'.format(
                str(e), repr(e), e.args))
            self.assertEqual(message, str(e))

    def test_exception_signal_received(self):
        '''
        Test if the SignalReceivedException deliveres the expected result.
        '''
        logger = logging.getLogger()

        try:
            raise bareos.exceptions.SignalReceivedException(
                Constants.BNET_TERMINATE)
        except bareos.exceptions.SignalReceivedException as e:
            logger.debug('signal: str={}, repr={}, args={}'.format(
                str(e), repr(e), e.args))
            self.assertIn('terminate', str(e))


class PythonBareosPlainTest(PythonBareosBase):
    def test_login_as_root(self):
        logger = logging.getLogger()

        #logger.debug('root pw: {}'.format(self.director_root_password))
        bareos_password = bareos.bsock.Password(self.director_root_password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            password=bareos_password)
        whoami = director.call('whoami').decode('utf-8')
        self.assertEqual('root', whoami.rstrip())

    def test_login_as_user(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)
        whoami = director.call('whoami').decode('utf-8')
        self.assertEqual(username, whoami.rstrip())

    def test_login_with_not_existing_username(self):
        '''
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        '''
        logger = logging.getLogger()

        username = 'nonexistinguser'
        password = 'secret'

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=bareos_password)

    def test_login_with_not_wrong_password(self):
        '''
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        '''
        logger = logging.getLogger()

        username = self.director_operator_username
        password = 'WRONGPASSWORD'

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=bareos_password)

    def test_no_autodisplay_command(self):
        '''
        The console has no access to the "autodisplay" command.
        However, the initialization of DirectorConsole calls this command.
        However, the error should not be visible to the console.
        '''
        logger = logging.getLogger()

        username = u'noautodisplaycommand'
        password = u'secret'

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        # get the list of all command
        result = director.call('.help all')
        #logger.debug(str(result))

        # verify, the result contains command. We test for the list command.
        self.assertIn(u'list', str(result))
        # verify, the result does not contain the autodisplay command.
        self.assertNotIn(u'autodisplay', str(result))

        # check if the result of 'whoami' only contains the expected result.
        result = director.call('whoami').decode('utf-8')
        logger.debug(str(result))

        self.assertEqual(username, result.rstrip())

    def test_json_without_json_backend(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)
        result = director.call('.api json').decode('utf-8')
        result = director.call('whoami').decode('utf-8')
        logger.debug(str(result))
        content = json.loads(str(result))
        logger.debug(str(content))
        self.assertEqual(content['result']['whoami'].rstrip(), username)

    # TODO: test_login_tlspsk


#
# Test with JSON backend
#


class PythonBareosJsonBase(PythonBareosBase):

    #
    # Util
    #

    @staticmethod
    def check_resource(director, resourcesname, name):
        logger = logging.getLogger()
        result = False
        try:
            result = director.call(u'.{}'.format(resourcesname))
            for i in result[resourcesname]:
                if i['name'] == name:
                    result = True
        except Exception as e:
            logger.warn(str(e))
        return result

    @staticmethod
    def check_console(director, name):
        return PythonBareosJsonBase.check_resource(director, 'consoles', name)

    @staticmethod
    def check_jobname(director, name):
        return PythonBareosJsonBase.check_resource(director, 'jobs', name)

    def configure_add(self, director, resourcesname, resourcename, cmd):
        if not self.check_resource(director, resourcesname, resourcename):
            result = director.call('configure add {}'.format(cmd))
            self.assertEqual(result['configure']['add']['name'], resourcename)
            self.assertTrue(self.check_jobname(director, resourcename))

    def run_job(self, director, jobname, level, wait=False):
        logger = logging.getLogger()
        run_parameter = ['job={}'.format(jobname), 'yes']
        if level:
            run_parameter.append(u'level={}'.format(level))
        result = director.call('run {}'.format(u' '.join(run_parameter)))
        jobId = result['run']['jobid']
        if wait:
            result = director.call('wait jobid={}'.format(jobId))
            # "result": {
            #    "Job": {
            #    "jobid": 1,
            #    "jobstatuslong": "OK",
            #    "jobstatus": "T",
            #    "exitstatus": 0
            #    }
            # }
            self.assertEqual(result['Job']['jobstatuslong'], u'OK')

        return jobId

    def _test_job_result(self, jobs, jobid):
        logger = logging.getLogger()
        for job in jobs:
            if job['jobid'] == jobid:
                files = int(job['jobfiles'])
                logger.debug(u'Job {} contains {} files.'.format(jobid, files))
                self.assertTrue(files >= 1,
                                u'Job {} contains no files ({}).'.format(
                                    jobid, files))
                return True
        self.fail('Failed to find job {}'.format(jobid))
        # add return to prevent pylint warning
        return False

    def _test_no_volume_in_pool(self, console, password, pool):
        logger = logging.getLogger()
        bareos_password = bareos.bsock.Password(password)
        console_poolbotfull = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=console,
            password=bareos_password)

        result = console_poolbotfull.call('llist media all')
        logger.debug(str(result))

        self.assertGreaterEqual(len(result['volumes']), 1)

        for volume in result['volumes']:
            self.assertNotEqual(volume['pool'], pool)

        return True

    def _test_list_with_valid_jobid(self, director, jobid):
        for cmd in ['list', 'llist']:
            result = director.call('{} jobs'.format(cmd))
            self._test_job_result(result['jobs'], jobid)

            listcmd = '{} jobid={}'.format(cmd, jobid)
            result = director.call(listcmd)
            self._test_job_result(result['jobs'], jobid)

    def _test_list_with_invalid_jobid(self, director, jobid):
        for cmd in ['list', 'llist']:
            result = director.call('{} jobs'.format(cmd))
            with self.assertRaises(AssertionError):
                self._test_job_result(result['jobs'], jobid)

            listcmd = '{} jobid={}'.format(cmd, jobid)
            result = director.call(listcmd)
            self.assertEqual(
                len(result), 0,
                u'Command {} should not return results. Current result: {} visible'.
                format(listcmd, str(result)))


class PythonBareosJsonBackendTest(PythonBareosJsonBase):
    def test_json_backend(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password
        client = self.client

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)
        result = director.call('list clients')
        logger.debug(str(result))
        # test if self.client is in the result of "list clients"
        for i in result['clients']:
            logger.debug(str(i))
            if i['name'] == client:
                return
        self.fail(
            'Failed to retrieve client {} from "list clients"'.format(client))

    def test_json_with_invalid_command(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        with self.assertRaises(
                bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call('invalidcommand')

    def test_json_whoami(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)
        result = director.call('whoami')
        logger.debug(str(result))
        self.assertEqual(username, result['whoami'])

    def test_json_backend_without_json_input(self):
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        bareos_password = bareos.bsock.Password(password)

        director_plain = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        result = director_plain.call('show clients')
        logger.debug(str(result))

        director_json = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        # The 'show' command does not deliver JSON output.

        # The JsonRpcInvalidJsonReceivedException
        # is inherited from JsonRpcErrorReceivedException,
        # so both exceptions could by tried.
        with self.assertRaises(
                bareos.exceptions.JsonRpcInvalidJsonReceivedException):
            result = director_json.call('show clients')

        with self.assertRaises(
                bareos.exceptions.JsonRpcErrorReceivedException):
            result = director_json.call('show clients')

    def test_json_no_api_command(self):
        '''
        The bareos.bsock.DirectorConsoleJson calls .api on initialization.
        This test verifies, that a exception is raised,
        when it is not available.
        '''
        logger = logging.getLogger()

        username = 'noapicommand'
        password = 'secret'

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(
                bareos.exceptions.JsonRpcInvalidJsonReceivedException):
            # We expect, that an exception is raised,
            # as con class initialization,
            # the ".api json" command is called
            # and the "noapicommand" don't have access to this command.
            director = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=bareos_password)


class PythonBareosJsonAclTest(PythonBareosJsonBase):
    def test_json_list_media_with_pool_acl(self):
        '''
        This tests checks if the Pool ACL works with the "llist media all" command.

        login as admin
          run a Full job. It gets stored in pool Full
          modify the backup directory
          run a Incremental job. It gets stored in pool Incremental
          verifies that at least 1 volume exists in the Full pool.
          verifies that at least 1 volume exists in the Incremental pool.
        login as console 'poolfull'
          verifies that "llist media all" only shows volumes from the Full pool.
        login as console 'poolnotfull'
          verifies that "llist media all" only shows volumes not from the Full pool.
        '''
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        console_password = 'secret'

        bareos_password = bareos.bsock.Password(password)
        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        result = director_root.call('run job=backup-bareos-fd level=Full yes')
        logger.debug(str(result))

        jobIdFull = result['run']['jobid']

        # wait for job to finish, otherwise next incremental is upgraded to Full.
        result = director_root.call('wait jobid={}'.format(jobIdFull))

        with open(u'{}/extrafile.txt'.format(self.backup_directory),
                  "a") as writer:
            writer.write('Test\n')

        result = director_root.call(
            'run job=backup-bareos-fd level=Incremental yes')
        logger.debug(str(result))

        jobIdIncr = result['run']['jobid']

        result = director_root.call('wait jobid={}'.format(jobIdIncr))

        result = director_root.call('list jobs')
        logger.debug(str(result))

        self._test_job_result(result['jobs'], jobIdFull)
        self._test_job_result(result['jobs'], jobIdIncr)

        result = director_root.call('list volume pool=Full count')
        self.assertTrue(
            int(result['volumes'][0]['count']) >= 1,
            u'Full pool contains no volumes.')

        result = director_root.call('list volume pool=Incremental count')
        self.assertTrue(
            int(result['volumes'][0]['count']) >= 1,
            u'Incremental pool contains no volumes.')

        # without Pool ACL restrictions,
        # 'list media all' returns all volumes from all pools.
        result = director_root.call('list media all')
        logger.debug(str(result))
        self.assertGreaterEqual(len(result['volumes']), 2)

        #
        # login as console 'poolfull'
        #
        bareos_password = bareos.bsock.Password(console_password)
        console_poolfull = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name='poolfull',
            password=bareos_password)

        # 'list media all' returns an error,
        # as the current user has limited Pool ACL permissions.
        # This behavior describes the current behavior.
        # Improvements on the server side welcome.
        with self.assertRaises(
                bareos.exceptions.JsonRpcErrorReceivedException):
            result = console_poolfull.call('list media all')

        result = console_poolfull.call('llist media all')
        logger.debug(str(result))

        self.assertGreaterEqual(len(result['volumes']), 1)

        for volume in result['volumes']:
            self.assertEqual(volume['pool'], 'Full')

        #
        # login as console 'poolnotfull'
        #
        self._test_no_volume_in_pool('poolnotfull', console_password, 'Full')

        #
        # use profile without Pool restrictions
        # and overwrite the Pool ACL in the console.
        #
        console_overwrite = 'overwritepoolacl'
        self.configure_add(
            director_root, 'consoles', console_overwrite,
            u'console={} password={} profile=operator poolacl=!Full'.format(
                console_overwrite, console_password))

        # TODO: IMHO this is a bug. This console should not see volumes in the Full pool.
        #       It needs to be fixed in the server side code.
        with self.assertRaises(AssertionError):
            self._test_no_volume_in_pool(console_overwrite, console_password,
                                         'Full')

    def test_json_list_jobid_with_job_acl(self):
        '''
        This tests checks if the Job ACL works with the "list jobs" and "list jobid=<>" commands.

        login as operator
          run a backup-bareos-fd job.
          create and run a backup-bareos-fd-test job.
          verifies that both jobs are visible by the list command.
        login as a console that can only see backup-bareos-fd jobs
          verifies that the backup-bareos-fd is visible.
          verifies that the backup-bareos-fd is not visible.
        '''
        logger = logging.getLogger()

        username = self.director_operator_username
        password = self.director_operator_password

        console_username = u'job-backup-bareos-fd'
        console_password = u'secret'
        jobname1 = u'backup-bareos-fd'
        jobname2 = u'backup-bareos-fd-test'

        bareos_password = bareos.bsock.Password(password)
        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password)

        jobid1 = self.run_job(
            director=director_root, jobname=jobname1, level=u'Full', wait=True)

        self.configure_add(
            director_root, 'jobs', jobname2,
            'job name={} client=bareos-fd jobdefs=DefaultJob'.format(jobname2))

        jobid2 = self.run_job(
            director=director_root, jobname=jobname2, level=u'Full', wait=True)

        #
        # both jobid should be visible
        #
        self._test_list_with_valid_jobid(director_root, jobid1)
        self._test_list_with_valid_jobid(director_root, jobid2)

        #
        # login as console_username
        #
        bareos_password = bareos.bsock.Password(console_password)
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=console_username,
            password=bareos_password)

        #
        # only jobid1 should be visible
        #
        self._test_list_with_valid_jobid(director, jobid1)
        self._test_list_with_invalid_jobid(director, jobid2)


def get_env():
    '''
    Get attributes as environment variables,
    if not available or set use defaults.
    '''

    director_root_password = os.environ.get('dir_password')
    if director_root_password:
        PythonBareosBase.director_root_password = director_root_password

    director_port = os.environ.get('BAREOS_DIRECTOR_PORT')
    if director_port:
        PythonBareosBase.director_port = director_port

    backup_directory = os.environ.get('BackupDirectory')
    if backup_directory:
        PythonBareosBase.backup_directory = backup_directory

    if os.environ.get('REGRESS_DEBUG'):
        PythonBareosBase.debug = True


if __name__ == '__main__':
    logging.basicConfig(
        format='%(levelname)s %(module)s.%(funcName)s: %(message)s',
        level=logging.ERROR)
    get_env()
    unittest.main()
