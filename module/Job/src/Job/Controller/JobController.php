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

namespace Job\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Job\Form\JobForm;

class JobController extends AbstractActionController
{

   protected $jobModel = null;
   protected $bsock = null;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $period = $this->params()->fromQuery('period') ? $this->params()->fromQuery('period') : '7';
      $status = $this->params()->fromQuery('status') ? $this->params()->fromQUery('status') : 'all';

      $form = new JobForm($period, $status);

      $action = $this->params()->fromQuery('action');
      if(empty($action)) {
         return new ViewModel(
            array(
               'form' => $form,
               'status' => $status,
               'period' => $period
            )
         );
      }
      elseif($action == "rerun") {
         $jobid = $this->params()->fromQuery('jobid');

         $result = null;
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->rerunJob($this->bsock, $jobid);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }

         return new ViewModel(
            array(
               'form' => $form,
               'status' => $status,
               'period' => $period,
               'result' => $result
            )
         );
      }
   }

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $jobid = (int) $this->params()->fromRoute('id', 0);

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $job = $this->getJobModel()->getJob($this->bsock, $jobid);
         $joblog = $this->getJobModel()->getJobLog($this->bsock, $jobid);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(array(
         'job' => $job,
         'joblog' => $joblog,
         'jobid' => $jobid
      ));
   }

   public function cancelAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $jobid = (int) $this->params()->fromRoute('id', 0);

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getJobModel()->cancelJob($this->bsock, $jobid);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'bconsoleOutput' => $result
         )
      );
   }

   public function actionsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $action = $this->params()->fromQuery('action');

      if(empty($action)) {
         return new ViewModel();
      }
      elseif($action == "queue") {
         $jobname = $this->params()->fromQuery('job');
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->runJob($this->bsock, $jobname);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
         return new ViewModel(
            array(
               'result' => $result
            )
         );
      }
      elseif($action == "enable") {
         $jobname = $this->params()->fromQuery('job');
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->enableJob($this->bsock, $jobname);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
         return new ViewModel(
            array(
               'result' => $result
            )
         );
      }
      elseif($action == "disable") {
         $jobname = $this->params()->fromQuery('job');
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->disableJob($this->bsock, $jobname);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
         return new ViewModel(
            array(
               'result' => $result
            )
         );
      }
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $data = $this->params()->fromQuery('data');
      $jobid = $this->params()->fromQuery('jobid');
      $status = $this->params()->fromQuery('status');
      $period = $this->params()->fromQuery('period');

      if($data == "jobs" && $status == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getJobs($this->bsock, $status, $period);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobs" && $status == "successful") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $jobs_T = $this->getJobModel()->getJobsByStatus($this->bsock, 'T', $period, null); // Terminated
            $jobs_W = $this->getJobModel()->getJobsByStatus($this->bsock, 'W', $period, null); // Terminated with warnings
            $result = array_merge($jobs_T, $jobs_W);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobs" && $status == "unsuccessful") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $jobs_A = $this->getJobModel()->getJobsByStatus($this->bsock, 'A', $period, null); // Canceled jobs
            $jobs_E = $this->getJobModel()->getJobsByStatus($this->bsock, 'E', $period, null); //
            $jobs_e = $this->getJobModel()->getJobsByStatus($this->bsock, 'e', $period, null); //
            $jobs_f = $this->getJobModel()->getJobsByStatus($this->bsock, 'f', $period, null); //
            $result = array_merge($jobs_A, $jobs_E, $jobs_e, $jobs_f);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobs" && $status == "running") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $jobs_R = $this->getJobModel()->getJobsByStatus($this->bsock, 'R', $period, null);
            $jobs_l = $this->getJobModel()->getJobsByStatus($this->bsock, 'l', $period, null);
            $result = array_merge($jobs_R, $jobs_l);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobs" && $status == "waiting") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $jobs_F = $this->getJobModel()->getJobsByStatus($this->bsock, 'F', $period, null);
            $jobs_S = $this->getJobModel()->getJobsByStatus($this->bsock, 'S', $period, null);
            $jobs_m = $this->getJobModel()->getJobsByStatus($this->bsock, 'm', $period, null);
            $jobs_M = $this->getJobModel()->getJobsByStatus($this->bsock, 'M', $period, null);
            $jobs_s = $this->getJobModel()->getJobsByStatus($this->bsock, 's', $period, null);
            $jobs_j = $this->getJobModel()->getJobsByStatus($this->bsock, 'j', $period, null);
            $jobs_c = $this->getJobModel()->getJobsByStatus($this->bsock, 'c', $period, null);
            $jobs_d = $this->getJobModel()->getJobsByStatus($this->bsock, 'd', $period, null);
            $jobs_t = $this->getJobModel()->getJobsByStatus($this->bsock, 't', $period, null);
            $jobs_p = $this->getJobModel()->getJobsByStatus($this->bsock, 'p', $period, null);
            $jobs_q = $this->getJobModel()->getJobsByStatus($this->bsock, 'q', $period, null);
            $jobs_C = $this->getJobModel()->getJobsByStatus($this->bsock, 'C', $period, null);
            $result = array_merge(
               $jobs_F,$jobs_S,$jobs_m,$jobs_M,
               $jobs_s,$jobs_j,$jobs_c,$jobs_d,
               $jobs_t,$jobs_p,$jobs_q,$jobs_C
            );
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "backupjobs") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getBackupJobs($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "details") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getJob($this->bsock, $jobid);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "logs" && isset($jobid)) {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getJobModel()->getJobLog($this->bsock, $jobid);
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

   public function getJobModel()
   {
      if(!$this->jobModel) {
         $sm = $this->getServiceLocator();
         $this->jobModel = $sm->get('Job\Model\JobModel');
      }
      return $this->jobModel;
   }
}
