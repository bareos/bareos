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

namespace Api\Controller;

use Zend\Mvc\Controller\AbstractRestfulController;
use Zend\View\Model\JsonModel;
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
                $waiting = null;
                $running = null;
                $successful = null;
                $warning = null;
                $failed = null;

                // waiting
                $jobs_F = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'F', $days, $hours);
                $jobs_S = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'S', $days, $hours);
                $jobs_s = $this->getJobModel()->getJobsByStatus($this->bsock, null, 's', $days, $hours);
                $jobs_m = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'm', $days, $hours);
                $jobs_M = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'M', $days, $hours);
                $jobs_j = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'j', $days, $hours);
                $jobs_c = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'c', $days, $hours);
                $jobs_C = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'C', $days, $hours);
                $jobs_d = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'd', $days, $hours);
                $jobs_t = $this->getJobModel()->getJobsByStatus($this->bsock, null, 't', $days, $hours);
                $jobs_p = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'p', $days, $hours);
                $jobs_q = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'q', $days, $hours);
                $waiting = count($jobs_F) + count($jobs_S) +
                    count($jobs_s) + count($jobs_m) +
                    count($jobs_M) + count($jobs_j) +
                    count($jobs_c) + count($jobs_C) +
                    count($jobs_d) + count($jobs_t) +
                    count($jobs_p) + count($jobs_q);

                // running
                $jobs_R = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'R', $days, $hours);
                $jobs_l = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'l', $days, $hours);
                $running = count($jobs_R) + count($jobs_l);

                // successful
                $jobs_T = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'T', $days, $hours);
                $successful = count($jobs_T);

                // warning
                $jobs_A = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'A', $days, $hours);
                $jobs_W = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'W', $days, $hours);
                $warning = count($jobs_A) + count($jobs_W);

                // failed
                $jobs_E = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'E', $days, $hours);
                $jobs_e = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'e', $days, $hours);
                $jobs_f = $this->getJobModel()->getJobsByStatus($this->bsock, null, 'f', $days, $hours);
                $failed = count($jobs_E) + count($jobs_e) + count($jobs_f);

                // json result
                $this->result['waiting'] = $waiting;
                $this->result['running'] = $running;
                $this->result['successful'] = $successful;
                $this->result['warning'] = $warning;
                $this->result['failed'] = $failed;

            } elseif(isset($client)) {
                $this->result = $this->getClientModel()->getClientJobs($this->bsock, $client, null, 'desc', null);
            } else {
                $this->result = $this->getJobModel()->getJobs($this->bsock, null, $period);
            }
        } catch(Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e);
        }

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
            error_log($e);
        }

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
