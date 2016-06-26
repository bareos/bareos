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

namespace Schedule\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class ScheduleController extends AbstractActionController
{

   protected $scheduleModel;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $schedules = $this->getScheduleModel()->getSchedules();
      $action = $this->params()->fromQuery('action');

      if(empty($action)) {
         return new ViewModel(
            array(
               'schedules' => $schedules
            )
         );
      }
      elseif($action == "enable") {
         $schedulename = $this->params()->fromQuery('schedule');
         $result = $this->getScheduleModel()->enableSchedule($schedulename);
         return new ViewModel(
            array(
               'schedules' => $schedules,
               'result' => $result
            )
         );
      }
      elseif($action == "disable") {
         $schedulename = $this->params()->fromQuery('schedule');
         $result = $this->getScheduleModel()->disableSchedule($schedulename);
         return new ViewModel(
            array(
               'schedules' => $schedules,
               'result' => $result
            )
         );
      }
   }

   public function overviewAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = $this->getScheduleModel()->showSchedules();

      return new ViewModel(
         array(
            'result' => $result
         )
      );
   }

   public function statusAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = $this->getScheduleModel()->getFullScheduleStatus();

      return new ViewModel(
         array(
            'result' => $result
         )
      );
   }

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $schedulename = $this->params()->fromQuery('schedule');
      $result = $this->getScheduleModel()->getScheduleStatus($schedulename);

      return new ViewModel(
         array(
            'result' => $result
         )
      );
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $data = $this->params()->fromQuery('data');
      $schedule = $this->params()->fromQuery('schedule');

      if($data == "all") {
         $result = $this->getScheduleModel()->getSchedules();
      }
      elseif($data == "details" && isset($schedule)) {
         $result = $this->getScheduleModel()->getSchedule($schedule);
      }
      else {
         $result = null;
      }

      $response = $this->getResponse();
      $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

      if(isset($result)) {
         $response->setContent(JSON::encode($result));
      }

      return $response;
   }

   public function getScheduleModel()
   {
      if(!$this->scheduleModel) {
         $sm = $this->getServiceLocator();
         $this->scheduleModel = $sm->get('Schedule\Model\ScheduleModel');
      }
      return $this->scheduleModel;
   }
}
