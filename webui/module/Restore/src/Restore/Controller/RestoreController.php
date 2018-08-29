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

namespace Restore\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\View\Model\JsonModel;
use Restore\Model\Restore;
use Restore\Form\RestoreForm;

class RestoreController extends AbstractActionController
{

   /**
    * Variables
    */
   protected $restoreModel = null;
   protected $jobModel = null;
   protected $clientModel = null;
   protected $filesetModel = null;

   protected $bsock = null;
   protected $restore_params = null;

   protected $acl_alert = false;

   private $directories = null;
   private $files = null;

   private $required_commands = array(
      "llist",
      ".filesets",
      ".jobs",
      "restore",
      ".bvfs_update",
      ".bvfs_get_jobids",
      ".bvfs_lsdirs",
      ".bvfs_lsfiles",
      ".bvfs_restore",
      ".bvfs_cleanup"
   );

   /**
    * Index Action
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

      $this->bsock = $this->getServiceLocator()->get('director');

      $this->setRestoreParams();
      $errors = null;
      $result = null;

      if($this->restore_params['client'] == null) {
         try {
            $clients = $this->getClientModel()->getClients($this->bsock);
            $client = array_pop($clients);
            $this->restore_params['client'] = $client['name'];
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      if($this->restore_params['type'] == "client" && $this->restore_params['jobid'] == null) {
         try {
            $latestbackup = $this->getClientModel()->getClientBackups($this->bsock, $this->restore_params['client'], "any", "desc", 1);
            if(empty($latestbackup)) {
               $this->restore_params['jobid'] = null;
            }
            else {
               $this->restore_params['jobid'] = $latestbackup[0]['jobid'];
            }
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      if(isset($this->restore_params['mergejobs']) && $this->restore_params['mergejobs'] == 1) {
         $jobids = $this->restore_params['jobid'];
      }
      else {
         try {
            $jobids = $this->getRestoreModel()->getJobIds($this->bsock, $this->restore_params['jobid'], $this->restore_params['mergefilesets']);
            $this->restore_params['jobids'] = $jobids;
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      if($this->restore_params['type'] == "client") {
         try {
            $backups = $this->getClientModel()->getClientBackups($this->bsock, $this->restore_params['client'], "any", "desc", null);
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      try {
         //$jobs = $this->getJobModel()->getJobs();
         $clients = $this->getClientModel()->getClients($this->bsock);
         $filesets = $this->getFilesetModel()->getDotFilesets($this->bsock);
         $restorejobs = $this->getJobModel()->getRestoreJobs($this->bsock);
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      if(empty($backups)) {
         $errors = 'No backups of client <strong>'.$this->restore_params['client'].'</strong> found.';
      }

      // Create the form
      $form = new RestoreForm(
         $this->restore_params,
         //$jobs,
         $clients,
         $filesets,
         $restorejobs,
         $jobids,
         $backups
      );

      // Set the method attribute for the form
      $form->setAttribute('method', 'post');
      $form->setAttribute('onsubmit','return getFiles()');

      $request = $this->getRequest();

      if($request->isPost()) {

         $restore = new Restore();
         $form->setInputFilter($restore->getInputFilter());
         $form->setData( $request->getPost() );

         if($form->isValid()) {

            if($this->restore_params['type'] == "client") {

               $type = $this->restore_params['type'];
               $jobid = $form->getInputFilter()->getValue('jobid');
               $client = $form->getInputFilter()->getValue('client');
               $restoreclient = $form->getInputFilter()->getValue('restoreclient');
               $restorejob = $form->getInputFilter()->getValue('restorejob');
               $where = $form->getInputFilter()->getValue('where');
               $fileid = $form->getInputFilter()->getValue('checked_files');
               $dirid = $form->getInputFilter()->getValue('checked_directories');
               $jobids = $form->getInputFilter()->getValue('jobids_hidden');
               $replace = $form->getInputFilter()->getValue('replace');

               try {
                  $result = $this->getRestoreModel()->restore($this->bsock, $type, $jobid, $client, $restoreclient, $restorejob, $where, $fileid, $dirid, $jobids, $replace);
               }
               catch(Exception $e) {
                  echo $e->getMessage();
               }

            }

            $this->bsock->disconnect();

            return new ViewModel(array(
               'restore_params' => $this->restore_params,
               'form' => $form,
               'result' => $result,
               'errors' => $errors
            ));

         }
         else {

            $this->bsock->disconnect();

            return new ViewModel(array(
               'restore_params' => $this->restore_params,
               'form' => $form,
               'result' => $result,
               'errors' => $errors
            ));

         }

      }
      else {

         $this->bsock->disconnect();

         return new ViewModel(array(
            'restore_params' => $this->restore_params,
            'form' => $form,
            'result' => $result,
            'errors' => $errors
         ));

      }

   }

   /**
    * Delivers a subtree as Json for JStree
    */
   public function filebrowserAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $this->setRestoreParams();
      $this->layout('layout/json');

