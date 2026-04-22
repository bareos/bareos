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

/**
 * Live status of a currently-running job. Consumes the director's
 * `status jobid=N` aggregator (available under `.api json` in Bareos
 * with the JSON-status feature). The director fans out to the FD and
 * SD and returns a single document combining dir/fd/sd views.
 *
 * Response matches the aggregator output verbatim:
 *   {
 *     "job_status": {
 *       "jobid": N,
 *       "dir": {...},          # only if the job is currently known
 *       "fd":  { "data": {...} | "error": "..." },
 *       "sd":  { "data": {...} | "error": "..." }
 *     }
 *   }
 * or:
 *   { "job_status": { "jobid": N, "error": "job_not_found" } }
 */
class JobStatusController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $jobModel = null;
    protected $result = null;

    /**
     * Handle `GET /api/jobstatus` (collection). Not used — clients must
     * request a specific jobid via `GET /api/jobstatus/<id>`. Redirects
     * to the login page if the session has timed out; otherwise returns
     * an empty JSON object.
     *
     * @return JsonModel|\Laminas\Http\Response
     */
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

        return new JsonModel();
    }

    /**
     * Handle `GET /api/jobstatus/<id>`. Forwards to
     * JobModel::getLiveJobStatus(), which invokes the director's
     * aggregating `status jobid=<id>` command under `.api json`.
     * Redirects to login on session timeout. Returns HTTP 500 if the
     * director call throws.
     *
     * @param int|string $id jobid to query.
     * @return JsonModel|\Laminas\Http\Response
     */
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
            $this->result = $this->getJobModel()->getLiveJobStatus($this->bsock, $jobid);
        } catch (Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e);
        }

        return new JsonModel($this->result);
    }

    /**
     * Lazily resolve and memoize the Job model from the service locator.
     *
     * @return \Job\Model\JobModel
     */
    public function getJobModel()
    {
        if (!$this->jobModel) {
            $sm = $this->getServiceLocator();
            $this->jobModel = $sm->get('Job\Model\JobModel');
        }
        return $this->jobModel;
    }
}
