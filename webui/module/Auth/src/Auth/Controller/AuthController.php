<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2020 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Auth\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Auth\Model\Auth;
use Auth\Form\LoginForm;
use Zend\Session\Container;

class AuthController extends AbstractActionController
{

   /**
    * Variables
    */
   protected $directorModel = null;
   protected $bsock = null;
   protected $updates = null;

   /**
    * Index Action
    *
    * @return object
    */
   public function indexAction()
   {
      return new ViewModel();
   }

   /**
    * Login Action
    *
    * @return object
    */
   public function loginAction()
   {
      $multi_dird_env = false;

      if($this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
      }

      $this->layout('layout/login');

      $config = $this->getServiceLocator()->get('Config');
      $dird = $this->params()->fromQuery('dird') ? $this->params()->fromQuery('dird') : null;

      if(count($config['directors']) > 1) {
         $multi_dird_env = true;
      }

      $form = new LoginForm($config['directors'], $dird);

      $request = $this->getRequest();

      if(!$request->isPost()) {
         return $this->createNewLoginForm($form, $multi_dird_env);
      }

      $auth = new Auth();
      $form->setInputFilter($auth->getInputFilter());
      $form->setData($request->getPost());

      if(!$form->isValid()) {
         $err_msg = "Please provide a director, username and password.";
         return $this->createNewLoginForm($form, $multi_dird_env, $err_msg);
      }

      $director = $form->getInputFilter()->getValue('director');
      $username = $form->getInputFilter()->getValue('consolename');
      $password = $form->getInputFilter()->getValue('password');
      $locale = $form->getInputFilter()->getValue('locale');
      $bareos_updates = $form->getInputFilter()->getValue('bareos_updates');

      $config = $this->getServiceLocator()->get('Config');
      $this->bsock = $this->getServiceLocator()->get('director');
      $this->bsock->set_config($config['directors'][$director]);
      $this->bsock->set_user_credentials($username, $password);

      if(!$this->bsock->connect_and_authenticate()) {
         $err_msg = "Sorry, can not authenticate. Wrong username and/or password.";
         return $this->createNewLoginForm($form, $multi_dird_env, $err_msg, $this->bsock);
      }

      $session = new Container('bareos');
      $session->offsetSet('director', $director);
      $session->offsetSet('username', $username);
      $session->offsetSet('password', $password);
      $session->offsetSet('authenticated', true);
      $session->offsetSet('locale', $locale);
      $session->offsetSet('idletime', time());
      $session->offsetSet('product-updates', $bareos_updates);
      $session->offsetSet('product-updates-status', false);
      $session->offsetSet('dird-update-available', false);

      $configuration = $this->getServiceLocator()->get('configuration');

      $session->offsetSet('dt_lengthmenu', $configuration['configuration']['tables']['pagination_values']);
      $session->offsetSet('dt_pagelength', $configuration['configuration']['tables']['pagination_default_value']);
      $session->offsetSet('dt_statesave', ($configuration['configuration']['tables']['save_previous_state']) ? 'true' : 'false');
      $session->offsetSet('dashboard_autorefresh_interval', $configuration['configuration']['dashboard']['autorefresh_interval']);
      $session->offsetSet('filetree_refresh_timeout', $configuration['configuration']['restore']['filetree_refresh_timeout']);

      if(isset($configuration['configuration']['autochanger']['labelpooltype'])) {
         $session->offsetSet('ac_labelpooltype', $configuration['configuration']['autochanger']['labelpooltype']);
      }

      if($bareos_updates != "false" &&
         !preg_match('/"statusText":"timeout"/', $bareos_updates)) {
            $session->offsetSet('product-updates-status', true);
            $this->updates = json_decode($bareos_updates, true);
            $session->offsetSet('dird-update-available', $this->checkUpdateStatusDIRD());
      }

      $apicheck = $this->checkAPIStatusDIRD();

      if(!$apicheck) {
         return $this->createNewLoginForm($form, $multi_dird_env, $apicheck, $this->bsock);
      }

      $aclcheck = $this->checkACLStatusDIRD();

      if(!$aclcheck) {
         return $this->createNewLoginForm($form, $multi_dird_env, $aclcheck, $this->bsock);
      } else {
         $session->offsetSet('commands', $aclcheck);
      }

      if($this->params()->fromQuery('req')) {
         $redirect = $this->params()->fromQuery('req');
         $request = $this->getRequest();
         $request->setUri($redirect);
         if($routeToBeMatched = $this->getServiceLocator()->get('Router')->match($request)) {
            return $this->redirect()->toUrl($this->params()->fromQuery('req'));
         }
      }

      return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
   }

