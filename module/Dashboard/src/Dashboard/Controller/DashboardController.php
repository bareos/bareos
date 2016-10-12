<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Dashboard\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class DashboardController extends AbstractActionController
{
   protected $directorModel = null;
   protected $jobModel = null;
   protected $dashboardModel = null;
   protected $bsock = null;
   protected $acl_alert = false;

   private $required_commands = array(
      "list",
      "llist"
   );

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      if(!$this->CommandACLPlugin()->validate($_SESSION['bareos']['commands'], $this->required_commands)) {
         $this->acl_alert = true;
         return new ViewModel(
            array(
               'acl_alert' => $this->acl_alert,
               'required_commands' => $this->required_commands,
            )
         );
      }

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $running = $this->getJobs("running", 1, null);
         $waiting = $this->getJobs("waiting", 1, null);
         $successful = $this->getJobs("successful", 1, null);
         $unsuccessful = $this->getJobs("unsuccessful", 1, null);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'runningJobs' => $running,
            'waitingJobs' => $waiting,
            'successfulJobs' => $successful,
            'unsuccessfulJobs' => $unsuccessful,
         )
      );
   }

   private function getJobs($status=null, $days=1, $hours=null)
   {
      $num = null;

      if($status != null) {
         if($status == "running") {
            $jobs_R = $this->getJobModel()->getJobsByStatus($this->bsock, 'R', $days, $hours);
            $jobs_l = $this->getJobModel()->getJobsByStatus($this->bsock, 'l', $days, $hours);
            $num = count($jobs_R) + count($jobs_l);
         }
         elseif($status == "waiting") {
            $jobs_F = $this->getJobModel()->getJobsByStatus($this->bsock, 'F', $days, $hours);
            $jobs_S = $this->getJobModel()->getJobsByStatus($this->bsock, 'S', $days, $hours);
            $jobs_s = $this->getJobModel()->getJobsByStatus($this->bsock, 's', $days, $hours);
            $jobs_m = $this->getJobModel()->getJobsByStatus($this->bsock, 'm', $days, $hours);
            $jobs_M = $this->getJobModel()->getJobsByStatus($this->bsock, 'M', $days, $hours);
            $jobs_j = $this->getJobModel()->getJobsByStatus($this->bsock, 'j', $days, $hours);
            $jobs_c = $this->getJobModel()->getJobsByStatus($this->bsock, 'c', $days, $hours);
            $jobs_C = $this->getJobModel()->getJobsByStatus($this->bsock, 'C', $days, $hours);
            $jobs_d = $this->getJobModel()->getJobsByStatus($this->bsock, 'd', $days, $hours);
            $jobs_t = $this->getJobModel()->getJobsByStatus($this->bsock, 't', $days, $hours);
            $jobs_p = $this->getJobModel()->getJobsByStatus($this->bsock, 'p', $days, $hours);
            $jobs_q = $this->getJobModel()->getJobsByStatus($this->bsock, 'q', $days, $hours);
            $num = count($jobs_F) + count($jobs_S) +
               count($jobs_s) + count($jobs_m) +
               count($jobs_M) + count($jobs_j) +
               count($jobs_c) + count($jobs_C) +
               count($jobs_d) + count($jobs_t) +
               count($jobs_p) + count($jobs_q);
         }
         elseif($status == "successful") {
            $jobs_T = $this->getJobModel()->getJobsByStatus($this->bsock, 'T', $days, $hours);
            $jobs_W = $this->getJobModel()->getJobsByStatus($this->bsock, 'W', $days, $hours);
            $num = count($jobs_T) + count($jobs_W);
         }
         elseif($status == "unsuccessful") {
            $jobs_A = $this->getJobModel()->getJobsByStatus($this->bsock, 'A', $days, $hours);
            $jobs_E = $this->getJobModel()->getJobsByStatus($this->bsock, 'E', $days, $hours);
            $jobs_e = $this->getJobModel()->getJobsByStatus($this->bsock, 'e', $days, $hours);
            $jobs_f = $this->getJobModel()->getJobsByStatus($this->bsock, 'f', $days, $hours);
            $num = count($jobs_A) + count($jobs_E) + count($jobs_e) + count($jobs_f);
         }
      }

      return $num;
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $data = $this->params()->fromQuery('data');

      if($data == "jobslaststatus") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getJobsLastStatus($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "dirdmsg") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getDirectorModel()->getDirectorMessages($this->bsock, 50, null, false);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      $response = $this->getResponse();
      $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

      if(isset($result)) {
         $response->setContent(JSON::encode($result));
      }

      return $response;

   }

   public function getDirectorModel()
   {
      if(!$this->directorModel) {
         $sm = $this->getServiceLocator();
         $this->directorModel = $sm->get('Director\Model\DirectorModel');
      }
      return $this->directorModel;
   }

   public function getJobModel()
   {
      if(!$this->jobModel) {
         $sm = $this->getServiceLocator();
         $this->jobModel = $sm->get('Job\Model\JobModel');
      }
      return $this->jobModel;
   }

   public function getDashboardModel()
   {
      if(!$this->dashboardModel) {
         $sm = $this->getServiceLocator();
         $this->dashboardModel = $sm->get('Dashboard\Model\DashboardModel');
      }
      return $this->dashboardModel;
   }

}
