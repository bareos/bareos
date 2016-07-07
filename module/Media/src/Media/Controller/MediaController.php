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

namespace Media\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class MediaController extends AbstractActionController
{
   protected $mediaModel = null;
   protected $bsock = null;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $volumes = $this->getMediaModel()->getVolumes($this->bsock);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'volumes' => $volumes,
         )
      );
   }

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $volumename = $this->params()->fromRoute('id');

      return new ViewModel(array(
         'volume' => $volumename,
      ));
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $data = $this->params()->fromQuery('data');
      $volume = $this->params()->fromQuery('volume');

      if($data == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getMediaModel()->getVolumes($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "details") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            // workaround until llist volume returns array instead of object
            $result[0] = $this->getMediaModel()->getVolume($this->bsock, $volume);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "jobs") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getMediaModel()->getVolumeJobs($this->bsock, $volume);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      $response = $this->getResponse();
      $response->getHeaders()->addHeaderLine('Content-Type','application/json');

      if(isset($result)) {
         $response->setContent(JSON::encode($result));
      }

      return $response;
   }

   public function getMediaModel()
   {
      if(!$this->mediaModel) {
         $sm = $this->getServiceLocator();
         $this->mediaModel = $sm->get('Media\Model\MediaModel');
      }
      return $this->mediaModel;
   }

}
