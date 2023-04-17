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
            error_log($e);
        }

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
