<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Job\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Job\Form\JobForm;
use Job\Form\RunJobForm;
use Job\Model\Job;
use Exception;

class JobController extends AbstractActionController
{
    protected $jobModel = null;
    protected $clientModel = null;
    protected $filesetModel = null;
    protected $storageModel = null;
    protected $poolModel = null;
    protected $bsock = null;
    protected $acl_alert = false;

    public function indexAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $period = $this->params()->fromQuery('period') ? $this->params()->fromQuery('period') : '7';
        $status = $this->params()->fromQuery('status') ? $this->params()->fromQuery('status') : 'all';
        $jobname = $this->params()->fromQuery('jobname') ? $this->params()->fromQuery('jobname') : 'all';

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $jobs = $this->getJobModel()->getJobsByType($this->bsock, null);
            array_push($jobs, array('name' => 'all'));
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        $form = new JobForm($jobs, $jobname, $period, $status);
        $result = null;

        $action = $this->params()->fromQuery('action');
        if (empty($action)) {
            return new ViewModel(
                array(
                    'form' => $form,
                    'status' => $status,
                    'period' => $period,
                    'jobname' => $jobname
                )
            );
        } else {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            if ($action == "rerun") {
                try {
                    $jobid = $this->params()->fromQuery('jobid');
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Job']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('rerun', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'rerun'
                            )
                        );
                    } else {
                        $result = $this->getJobModel()->rerunJob($this->bsock, $jobid);
                        if (!preg_match("/authorization/i", $result)) {
                            $jobid = rtrim(substr($result, strrpos($result, "=") + 1));
                            return $this->redirect()->toRoute('job', array('action' => 'details', 'id' => $jobid));
                        }
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            }

