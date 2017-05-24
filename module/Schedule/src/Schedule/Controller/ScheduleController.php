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

   /**
    * Variables
    */
   protected $scheduleModel = null;
   protected $bsock = null;
   protected $acl_alert = false;

   private $required_commands = array(
      ".schedule",
      "show",
      "status",
      "enable",
      "disable"
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

      $result = null;

      $action = $this->params()->fromQuery('action');
      $schedulename = $this->params()->fromQuery('schedule');

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      if(empty($action)) {
         try {
            $schedules = $this->getScheduleModel()->getSchedules($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
         return new ViewModel(
            array(
               'schedules' => $schedules
            )
         );
      }
      else {
         if($action == "enable") {
            try {
               $schedules = $this->getScheduleModel()->getSchedules($this->bsock);
               $result = $this->getScheduleModel()->enableSchedule($this->bsock, $schedulename);
            }
            catch(Exception $e) {
               echo $e->getMessage();
            }
         }
         elseif($action == "disable") {
            try {
               $schedules = $this->getScheduleModel()->getSchedules($this->bsock);
               $result = $this->getScheduleModel()->disableSchedule($this->bsock, $schedulename);
            }
            catch(Exception $e) {
               echo $e->getMessage();
            }
         }

         try {
            $this->bsock->disconnect();
            }
         catch(Exception $e) {
            echo $e->getMessage();
         }

         return new ViewModel(
            array(
               'schedules' => $schedules,
               'result' => $result
            )
         );
      }
   }

   /**
    * Overview Action
    *
    * @return object
    */
   public function overviewAction()
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

      $result = null;

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getScheduleModel()->showSchedules($this->bsock);
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

   /**
    * Status Action
    *
    * @return object
    */
   public function statusAction()
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

      $result = null;

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getScheduleModel()->getFullScheduleStatus($this->bsock);
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

   /**
    * Details Action
    *
    * @return object
    */
   public function detailsAction()
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

      $result = null;

      $schedulename = $this->params()->fromQuery('schedule');

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getScheduleModel()->getScheduleStatus($this->bsock, $schedulename);
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
      $schedule = $this->params()->fromQuery('schedule');

      if($data == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getScheduleModel()->getSchedules($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "details" && isset($schedule)) {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getScheduleModel()->getSchedule($this->bsock, $schedule);
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
    * Get Schedule Model
    *
    * @return object
    */
   public function getScheduleModel()
   {
      if(!$this->scheduleModel) {
         $sm = $this->getServiceLocator();
         $this->scheduleModel = $sm->get('Schedule\Model\ScheduleModel');
      }
      return $this->scheduleModel;
   }
}
