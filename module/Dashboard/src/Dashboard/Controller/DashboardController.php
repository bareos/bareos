<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2017 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
   /**
    * Variables
    */
   protected $directorModel = null;
   protected $jobModel = null;
   protected $dashboardModel = null;
   protected $bsock = null;
   protected $acl_alert = false;

   private $required_commands = array(
      "list",
      "llist"
   );

   /**
    * Index Action
    *
    * @return object
    */
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

      return new ViewModel();
   }

   /**
    * Get Data Action
    *
    * @return object
    */
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
      elseif($data == "jobspast24h") {

         $days = 1;
         $hours = null;
         $waiting = null;
         $running = null;
         $successful = null;
         $warning = null;
         $failed = null;
         $result = null;

         try {
            $this->bsock = $this->getServiceLocator()->get('director');

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
            $result['waiting'] = $waiting;
            $result['running'] = $running;
            $result['successful'] = $successful;
            $result['warning'] = $warning;
            $result['failed'] = $failed;
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "runningjobs") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getRunningJobsStatistics($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobtotals") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getJobTotals($this->bsock);
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

   /**
    * Get Director Model
    *
    * @return object
    */
   public function getDirectorModel()
   {
      if(!$this->directorModel) {
         $sm = $this->getServiceLocator();
         $this->directorModel = $sm->get('Director\Model\DirectorModel');
      }
      return $this->directorModel;
   }

   /**
    * Get Job Model
    *
    * @return object
    */
   public function getJobModel()
   {
      if(!$this->jobModel) {
         $sm = $this->getServiceLocator();
         $this->jobModel = $sm->get('Job\Model\JobModel');
      }
      return $this->jobModel;
   }

   /**
    * Get Dashboard Model
    *
    * @return object
    */
   public function getDashboardModel()
   {
      if(!$this->dashboardModel) {
         $sm = $this->getServiceLocator();
         $this->dashboardModel = $sm->get('Dashboard\Model\DashboardModel');
      }
      return $this->dashboardModel;
   }

}
