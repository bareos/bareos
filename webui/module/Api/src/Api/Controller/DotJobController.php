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

class DotJobController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $jobModel = null;
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
        $type = $this->params()->fromQuery('type');

        try {
            if ($type === "executable") {
                // Only get manually executable jobs.
                // Jobs of type M,V,R,U,I,C and S are excluded here.
                $jobs_B = $this->getJobModel()->getJobsByType($this->bsock, 'B'); // Backup Job
                $jobs_D = $this->getJobModel()->getJobsByType($this->bsock, 'D'); // Admin Job
                $jobs_A = $this->getJobModel()->getJobsByType($this->bsock, 'A'); // Archive Job
                $jobs_c = $this->getJobModel()->getJobsByType($this->bsock, 'c'); // Copy Job
                $jobs_g = $this->getJobModel()->getJobsByType($this->bsock, 'g'); // Migration Job
                $jobs_O = $this->getJobModel()->getJobsByType($this->bsock, 'O'); // Always Incremental Consolidate Job
                $jobs_V = $this->getJobModel()->getJobsByType($this->bsock, 'V'); // Verify Job
                $this->result = array_merge(
                    $jobs_B,
                    $jobs_D,
                    $jobs_A,
                    $jobs_c,
                    $jobs_g,
                    $jobs_O,
                    $jobs_V
                );
            } else {
                $jobs_B = $this->getJobModel()->getJobsByType($this->bsock, 'B'); // Backup Job
                $jobs_D = $this->getJobModel()->getJobsByType($this->bsock, 'D'); // Admin Job
                $jobs_A = $this->getJobModel()->getJobsByType($this->bsock, 'A'); // Archive Job
                $jobs_c = $this->getJobModel()->getJobsByType($this->bsock, 'c'); // Copy Job
                $jobs_g = $this->getJobModel()->getJobsByType($this->bsock, 'g'); // Migration Job
                $jobs_O = $this->getJobModel()->getJobsByType($this->bsock, 'O'); // Always Incremental Consolidate Job
                $jobs_V = $this->getJobModel()->getJobsByType($this->bsock, 'V'); // Verify Job
                $jobs_R = $this->getJobModel()->getJobsByType($this->bsock, 'R'); // Restore Job
                $this->result = array_merge(
                    $jobs_B,
                    $jobs_D,
                    $jobs_A,
                    $jobs_c,
                    $jobs_g,
                    $jobs_O,
                    $jobs_V,
                    $jobs_R
                );
            }
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
}