   /**
    * Logout Action
    *
    * @return object
    */
   public function logoutAction()
   {
      $session = new Container('bareos');
      $session->getManager()->destroy();
      return $this->redirect()->toRoute('auth', array('action' => 'login'));
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

   /**
    * Create New Login Form
    *
    * @return object
    */
   private function createNewLoginForm($form, $multi_dird_env = null, $err_msg = null, $bsock = null)
   {
      if ($bsock != null) {
         $bsock->disconnect();
      }
      session_destroy();
      return new ViewModel(
         array(
            'form' => $form,
            'multi_dird_env' => $multi_dird_env,
            'err_msg' => $err_msg
         )
      );
   }

   /**
    * DIRD API check
    *
    * @return mixed
    */
   private function checkAPIStatusDIRD() {

      $err_msg_1 = 'Sorry, the user you are trying to login with has no permissions for the .api command. For further information, please read the <a href="https://docs.bareos.org/IntroductionAndTutorial/InstallingBareosWebui.html#configuration-of-profile-resources" target="_blank">Bareos documentation</a>.';
      $err_msg_2 = 'Error: API 2 not available on 15.2.2 or greater and/or compile with jansson support.';

      $result = $this->getDirectorModel()->sendDirectorCommand($this->bsock, ".api 2 compact=yes");

      if(preg_match("/.api:/", $result)) {
         return $err_msg_1;
      }

      if(preg_match("/result/", $result)) {
         return $err_msg_2;
      }

      return true;
   }

   /**
    * DIRD ACL check
    *
    * @return mixed
    */
   private function checkACLStatusDIRD() {

      $err_msg = 'Sorry, your Command ACL does not fit the minimal requirements. For further information, please read the <a href="https://docs.bareos.org/IntroductionAndTutorial/InstallingBareosWebui.html#configuration-of-profile-resources" target="_blank">Bareos documentation</a>.';

      try {
         $commands = $this->getDirectorModel()->getAvailableCommands($this->bsock);
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      if($commands['.help']['permission'] == 0) {
         return $err_msg;
      }

      return $commands;
   }

   /**
    * DIRD update check
    *
    * @return boolean
    */
   private function checkUpdateStatusDIRD() {

      $dird_version = null;
      $dird_dist = null;

      try {
         $dird_version = $this->getDirectorModel()->getDirectorVersion($this->bsock);
         if(array_key_exists('version', $dird_version)) {
            $dird_vers = $dird_version['version'];
         }
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      if(array_key_exists('obsdistribution', $dird_version)) {
         $dird_dist = $dird_version['obsdistribution'];
      }

      if(!array_key_exists('obsarch', $dird_version)) {
         $dird_arch = null;
      }

      if($dird_dist !== null) {
         if(preg_match("/ubuntu/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
            $dird_arch = "amd64";
         }
         elseif(preg_match("/debian/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
            $dird_arch = "amd64";
         }
         elseif(preg_match("/univention/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
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

         if(isset($dird_arch) && isset($dird_vers)) {
            if(array_key_exists('product', $this->updates) &&
               array_key_exists($dird_dist, $this->updates['product']['bareos-director']['distribution']) &&
               array_key_exists($dird_arch, $this->updates['product']['bareos-director']['distribution'][$dird_dist])) {
               foreach($this->updates['product']['bareos-director']['distribution'][$dird_dist][$dird_arch] as $key => $value) {
                  if( version_compare($dird_vers, $key, '>=') ) {
                     return false;
                  }
                  if( version_compare($dird_vers, $key, '<') ) {
                     return true;
                  }
               }
            }
         }
      }

      return false;
   }

}
