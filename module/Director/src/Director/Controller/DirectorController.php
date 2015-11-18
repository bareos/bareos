<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 * @author    Frank Bergkemper
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

namespace Director\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DirectorController extends AbstractActionController
{
   protected $directorModel;

   public function indexAction()
   {
      if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
         return new ViewModel(array(
               'directorOutput' => $this->getDirectorModel()->getDirectorStatus()
            )
         );
      }
      else {
         return $this->redirect()->toRoute('auth', array('action' => 'login'));
      }
   }

   public function messagesAction()
   {
      if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
         return new ViewModel(array(
               'directorOutput' => $this->getDirectorModel()->getDirectorMessages()
            )
         );
      }
      else {
         return $this->redirect()->toRoute('auth', array('action' => 'login'));
      }
   }

   public function scheduleAction()
   {
      if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
         return new ViewModel(array(
               'directorOutput' => $this->getDirectorModel()->getDirectorSchedules()
            )
         );
      }
      else {
         return $this->redirect()->toRoute('auth', array('action' => 'login'));
      }
   }

   public function schedulerstatusAction()
   {
      if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
         return new ViewModel(array(
               'directorOutput' => $this->getDirectorModel()->getDirectorSchedulerStatus()
            )
         );
      }
      else {
         return $this->redirect()->toRoute('auth', array('action' => 'login'));
      }
   }

   public function versionAction()
   {
      if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
         return new ViewModel(array(
               'directorOutput' => $this->getDirectorModel()->getDirectorVersion()
            )
         );
      }
      else {
         return $this->redirect()->toRoute('auth', array('action' => 'login'));
      }
   }

   public function getDirectorModel()
   {
      if(!$this->directorModel) {
         $sm = $this->getServiceLocator();
         $this->directorModel = $sm->get('Director\Model\DirectorModel');
      }
      return $this->directorModel;
   }
}
