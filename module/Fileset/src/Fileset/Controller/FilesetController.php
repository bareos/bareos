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

namespace Fileset\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class FilesetController extends AbstractActionController
{

   protected $filesetModel = null;
   protected $bsock = null;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $filesets = $this->getFilesetModel()->getFilesets($this->bsock);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'filesets' => $filesets,
            )
      );
   }

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $filesetid = $this->params()->fromRoute('id', 0);

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $fileset = $this->getFilesetModel()->getFileset($this->bsock, $filesetid);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'fileset' => $fileset
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
      $fileset = $this->params()->fromQuery('fileset');

      if($data == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getFilesetModel()->getFilesets($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "details" && isset($fileset)) {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getFilesetModel()->getFileset($this->bsock, $fileset);
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

   public function getFilesetModel()
   {
      if(!$this->filesetModel) {
         $sm = $this->getServiceLocator();
         $this->filesetModel = $sm->get('Fileset\Model\FilesetModel');
      }
      return $this->filesetModel;
   }

}