      return new ViewModel(array(
         'items' => $this->buildSubtree()
      ));
   }

   /**
    * Get the directories
    */
   private function getDirectories()
   {
      $this->directories = null;

      try {
         if($this->restore_params['type'] == "client") {
            if(empty($this->restore_params['jobid'])) {
               $this->restore_params['jobids'] = null;
            }
            else {
               $jobids = $this->getRestoreModel()->getJobIds($this->bsock, $this->restore_params['jobid'],$this->restore_params['mergefilesets'],$this->restore_params['mergejobs']);
               $this->restore_params['jobids'] = $jobids;
               $this->directories = $this->getRestoreModel()->getDirectories($this->bsock, $this->restore_params['jobids'], $this->restore_params['id']);
            }
         }
         else {
            $this->directories = $this->getRestoreModel()->getDirectories($this->bsock, $this->restore_params['jobid'], $this->restore_params['id']);
         }
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

   }

   /**
    * Get the files
    */
   private function getFiles()
   {
      $this->files = null;

      try {
         if($this->restore_params['type'] == "client") {
            if(empty($this->restore_params['jobid'])) {
               $this->restore_params['jobids'] = null;
            }
            else {
               $jobids = $this->getRestoreModel()->getJobIds($this->bsock, $this->restore_params['jobid'],$this->restore_params['mergefilesets'],$this->restore_params['mergejobs']);
               $this->restore_params['jobids'] = $jobids;
               $this->files = $this->getRestoreModel()->getFiles($this->bsock, $this->restore_params['jobids'], $this->restore_params['id']);
            }
         }
         else {
            $this->files = $this->getRestoreModel()->getFiles($this->bsock, $this->restore_params['jobid'], $this->restore_params['id']);
         }
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

   }

   /**
    * Builds a subtree as Json for JStree
    */
   private function buildSubtree()
   {

      $this->bsock = $this->getServiceLocator()->get('director');
      $this->setRestoreParams();
      $this->getDirectories();
      $this->getFiles();

      $dnum = count($this->directories);
      $fnum = count($this->files);
      $tmp = $dnum;

      // Build Json for JStree
      $items = '[';

      if($dnum > 0) {

         foreach($this->directories as $dir) {
            if($dir['name'] == ".") {
               --$dnum;
               next($this->directories);
            }
            elseif($dir['name'] == "..") {
               --$dnum;
               next($this->directories);
            }
            else {
               --$dnum;
               $items .= '{';
               $items .= '"id":"-' . $dir['pathid'] . '"';
               $items .= ',"text":"' . preg_replace('/[\x00-\x1F\x7F]/', '', str_replace('"', '\"', $dir["name"])) . '"';
               $items .= ',"icon":"glyphicon glyphicon-folder-close"';
               $items .= ',"state":""';
               $items .= ',"data":' . \Zend\Json\Json::encode($dir, \Zend\Json\Json::TYPE_OBJECT);
               $items .= ',"children":true';
               $items .= '}';
               if($dnum > 0) {
                  $items .= ",";
               }
            }
         }

         if($tmp > 0 && $fnum > 0) {
            $items .= ",";
         }

      }

      if($fnum > 0) {

         foreach($this->files as $file) {
            $items .= '{';
            $items .= '"id":"' . $file["fileid"] . '"';
            $items .= ',"text":"' . preg_replace('/[\x00-\x1F\x7F]/', '', str_replace('"', '\"', $file["name"])) . '"';
            $items .= ',"icon":"glyphicon glyphicon-file"';
            $items .= ',"state":""';
            $items .= ',"data":' . \Zend\Json\Json::encode($file, \Zend\Json\Json::TYPE_OBJECT);
            $items .= '}';
            --$fnum;
            if($fnum > 0) {
               $items .= ",";
            }
         }

      }

      $items .= ']';

      $this->bsock->disconnect();

      return $items;
   }

   /**
    * Set the restore parameters
    */
   private function setRestoreParams()
   {
      if($this->params()->fromQuery('type')) {
         $this->restore_params['type'] = $this->params()->fromQuery('type');
      }
      else {
         $this->restore_params['type'] = 'client';
      }

      if($this->params()->fromQuery('jobid')) {
         $this->restore_params['jobid'] = $this->params()->fromQuery('jobid');
      }
      else {
         $this->restore_params['jobid'] = null;
      }

      if($this->params()->fromQuery('client')) {
         $this->restore_params['client'] = $this->params()->fromQuery('client');
      }
      else {
         $this->restore_params['client'] = null;
      }

      if($this->params()->fromQuery('restoreclient')) {
         $this->restore_params['restoreclient'] = $this->params()->fromQuery('restoreclient');
      }
      else {
         $this->restore_params['restoreclient'] = null;
      }

      if($this->params()->fromQuery('restorejob')) {
         $this->restore_params['restorejob'] = $this->params()->fromQuery('restorejob');
      }
      else {
         $this->restore_params['restorejob'] = null;
      }

      if($this->params()->fromQuery('fileset')) {
         $this->restore_params['fileset'] = $this->params()->fromQuery('fileset');
      }
      else {
         $this->restore_params['fileset'] = null;
      }

      if($this->params()->fromQuery('before')) {
         $this->restore_params['before'] = $this->params()->fromQuery('before');
      }
      else {
         $this->restore_params['before'] = null;
      }

      if($this->params()->fromQuery('where')) {
         $this->restore_params['where'] = $this->params()->fromQuery('where');
      }
      else {
         $this->restore_params['where'] = null;
      }

      if($this->params()->fromQuery('fileid')) {
         $this->restore_params['fileid'] = $this->params()->fromQuery('fileid');
      }
      else {
         $this->restore_params['fileid'] = null;
      }

      if($this->params()->fromQuery('dirid')) {
         $this->restore_params['dirid'] = $this->params()->fromQuery('dirid');
      }
      else {
         $this->restore_params['dirid'] = null;
      }

      if($this->params()->fromQuery('id')) {
         $this->restore_params['id'] = $this->params()->fromQuery('id');
      }
      else {
         $this->restore_params['id'] = null;
      }

      if($this->params()->fromQuery('limit')) {
         $this->restore_params['limit'] = $this->params()->fromQuery('limit');
      }
      else {
         $this->restore_params['limit'] = 2000;
      }

      if($this->params()->fromQuery('jobids')) {
         $this->restore_params['jobids'] = $this->params()->fromQuery('jobids');
      }
      else {
         $this->restore_params['jobids'] = null;
      }

      if($this->params()->fromQuery('mergefilesets')) {
         $this->restore_params['mergefilesets'] = $this->params()->fromQuery('mergefilesets');
      }
      else {
         $this->restore_params['mergefilesets'] = 0;
      }

      if($this->params()->fromQuery('mergejobs')) {
         $this->restore_params['mergejobs'] = $this->params()->fromQuery('mergejobs');
      }
      else {
         $this->restore_params['mergejobs'] = 0;
      }

      if($this->params()->fromQuery('replace')) {
         $this->restore_params['replace'] = $this->params()->fromQuery('replace');
      }
      else {
         $this->restore_params['replace'] = null;
      }

      if($this->params()->fromQuery('replaceoptions')) {
         $this->restore_params['replaceoptions'] = $this->params()->fromQuery('replaceoptions');
      }
      else {
         $this->restore_params['replaceoptions'] = null;
      }

      if($this->params()->fromQuery('versions')) {
         $this->restore_params['versions'] = $this->params()->fromQuery('versions');
      }
      else {
         $this->restore_params['versions'] = null;
      }
   }

   /**
    * Get the restore model
    * @return object
    */
   public function getRestoreModel()
   {
      if(!$this->restoreModel) {
         $sm = $this->getServiceLocator();
         $this->restoreModel = $sm->get('Restore\Model\RestoreModel');
      }
      return $this->restoreModel;
   }

   /**
    * Get the job model
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
    * Get the client model
    * @return object
    */
   public function getClientModel()
   {
      if(!$this->clientModel) {
         $sm = $this->getServiceLocator();
         $this->clientModel = $sm->get('CLient\Model\ClientModel');
      }
      return $this->clientModel;
   }

   /**
    * Get the fileset model
    * @return object
    */
   public function getFilesetModel()
   {
      if(!$this->filesetModel) {
         $sm = $this->getServiceLocator();
         $this->filesetModel = $sm->get('Fileset\Model\FilesetModel');
      }
      return $this->filesetModel;
   }
}
