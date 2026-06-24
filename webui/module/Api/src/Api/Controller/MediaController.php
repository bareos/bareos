<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

class MediaController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $mediaModel = null;
    protected $poolModel = null;
    protected $jobModel =null;
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
        $pool = $this->params()->fromQuery('pool');
        $volume = $this->params()->fromQuery('volume');
        $filter = $this->params()->fromQuery('filter');
        $jobid = $this->params()->fromQuery('jobid');

        if ($pool !== null && !preg_match('/^[A-Za-z0-9_\-\. ]+$/', $pool)) {
            $this->bsock->disconnect();
            $this->getResponse()->setStatusCode(400);
            return new JsonModel(['error' => 'Invalid pool name']);
        }
        if ($volume !== null && !preg_match('/^[A-Za-z0-9_\-\. ]+$/', $volume)) {
            $this->bsock->disconnect();
            $this->getResponse()->setStatusCode(400);
            return new JsonModel(['error' => 'Invalid volume name']);
        }
        if ($jobid !== null) {
            $jobid = (int) $jobid;
            if ($jobid <= 0) {
                $this->bsock->disconnect();
                $this->getResponse()->setStatusCode(400);
                return new JsonModel(['error' => 'Invalid job ID']);
            }
        }

        try{
            if ($filter === "jobs" && isset($volume)) {
                $this->result = $this->getMediaModel()->getVolumeJobs($this->bsock, $volume);
            } elseif($filter === "jobid" && isset($jobid)) {
                $this->result = $this->getJobModel()->getJobMedia($this->bsock, $jobid);
            } elseif (isset($pool)) {
                $this->result = $this->getPoolModel()->getPoolMedia($this->bsock, $pool);
            } elseif (isset($volume)) {
                // workaround until llist volume returns array instead of object
                $this->result[0] = $this->getMediaModel()->getVolume($this->bsock, $volume);
            } else {
                $this->result = $this->getMediaModel()->getVolumes($this->bsock);
            }
        } catch(Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e->getMessage());
        }

        $this->bsock->disconnect();

        return new JsonModel($this->result);
    }

    public function getMediaModel()
    {
        if (!$this->mediaModel) {
            $sm = $this->getServiceLocator();
            $this->mediaModel = $sm->get('Media\Model\MediaModel');
        }
        return $this->mediaModel;
    }

    public function getPoolModel()
    {
        if (!$this->poolModel) {
            $sm = $this->getServiceLocator();
            $this->poolModel = $sm->get('Pool\Model\PoolModel');
        }
        return $this->poolModel;
    }

    public function getJobModel()
    {
        if (!$this->jobModel) {
            $sm = $this->getServiceLocator();
            $this->jobModel = $sm->get('Job\Model\JobModel');
        }
        return $this->jobModel;
    }
}
