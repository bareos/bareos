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

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class ClientController extends AbstractActionController
{

   protected $clientModel = null;
   protected $bsock = null;

   public function indexAction()
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
      elseif($action == "enable") {
         $clientname = $this->params()->fromQuery('client');
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->enableClient($this->bsock, $clientname);
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
         $clientname = $this->params()->fromQuery('client');
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->disableClient($this->bsock, $clientname);
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

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      return new ViewModel(
         array(
            'client' => $this->params()->fromRoute('id')
         )
      );
   }

   public function statusAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $clientname = $this->params()->fromQuery('client');

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getClientModel()->statusClient($this->bsock, $clientname);
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

   public function getDataAction()
   {

      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $data = $this->params()->fromQuery('data');
      $client = $this->params()->fromQuery('client');

      if($data == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->getClients($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "details" && isset($client)) {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->getClient($this->bsock, $client);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "backups" && isset($client)) {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->getClientBackups($this->bsock, $client, null, 'desc');
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

   public function getClientModel()
   {
      if(!$this->clientModel) {
         $sm = $this->getServiceLocator();
         $this->clientModel = $sm->get('Client\Model\ClientModel');
      }
      return $this->clientModel;
   }
}
