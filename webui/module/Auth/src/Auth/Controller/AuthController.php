<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
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

      if($this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
      }

      $this->layout('layout/login');

      $config = $this->getServiceLocator()->get('Config');
      $dird = $this->params()->fromQuery('dird') ? $this->params()->fromQuery('dird') : null;

      $form = new LoginForm($config['directors'], $dird);

      $request = $this->getRequest();

      if(!$request->isPost()) {
         return $this->createNewLoginForm($form);
      }

      $auth = new Auth();
      $form->setInputFilter($auth->getInputFilter());
      $form->setData($request->getPost());
      if(!$form->isValid()) {
         // given credentials in login form could not be validated in this case
         $err_msg = "Please provide a director, username and password.";
         return $this->createNewLoginForm($form,$err_msg);
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
         return $this->createNewLoginForm($form,$err_msg,$this->bsock);
      } 

      $_SESSION['bareos']['director'] = $director;
      $_SESSION['bareos']['username'] = $username;
      $_SESSION['bareos']['password'] = $password;
      $_SESSION['bareos']['authenticated'] = true;
      $_SESSION['bareos']['locale'] = $locale;
      $_SESSION['bareos']['idletime'] = time();
      $_SESSION['bareos']['product-updates'] = $bareos_updates;
      $_SESSION['bareos']['dird-update-available'] = false;

      // Check if .api command is allowed and version 2 is available
      $debug = $this->getDirectorModel()->sendDirectorCommand($this->bsock, ".api 2 compact=yes");
      if(preg_match("/.api:/",$debug)) {
         $err_msg = 'Sorry, the user you are trying to login with has no permissions for the .api command. For further information, please read the <a href="https://docs.bareos.org/IntroductionAndTutorial/InstallingBareosWebui.html#configuration-of-profile-resources" target="_blank">Bareos documentation</a>.';
         return $this->createNewLoginForm($form,$err_msg,$this->bsock);
      }
      elseif(!preg_match('/result/', $debug)) {
         $err_msg = 'Error: API 2 not available on 15.2.2 or greater and/or compile with jansson support.';
         return $this->createNewLoginForm($form,$err_msg,$this->bsock);
      }

      if(isset($bareos_updates) && $bareos_updates == false) {
         // updates could not be retrieved by ajax call
         $_SESSION['bareos']['product-updates-status'] = false;
         return null;
      }

      $_SESSION['bareos']['product-updates-status'] = true;
      $updates = json_decode($bareos_updates, true);

      try {
         $dird_version = $this->getDirectorModel()->getDirectorVersion($this->bsock);
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
      if(array_key_exists('version', $dird_version)) {
         $dird_vers = $dird_version['version'];
      }
      if(isset($dird_dist) && isset($dird_arch) && isset($dird_vers)) {
         if(array_key_exists('product', $updates) &&
            array_key_exists($dird_dist, $updates['product']['bareos-director']['distribution']) &&
            array_key_exists($dird_arch, $updates['product']['bareos-director']['distribution'][$dird_dist])) {
            foreach($updates['product']['bareos-director']['distribution'][$dird_dist][$dird_arch] as $key => $value) {
               if( version_compare($dird_vers, $key, '>=') ) {
                  $_SESSION['bareos']['dird-update-available'] = false;
               }
               if( version_compare($dird_vers, $key, '<') ) {
                  $_SESSION['bareos']['dird-update-available'] = true;
               }
            }
         }
      }

      // Get available commands
      try {
         $commands = $this->getDirectorModel()->getAvailableCommands($this->bsock);
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }
      // Push available commands into SESSION context.
      $_SESSION['bareos']['commands'] = $commands;
      // Check if Command ACL has the minimal requirements
      if($_SESSION['bareos']['commands']['.help']['permission'] == 0) {
         $err_msg = 'Sorry, your Command ACL does not fit the minimal requirements. For further information, please read the <a href="https://docs.bareos.org/IntroductionAndTutorial/InstallingBareosWebui.html#configuration-of-profile-resources" target="_blank">Bareos documentation</a>.';
         return $this->createNewLoginForm($form,$err_msg,$this->bsock);
      }
      // Get the config.
      $configuration = $this->getServiceLocator()->get('configuration');
      // Push the datatable settings into the SESSION context.
      $_SESSION['bareos']['dt_lengthmenu'] = $configuration['configuration']['tables']['pagination_values'];
      $_SESSION['bareos']['dt_pagelength'] = $configuration['configuration']['tables']['pagination_default_value'];
      $_SESSION['bareos']['dt_statesave'] = ($configuration['configuration']['tables']['save_previous_state']) ? 'true' : 'false';
      // Push the autochanger settings into the SESSION context.
      if(isset($configuration['configuration']['autochanger']['labelpooltype'])) {
         $_SESSION['bareos']['ac_labelpooltype'] = $configuration['configuration']['autochanger']['labelpooltype'];
      }
      // Push dashboard configuration settings into SESSION context.
      $_SESSION['bareos']['dashboard_autorefresh_interval'] = $configuration['configuration']['dashboard']['autorefresh_interval'];
      // Push restore configuration settings into SESSION context.
      $_SESSION['bareos']['filetree_refresh_timeout'] = $configuration['configuration']['restore']['filetree_refresh_timeout'];

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
      // todo - ask user if he's really wants to log out!
      unset($_SESSION['bareos']);
      session_destroy();
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
   private function createNewLoginForm($form, $err_msg = null, $bsock = null)
   {
      if ($bsock != null) {
         $bsock->disconnect();
      }
      session_destroy();
      return new ViewModel(
         array(
            'form' => $form,
            'err_msg' => $err_msg,
         )
      );
   }
}
