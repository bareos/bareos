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

      if($request->isPost()) {
         $auth = new Auth();
         $form->setInputFilter($auth->getInputFilter());
         $form->setData($request->getPost());

         if($form->isValid()) {
            $director = $form->getInputFilter()->getValue('director');
            $username = $form->getInputFilter()->getValue('consolename');
            $password = $form->getInputFilter()->getValue('password');
            $locale = $form->getInputFilter()->getValue('locale');
            $bareos_updates = $form->getInputFilter()->getValue('bareos_updates');
            $rememberme = $form->getInputFilter()->getValue('remember_me');

            $config = $this->getServiceLocator()->get('Config');

            $this->bsock = $this->getServiceLocator()->get('director');
            $this->bsock->set_config($config['directors'][$director]);
            $this->bsock->set_user_credentials($username, $password);

            if($this->bsock->auth($username, $password)) {
               $_SESSION['bareos']['director'] = $director;
               $_SESSION['bareos']['username'] = $username;
               $_SESSION['bareos']['password'] = $password;
               $_SESSION['bareos']['authenticated'] = true;
               $_SESSION['bareos']['locale'] = $locale;
               $_SESSION['bareos']['idletime'] = time();
               $_SESSION['bareos']['product-updates'] = $bareos_updates;
               $_SESSION['bareos']['dird-update-available'] = false;

               if($rememberme == '1'){
                  $_SESSION['bareos']['rememberme'] = true;
               }
               else{
                  $_SESSION['bareos']['rememberme'] = false;
               }

               if(isset($bareos_updates) && $bareos_updates != false) {
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

                  if(array_key_exists('obsarch', $dird_version)) {
                     if(preg_match("/ubuntu/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
                        $dird_arch = "amd64";
                     }
                     elseif(preg_match("/debian/i", $dird_dist) && $dird_version['obsarch'] == "x86_64") {
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
               }
               else {
                  // updates could not be retrieved by ajax call
                  $_SESSION['bareos']['product-updates-status'] = false;
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
                  $this->bsock->disconnect();
                  session_destroy();
                  $err_msg = 'Sorry, your Command ACL does not fit the minimal requirements. For further information, please read the <a href="http://doc.bareos.org/master/html/bareos-manual-main-reference.html" target="_blank">Bareos documentation</a>.';
                  return new ViewModel(
                     array(
                        'form' => $form,
                        'err_msg' => $err_msg,
                     )
                  );
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

               if($this->params()->fromQuery('req')) {
                  $redirect = $this->params()->fromQuery('req');
                  $request = $this->getRequest();
                  $request->setUri($redirect);
                  if($routeToBeMatched = $this->getServiceLocator()->get('Router')->match($request)) {
                     return $this->redirect()->toUrl($this->params()->fromQuery('req'));
                  }
                  return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
               }
               else {
                  return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
               }

               $this->bsock->disconnect();
            } else {
               $this->bsock->disconnect();
               session_destroy();

               $err_msg = "Sorry, can not authenticate. Wrong username and/or password.";

               return new ViewModel(
                  array(
                     'form' => $form,
                     'err_msg' => $err_msg,
                  )
               );
            }
         } else {
            // given credentials in login form could not be validated in this case
            $err_msg = "Please provide a director, username and password.";

            session_destroy();

            return new ViewModel(
                  array(
                     'form' => $form,
                     'err_msg' => $err_msg,
                  )
            );
         }
      }

      return new ViewModel(
         array(
            'form' => $form,
         )
      );

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

}