            try {
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            return new ViewModel(
                array(
                    'form' => $form,
                    'status' => $status,
                    'period' => $period,
                    'jobname' => $jobname,
                    'result' => $result
                )
            );
        }
    }

    public function detailsAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $jobid = (int) $this->params()->fromRoute('id', 0);

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(array(
            'jobid' => $jobid
        ));
    }

    public function cancelAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['optional']
        );
        if (count($invalid_commands) > 0 && in_array('cancel', $invalid_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => 'cancel'
                )
            );
        }

        $result = null;

        $jobid = (int) $this->params()->fromRoute('id', 0);

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->cancelJob($this->bsock, $jobid);
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(
            array(
                'bconsoleOutput' => $result
            )
        );
    }

    public function actionsAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $result = null;

        $action = $this->params()->fromQuery('action');

        if (empty($action)) {
            return new ViewModel();
        } else {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            if ($action == "queue") {
                $jobname = $this->params()->fromQuery('job');
                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Job']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('run', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'run'
                            )
                        );
                    } else {
                        $result = $this->getJobModel()->runJob($this->bsock, $jobname);
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            } elseif ($action == "enable") {
                $jobname = $this->params()->fromQuery('job');
                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Job']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('enable', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'enable'
                            )
                        );
                    } else {
                        $result = $this->getJobModel()->enableJob($this->bsock, $jobname);
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            } elseif ($action == "disable") {
                $jobname = $this->params()->fromQuery('job');
                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Job']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('disable', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'disable'
                            )
                        );
                    } else {
                        $result = $this->getJobModel()->disableJob($this->bsock, $jobname);
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            }

            try {
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            return new ViewModel(
                array(
                    'result' => $result
                )
            );
        }
    }

    public function runAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $this->bsock = $this->getServiceLocator()->get('director');

        // Get query parameter jobname
        $jobname = $this->params()->fromQuery('jobname') ? $this->params()->fromQuery('jobname') : null;

        if ($jobname != null) {
            $jobdefaults = $this->getJobModel()->getJobDefaults($this->bsock, $jobname);
        } else {
            $jobdefaults = null;
        }

        // Get required form construction data, jobs, clients, etc.
        $clients = $this->getClientModel()->getClients($this->bsock);

        // Get the different kind of jobs and merge them. Jobs of the following types
        // cannot nor wanted to be run. M,V,R,U,I,C and S.
        $job_type_B = $this->getJobModel()->getJobsByType($this->bsock, "B"); // Backup Job
        $job_type_D = $this->getJobModel()->getJobsByType($this->bsock, 'D'); // Admin Job
        $job_type_A = $this->getJobModel()->getJobsByType($this->bsock, 'A'); // Archive Job
        $job_type_c = $this->getJobModel()->getJobsByType($this->bsock, 'c'); // Copy Job
        $job_type_g = $this->getJobModel()->getJobsByType($this->bsock, 'g'); // Migration Job
        $job_type_O = $this->getJobModel()->getJobsByType($this->bsock, 'O'); // Always Incremental Consolidate Job
        $job_type_V = $this->getJobModel()->getJobsByType($this->bsock, 'V'); // Verify Job

        $jobs = array_merge($job_type_B, $job_type_D, $job_type_A, $job_type_c, $job_type_g, $job_type_O, $job_type_V);

        $filesets = $this->getFilesetModel()->getDotFilesets($this->bsock);
        $storages = $this->getStorageModel()->getStorages($this->bsock);
        $pools = $this->getPoolModel()->getDotPools($this->bsock, null);

        foreach ($pools as &$pool) {
            $nextpool = $this->getPoolModel()->getPoolNextPool($this->bsock, $pool["name"]);
            $pool["nextpool"] = $nextpool;
        }

        // build form
        $form = new RunJobForm($clients, $jobs, $filesets, $storages, $pools, $jobdefaults);

        // Set the method attribute for the form
        $form->setAttribute('method', 'post');

        // result
        $result = null;
        $errors = null;

        $request = $this->getRequest();

        if ($request->isPost()) {
            $job = new Job();
            $form->setInputFilter($job->getInputFilter());
            $form->setData($request->getPost());

            if ($form->isValid()) {
                $jobname = $form->getInputFilter()->getValue('job');
                $client = $form->getInputFilter()->getValue('client');
                $fileset = $form->getInputFilter()->getValue('fileset');
                $storage = $form->getInputFilter()->getValue('storage');
                $pool = $form->getInputFilter()->getValue('pool');
                $level = $form->getInputFilter()->getValue('level');
                $nextpool = $form->getInputFilter()->getValue('nextpool');
                $priority = $form->getInputFilter()->getValue('priority');
                $backupformat = null; // $form->getInputFilter()->getValue('backupformat');
                $when = $form->getInputFilter()->getValue('when');

                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Job']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('run', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'run'
                            )
                        );
                    } else {
                        $result = $this->getJobModel()->runCustomJob($this->bsock, $jobname, $client, $fileset, $storage, $pool, $level, $nextpool, $priority, $backupformat, $when);
                        $this->bsock->disconnect();
                        $s = strrpos($result, "=") + 1;
                        $jobid = rtrim(substr($result, $s));
                        preg_match("'\d{1,99}(?=%)?'", $jobid, $filtered);
                        return $this->redirect()->toRoute('job', array('action' => 'details', 'id' => $filtered[0]));
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            } else {
                $this->bsock->disconnect();
                return new ViewModel(array(
                    'form' => $form,
                    'result' => $result,
                    'errors' => $errors
                ));
            }
        } else {
            $this->bsock->disconnect();
            return new ViewModel(array(
                'form' => $form,
                'result' => $result,
                'errors' => $errors
            ));
        }
    }

    public function timelineAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Job']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $this->bsock = $this->getServiceLocator()->get('director');

        return new ViewModel();
    }

    public function getDataAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $result = null;

        $data = $this->params()->fromQuery('data');
        $jobid = $this->params()->fromQuery('jobid');
        $jobname = $this->params()->fromQuery('jobname');
        $jobs = $this->params()->fromQuery('jobs');
        $status = $this->params()->fromQuery('status');
        $period = $this->params()->fromQuery('period');
        $client = $this->params()->fromQuery('client');
        $clients = $this->params()->fromQUery('clients');

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        if ($data == "jobs") {
            try {
                $result = $this->getJobModel()->getJobs($this->bsock, null, $period);
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data == "runjobs") {
            try {
                // Get the different kind of jobs and merge them. Jobs of the following types
                // cannot nor wanted to be run. M,V,R,U,I,C and S.
                $jobs_B = $this->getJobModel()->getJobsByType($this->bsock, 'B'); // Backup Job
                $jobs_D = $this->getJobModel()->getJobsByType($this->bsock, 'D'); // Admin Job
                $jobs_A = $this->getJobModel()->getJobsByType($this->bsock, 'A'); // Archive Job
                $jobs_c = $this->getJobModel()->getJobsByType($this->bsock, 'c'); // Copy Job
                $jobs_g = $this->getJobModel()->getJobsByType($this->bsock, 'g'); // Migration Job
                $jobs_O = $this->getJobModel()->getJobsByType($this->bsock, 'O'); // Always Incremental Consolidate Job
                $jobs_V = $this->getJobModel()->getJobsByType($this->bsock, 'V'); // Verify Job
                $result = array_merge(
                    $jobs_B,
                    $jobs_D,
                    $jobs_A,
                    $jobs_c,
                    $jobs_g,
                    $jobs_O,
                    $jobs_V
                );
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data == "all-job-resources") {
            try {
                $jobs_B = $this->getJobModel()->getJobsByType($this->bsock, 'B'); // Backup Job
                $jobs_D = $this->getJobModel()->getJobsByType($this->bsock, 'D'); // Admin Job
                $jobs_A = $this->getJobModel()->getJobsByType($this->bsock, 'A'); // Archive Job
                $jobs_c = $this->getJobModel()->getJobsByType($this->bsock, 'c'); // Copy Job
                $jobs_g = $this->getJobModel()->getJobsByType($this->bsock, 'g'); // Migration Job
                $jobs_O = $this->getJobModel()->getJobsByType($this->bsock, 'O'); // Always Incremental Consolidate Job
                $jobs_V = $this->getJobModel()->getJobsByType($this->bsock, 'V'); // Verify Job
                $jobs_R = $this->getJobModel()->getJobsByType($this->bsock, 'R'); // Restore Job
                $result = array_merge(
                    $jobs_B,
                    $jobs_D,
                    $jobs_A,
                    $jobs_c,
                    $jobs_g,
                    $jobs_O,
                    $jobs_V,
                    $jobs_R
                );
            } catch (Exception $e) {
                echo $e->getMessages();
            }
        } elseif ($data == "details") {
            try {
                $result = $this->getJobModel()->getJob($this->bsock, $jobid);
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data == "logs" && isset($jobid)) {
            try {
                $result = $this->getJobModel()->getJobLog($this->bsock, $jobid);
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data == "jobmedia" && isset($jobid)) {
            try {
                $result = $this->getJobModel()->getJobMedia($this->bsock, $jobid);
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data = "job-timeline") {
            try {
                $result = [];
                $j = explode(",", $jobs);

                foreach ($j as $jobname) {
                    $result = array_merge($result, $this->getJobModel()->getJobsForPeriodByJobname($this->bsock, $jobname, $period));
                }

                $jobs = array();

                // Ensure a proper date.timezone setting for the job timeline.
                // Surpress a possible error thrown by date_default_timezone_get()
                // in older PHP versions with @ in front of the function call.
                date_default_timezone_set(@date_default_timezone_get());

                foreach ($result as $job) {
                    $starttime = new \DateTime($job['starttime']);
                    $endtime = new \DateTime($job['endtime']);
                    $schedtime = new \DateTime($job['schedtime']);

                    $starttime = $starttime->format('U') * 1000;
                    $endtime = $endtime->format('U') * 1000;
                    $schedtime = $schedtime->format('U') * 1000;

                    switch ($job['jobstatus']) {
                        // SUCESS
                        case 'T':
                            $fillcolor = "#5cb85c";
                            break;
                            // WARNING
                        case 'A':
                        case 'W':
                            $fillcolor = "#f0ad4e";
                            break;
                            // RUNNING
                        case 'R':
                        case 'l':
                            $fillcolor = "#5bc0de";
                            $endtime = new \DateTime(null);
                            $endtime = $endtime->format('U') * 1000;
                            break;
                            // FAILED
                        case 'E':
                        case 'e':
                        case 'f':
                            $fillcolor = "#d9534f";
                            break;
                            // WAITING
                        case 'F':
                        case 'S':
                        case 's':
                        case 'M':
                        case 'm':
                        case 'j':
                        case 'C':
                        case 'c':
                        case 'd':
                        case 't':
                        case 'p':
                        case 'q':
                            $fillcolor = "#555555";
                            $endtime = new \DateTime(null);
                            $endtime = $endtime->format('U') * 1000;
                            break;
                        default:
                            $fillcolor = "#555555";
                            break;
                    }

                    // workaround to display short job runs <= 1 sec.
                    if ($starttime === $endtime) {
                        $endtime += 1000;
                    }

                    $item = '{"x":"' . $job['name'] . '","y":["' . $starttime . '","' . $endtime . '"],"fillColor":"' . $fillcolor . '","name":"' . $job['name'] . '","jobid":"' . $job['jobid'] . '","starttime":"' . $job['starttime'] . '","endtime":"' . $job['endtime'] . '","schedtime":"' . $job['schedtime'] . '","client":"' . $job['client'] . '"}';
                    array_push($jobs, json_decode($item));
                }

                $result = $jobs;
            } catch (Exception $e) {
                echo $e->getMessages();
            }
        }

        try {
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        $response = $this->getResponse();
        $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

        if (isset($result)) {
            $response->setContent(JSON::encode($result));
        }

        return $response;
    }

    public function getJobModel()
    {
        if (!$this->jobModel) {
            $sm = $this->getServiceLocator();
            $this->jobModel = $sm->get('Job\Model\JobModel');
        }
        return $this->jobModel;
    }

    public function getClientModel()
    {
        if (!$this->clientModel) {
            $sm = $this->getServiceLocator();
            $this->clientModel = $sm->get('Client\Model\ClientModel');
        }
        return $this->clientModel;
    }

    public function getStorageModel()
    {
        if (!$this->storageModel) {
            $sm = $this->getServiceLocator();
            $this->storageModel = $sm->get('Storage\Model\StorageModel');
        }
        return $this->storageModel;
    }

    public function getPoolModel()
    {
        if (!$this->poolModel) {
            $sm = $this->getServiceLocator();
            $this->poolModel = $sm->get('Pool\Model\PoolModel');
        }
        return $this->poolModel;
    }

    public function getFilesetModel()
    {
        if (!$this->filesetModel) {
            $sm = $this->getServiceLocator();
            $this->filesetModel = $sm->get('Fileset\Model\FilesetModel');
        }
        return $this->filesetModel;
    }
}
