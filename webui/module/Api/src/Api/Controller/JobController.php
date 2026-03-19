<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2025 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Api\Controller;

use Laminas\Mvc\Controller\AbstractRestfulController;
use Laminas\View\Model\JsonModel;
use Exception;

class JobController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $jobModel = null;
    protected $clientModel = null;
    protected $result = null;

    public function getList()
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

        $this->bsock = $this->getServiceLocator()->get('director');
        $period = $this->params()->fromQuery('period');
        $filter = $this->params()->fromQuery('filter');
        $client = $this->params()->fromQuery('client');

        try {
            if ($filter === "rerun") {
                $this->result = $this->getJobModel()->getJobsToRerun($this->bsock, $period, null);
            } elseif ($filter === "lastrun") {
                $this->result = $this->getJobModel()->getJobsLastStatus($this->bsock);
            } elseif ($filter === "running") {
                $this->result = $this->getJobModel()->getRunningJobsStatistics($this->bsock);
            } elseif ($filter === "last24h") {
                $days = 1;
                $hours = null;

                $status_categories = [
                    'running' => ['R'],
                    'successful' => ['T', 'D'],
                    'warning' => ['W'],
                    'failed' => ['E', 'e', 'f'],
                    'canceled' => ['A'],
                    'waiting' => ['F', 'S', 'm', 'M', 's', 'j', 'c', 'r', 'C'],
                ];

                $jobs = [];
                $total = 0;
                foreach ($status_categories as $category => $statuses) {
                    $jobs[$category] = [];
                    foreach ($statuses as $status) {
                        $result = $this->getJobModel()->getJobsByStatus($this->bsock, null, $status, $days, $hours);
                        if (is_array($result)) {
                            $jobs[$category] = array_merge($jobs[$category], $result);
                            $total += count($result);
                        }
                    }
                }

                $this->result['waiting'] = count($jobs['waiting']);
                $this->result['running'] = count($jobs['running']);
                $this->result['successful'] = count($jobs['successful']);
                $this->result['warning'] = count($jobs['warning']) + count($jobs['canceled']);
                $this->result['failed'] = count($jobs['failed']);

            } elseif(isset($client)) {
                $this->result = $this->getClientModel()->getClientJobs($this->bsock, $client, null, 'desc', null);
            } else {
                $this->result = $this->getJobModel()->getJobs($this->bsock, null, $period);
            }
        } catch(Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e->getMessage());
        }

        $this->bsock->disconnect();

        return new JsonModel($this->result);
    }

    public function get($id)
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

        $this->bsock = $this->getServiceLocator()->get('director');
        $jobid = $this->params()->fromRoute('id');

        try {
            $this->result = $this->getJobModel()->getJob($this->bsock, $jobid);
        } catch(Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e->getMessage());
        }

        $this->bsock->disconnect();

        return new JsonModel($this->result);
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
}
