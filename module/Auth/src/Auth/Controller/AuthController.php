<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

	protected $bsock;

	public function indexAction()
	{
		return new ViewModel();
	}

	public function loginAction()
	{

		if(isset($_SESSION['bareos']['authenticated']) && $_SESSION['bareos']['authenticated']) {
			return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
		}

		$this->layout('layout/login');

		$config = $this->getServiceLocator()->get('Config');

		$form = new LoginForm($config['directors']);
		$form->get('submit')->setValue('Login');

		$request = $this->getRequest();

		if($request->isPost()) {

			$auth = new Auth();
			$form->setInputFilter($auth->getInputFilter());
			$form->setData($request->getPost());

			if($form->isValid()) {

				$director = $form->getInputFilter()->getValue('director');
				$username = $form->getInputFilter()->getValue('consolename');
				$password = $form->getInputFilter()->getValue('password');

				$config = $this->getServiceLocator()->get('Config');
				$this->bsock = $this->getServiceLocator()->get('bsock');
				$this->bsock->set_config($config['directors'][$director]);
				$this->bsock->set_user_credentials($username, $password);

				if($this->bsock->auth($username, $password)) {
					//$session = new Container('user');
					$_SESSION['bareos']['director'] = $director;
					$_SESSION['bareos']['username'] = $username;
					$_SESSION['bareos']['password'] = $password;
					$_SESSION['bareos']['authenticated'] = true;
					$_SESSION['bareos']['socket'] = $this->bsock;
					$this->bsock->disconnect();
					return $this->redirect()->toRoute('dashboard', array('action' => 'index'));
				} else {

					// todo - give user a message about what went wrong

					// if we get false, wrong credentials(username, password) in this case, pass a message in auth action login and display
					// if we get a exception the socket could not be initialized for whatever reason

					$this->bsock->disconnect();
					session_destroy();
					//return $this->redirect()->toRoute('auth', array('action' => 'login'));
					$err_msg = "Sorry, can not authenticate. Wrong username and/or password.";
					return new ViewModel(
						array(
							'form' => $form,
							'err_msg' => $err_msg,
						)
					);
				}

			} else {

				// todo - give user a message about what went wrong

				// given credentials in login form could not be validated in this case
				$err_msg = "Please provide a director, username and password.";

				//$this->bsock->disconnect();
				session_destroy();
				//return $this->redirect()->toRoute('auth', array('action' => 'login'));

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

	public function logoutAction()
	{

		// todo - ask user if he's really sure to logout!
		// "Do you really want to logout?"

		unset($_SESSION['user']);
		session_destroy();
		//$this->bsock = $this->getServiceLocator()->get('bsock');
		//$this->bsock->disconnect();
		return $this->redirect()->toRoute('auth', array('action' => 'login'));
	}

}
