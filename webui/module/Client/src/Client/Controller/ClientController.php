<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
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

   /**
    * Variables
    */
   protected $clientModel = null;
   protected $directorModel = null;
   protected $bsock = null;
   protected $acl_alert = false;

   private $required_commands = array(
      "llist",
      "status",
      "enable",
      "disable",
      "version"
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

      if(empty($action)) {
         return new ViewModel();
      }
      else {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }

         if($action == "enable") {
            $clientname = $this->params()->fromQuery('client');
            try {
               $result = $this->getClientModel()->enableClient($this->bsock, $clientname);
            }
            catch(Exception $e) {
               echo $e->getMessage();
            }
         }
         elseif($action == "disable") {
            $clientname = $this->params()->fromQuery('client');
            try {
               $result = $this->getClientModel()->disableClient($this->bsock, $clientname);
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
               'result' => $result
            )
         );
      }
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

      return new ViewModel(
         array(
            'client' => $this->params()->fromRoute('id')
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
            $clients = $this->getClientModel()->getClients($this->bsock);
            $dot_clients = $this->getClientModel()->getDotClients($this->bsock);
            $dird_version = $this->getDirectorModel()->getDirectorVersion($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }

         $bareos_updates = json_decode($_SESSION['bareos']['product-updates'], true);

         if(array_key_exists('obsdistribution', $dird_version)) {
            $dird_dist = $dird_version['obsdistribution'];
         }
         else {
            $dird_dist = null;
         }

         if(array_key_exists('obsarch', $dird_version)) {
            if(preg_match("/debian/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
               $dird_arch = "amd64";
            }
            elseif(preg_match("/ubuntu/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
               $dird_arch = "amd64";
            }
            elseif(preg_match("/windows/i", $dird_dist) && $dird_version['obsarch'] == "Win32") {
               $dird_arch = "32";
            }
            elseif(preg_match("/windows/i", $dird_dist) && $dird_version['obsarch'] == "Win64") {
               $dird_arch = "64";
            }
            else {
               $dird_arch = $dird_version['obsarch'];
            }
         }
         else {
            $dird_arch = null;
         }

         if(array_key_exists('version', $dird_version)) {
            $dird_vers = $dird_version['version'];
         }
         else {
            $dird_vers = null;
         }

         $result = array();
         for($i = 0; $i < count($clients); $i++) {
            $result[$i]['clientid'] = $clients[$i]['clientid'];
            $result[$i]['uname'] = $clients[$i]['uname'];
            $result[$i]['name'] = $clients[$i]['name'];
            $result[$i]['autoprune'] = $clients[$i]['autoprune'];
            $result[$i]['fileretention'] = $clients[$i]['fileretention'];
            $result[$i]['jobretention'] = $clients[$i]['jobretention'];
            $result[$i]['installed_fd'] = "";
            $result[$i]['available_fd'] = "";
            $result[$i]['installed_dird'] = "";
            $result[$i]['available_dird'] = "";
            $result[$i]['enabled'] = "";

            for($j = 0; $j < count($dot_clients); $j++) {
               if($result[$i]['name'] == $dot_clients[$j]['name']) {
                  $result[$i]['enabled'] = $dot_clients[$j]['enabled'];
               }
            }

            if(isset($dird_vers)) {
               $result[$i]['installed_dird'] = $dird_vers;
            }
            else {
               $result[$i]['installed_dird'] = "";
            }

            if(isset($bareos_updates) && $bareos_updates != NULL) {
               if(array_key_exists('product', $bareos_updates) &&
                  array_key_exists($dird_dist, $bareos_updates['product']['bareos-director']['distribution']) &&
                  array_key_exists($dird_arch, $bareos_updates['product']['bareos-director']['distribution'][$dird_dist])) {
                  foreach($bareos_updates['product']['bareos-director']['distribution'][$dird_dist][$dird_arch] as $key => $value) {
                     if( version_compare($dird_vers, $key, '>=') ) {
                        $result[$i]['update_dird'] = false;
                        $result[$i]['available_dird'] = $key;
                     }
                     if( version_compare($dird_vers, $key, '<') ) {
                        $result[$i]['update_dird'] = true;
                        $result[$i]['available_dird'] = $key;
                     }
                  }
               }
            }
            else {
               $result[$i]['update_dird'] = false;
               $result[$i]['available_dird'] = NULL;
            }

            $uname = explode(",", $clients[$i]['uname']);
            $v = explode(" ", $uname[0]);
            $fd_vers = $v[0];

            if(array_key_exists(3, $uname)) {
               $fd_dist = $uname[3];
            }
            else {
               $fd_dist = "";
            }

            if(array_key_exists(4, $uname)) {
               if(preg_match("/debian/i", $fd_dist) && $uname[4] == "x86_64") {
                  $fd_arch = "amd64";
               }
               elseif(preg_match("/ubuntu/i", $fd_dist) && $uname[4] == "x86_64") {
                  $fd_arch = "amd64";
               }
               elseif(preg_match("/windows/i", $fd_dist) && $uname[4] == "Win32") {
                  $fd_arch = "32";
               }
               elseif(preg_match("/windows/i", $fd_dist) && $uname[4] == "Win64") {
                  $fd_arch = "64";
               }
               else {
                  $fd_arch = $uname[4];
               }
            }
            else {
               $fd_arch = NULL;
            }

            $result[$i]['installed_fd'] = $fd_vers;

            if(isset($bareos_updates) && $bareos_updates != NULL) {
               if(array_key_exists('product', $bareos_updates) &&
                  array_key_exists($fd_dist, $bareos_updates['product']['bareos-filedaemon']['distribution']) &&
                  array_key_exists($fd_arch, $bareos_updates['product']['bareos-filedaemon']['distribution'][$fd_dist])) {
                  foreach($bareos_updates['product']['bareos-filedaemon']['distribution'][$fd_dist][$fd_arch] as $key => $value) {
                     if( version_compare($fd_vers, $key, '>=') ) {
                        $result[$i]['available_fd'] = $key;
                        $result[$i]['update_fd'] = false;
                     }
                     if( version_compare($fd_vers, $key, '<') ) {
                        if( version_compare($key, $dird_version['version'], '<=') ) {
                           $result[$i]['available_fd'] = $key;
                           $result[$i]['update_fd'] = true;
                           $result[$i]['url_package'] = $bareos_updates['product']['bareos-filedaemon']['distribution'][$fd_dist][$fd_arch][$key]['url_package'];
                           break;
                        }
                        elseif( version_compare($key, $dird_version['version'], '>') ) {
                           $result[$i]['available_fd'] = $key;
                           $result[$i]['update_fd'] = false;
                           break;
                        }
                     }
                  }
               }
            }
            else {
               $result[$i]['available_fd'] = NULL;
               $result[$i]['update_fd'] = false;
            }
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
            $result = $this->getClientModel()->getClientBackups($this->bsock, $client, null, 'desc', null);
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
    * Get Client Model
    *
    * @return object
    */
   public function getClientModel()
   {
      if(!$this->clientModel) {
         $sm = $this->getServiceLocator();
         $this->clientModel = $sm->get('Client\Model\ClientModel');
      }
      return $this->clientModel;
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
}
