<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
   protected $dashboardModel;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI())));
      }

      return new ViewModel(
         array(
            'runningJobs' => $this->getJobs("running", 1, null),
            'waitingJobs' => $this->getJobs("waiting", 1, null),
            'successfulJobs' => $this->getJobs("successful", 1, null),
            'unsuccessfulJobs' => $this->getJobs("unsuccessful", 1, null),
         )
      );
   }

   private function getJobs($status=null, $days=1, $hours=null)
   {
      if($status != null) {
         if($status == "running") {
            $jobs_R = $this->getDashboardModel()->getJobs('R', $days, $hours);
            $jobs_l = $this->getDashboardModel()->getJobs('l', $days, $hours);
            $num = count($jobs_R) + count($jobs_l);
            return $num;
         }
         elseif($status == "waiting") {
            $jobs_F = $this->getDashboardModel()->getJobs('F', $days, $hours);
            $jobs_S = $this->getDashboardModel()->getJobs('S', $days, $hours);
            $jobs_s = $this->getDashboardModel()->getJobs('s', $days, $hours);
            $jobs_m = $this->getDashboardModel()->getJobs('m', $days, $hours);
            $jobs_M = $this->getDashboardModel()->getJobs('M', $days, $hours);
            $jobs_j = $this->getDashboardModel()->getJobs('j', $days, $hours);
            $jobs_c = $this->getDashboardModel()->getJobs('c', $days, $hours);
            $jobs_C = $this->getDashboardModel()->getJobs('C', $days, $hours);
            $jobs_d = $this->getDashboardModel()->getJobs('d', $days, $hours);
            $jobs_t = $this->getDashboardModel()->getJobs('t', $days, $hours);
            $jobs_p = $this->getDashboardModel()->getJobs('p', $days, $hours);
            $jobs_q = $this->getDashboardModel()->getJobs('q', $days, $hours);
            $num = count($jobs_F) + count($jobs_S) +
               count($jobs_s) + count($jobs_m) +
               count($jobs_M) + count($jobs_j) +
               count($jobs_c) + count($jobs_C) +
               count($jobs_d) + count($jobs_t) +
               count($jobs_p) + count($jobs_q);
            return $num;
         }
         elseif($status == "successful") {
            $jobs_T = $this->getDashboardModel()->getJobs('T', $days, $hours);
            $jobs_W = $this->getDashboardModel()->getJobs('W', $days, $hours);
            $num = count($jobs_T) + count($jobs_W);
            return $num;
         }
         elseif($status == "unsuccessful") {
            $jobs_A = $this->getDashboardModel()->getJobs('A', $days, $hours);
            $jobs_E = $this->getDashboardModel()->getJobs('E', $days, $hours);
            $jobs_e = $this->getDashboardModel()->getJobs('e', $days, $hours);
            $jobs_f = $this->getDashboardModel()->getJobs('f', $days, $hours);
            $num = count($jobs_A) + count($jobs_E) + count($jobs_e) + count($jobs_f);
            return $num;
         }
         else {
            return null;
         }
      }
      else {
         return null;
      }
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI())));
      }

      $result = $this->getDashboardModel()->getJobsLastStatus();

      $response = $this->getResponse();
      $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

      if(isset($result)) {
         $response->setContent(JSON::encode($result));
      }

      return $response;

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
