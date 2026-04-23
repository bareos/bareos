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
use Throwable;

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
            $types = [
                'B' => 'Backup Job',
                'D' => 'Admin Job',
                'A' => 'Archive Job',
                'c' => 'Copy Job',
                'g' => 'Migration Job',
                'O' => 'Always Incremental Consolidate Job',
                'V' => 'Verify Job',
            ];

            if ($type !== "executable") {
                $types['R'] = 'Restore Job';
            }

            $jobs = [];
            foreach ($types as $type_char => $type_label) {
                $jobs[] = $this->getJobModel()->getJobsByType($this->bsock, $type_char);
            }

            $jobs = array_values(
                array_filter(
                    $jobs,
                    function ($jobs_by_type) {
                        return is_array($jobs_by_type);
                    }
                )
            );

            if (!empty($jobs)) {
                $this->result = array_merge(...$jobs);
            } else {
                $this->result = [];
            }
        } catch (Throwable $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e->getMessage());
        } finally {
            $this->bsock->disconnect();
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
