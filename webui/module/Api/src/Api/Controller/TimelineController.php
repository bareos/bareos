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

class TimelineController extends AbstractRestfulController
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
        $module = $this->params()->fromQuery('module');
        $jobs = $this->params()->fromQuery('jobs');
        $clients = $this->params()->fromQuery('clients');
        $period = $this->params()->fromQuery('period');

        try{
            if ($module === "client") {
                $this->result = array();
                $c = explode(",", $clients);

                foreach ($c as $client) {
                    $result = array_merge($this->result, $this->getJobModel()->getClientJobsForPeriod($this->bsock, $client, $period));
                }

                $jobs = array();

                // Ensure a proper date.timezone setting for the job timeline.
                // Surpress a possible error thrown by date_default_timezone_get()
                // in older PHP versions with @ in front of the function call.
                date_default_timezone_set(@date_default_timezone_get());

                foreach ($result as $job) {
                    $starttime = new \DateTime($job['starttime'], new \DateTimeZone('UTC'));
                    $endtime = new \DateTime($job['endtime'], new \DateTimeZone('UTC'));
                    $schedtime = new \DateTime($job['schedtime'], new \DateTimeZone('UTC'));

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
                            $endtime = new \DateTime("now", new \DateTimeZone('UTC'));
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
                            $endtime = new \DateTime("now", new \DateTimeZone('UTC'));
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

                    $item = new \stdClass();
                    $item->x = $job["client"];
                    $item->y = array($starttime, $endtime);
                    $item->fillColor = $fillcolor;
                    $item->name = $job["name"];
                    $item->jobid = $job["jobid"];
                    $item->starttime = $job["starttime"];
                    $item->endtime = $job["endtime"];
                    $item->schedtime = $job["schedtime"];
                    $item->client = $job["client"];

                    array_push($jobs, $item);
                }

                $this->result = $jobs;
            }
            elseif ($module === "job") {
                $this->result = array();
                $j = explode(",", $jobs);

                foreach ($j as $jobname) {
                    $this->result = array_merge($this->result, $this->getJobModel()->getJobsForPeriodByJobname($this->bsock, $jobname, $period));
                }

                $jobs = array();

                foreach ($this->result as $job) {
                    $starttime = new \DateTime($job['starttime'], new \DateTimeZone('UTC'));
                    $endtime = new \DateTime($job['endtime'], new \DateTimeZone('UTC'));
                    $schedtime = new \DateTime($job['schedtime'], new \DateTimeZone('UTC'));

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
                            $endtime = new \DateTime("now", new \DateTimeZone('UTC'));
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
                            $endtime = new \DateTime("now", new \DateTimeZone('UTC'));
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

                    $item = new \stdClass();
                    $item->x = $job["client"];
                    $item->y = array($starttime, $endtime);
                    $item->fillColor = $fillcolor;
                    $item->name = $job["name"];
                    $item->jobid = $job["jobid"];
                    $item->starttime = $job["starttime"];
                    $item->endtime = $job["endtime"];
                    $item->schedtime = $job["schedtime"];
                    $item->client = $job["client"];

                    array_push($jobs, $item);
                }

                $this->result = $jobs;
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
