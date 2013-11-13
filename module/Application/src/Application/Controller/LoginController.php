<?php

namespace Application\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\Authentication\Adapter\DbTable;
use Zend\Session\Container as SessionContainer;
use Zend\View\Model\ViewModel;
use Application\Model\User;
use Application\Form\Login;

class LoginController extends AbstractActionController
{

    public function loginAction() 
    {
	$authService = $this->serviceLocator->get('auth_service');
	
	if($authService->hasIdentity()) {
	    return $this->redirect()->toUrl('/dashboard');
	}
	
	$form = new Login;
	
	$loginMsg = array();
	
	if($this->getRequest()->isPost()) {
	    
	    $form->setData($this->getRequest()->getPost());
	    
	    if(! $form->isValid()) {
		return new ViewModel(
		    array(
			'title' => 'Log in',
			'form' => $form
		    )
		);
	    }
	    
	    $dbAdapter = $this->serviceLocator->get('Zend\Db\Adapter\Adapter');
	    $loginData = $form->getData();
	    
	    $authAdapter = new DbTable($dbAdapter, 'user', 'username', 'password', 'MD5(?)');
	    $authAdapter->setIdentity($loginData['username']);
	    $authAdapter->setCredential($loginData['password']);
	    
	    $authService = $this->serviceLocator->get('auth_service');
	    $authService->setAdapter($authAdapter);
	    
	    $result = $authService->authenticate();
	    
	    if($result->isValid()) {
		// set id as identifier in session
		$userId = $authAdapter->getResultRowObject('id')->id;
		$authService->getStorage()->write($userId);
		return $this->redirect()->toUrl('/dashboard');
	    }
	    else {
		$loginMsg = $result->getMessages();
	    }
	    
	}
	
	return new ViewModel(
	    array(
		'title' => 'Log in',
		'form' => $form,
		'loginMsg' => $loginMsg
	    )
	);
	
    }

    public function logoutAction()
    {
	$authService = $this->serviceLocator->get('auth_service');
	
	if(! $authService->hasIdentity()) {
	    // if not logged in, redirect to login page
	    return $this->redirect()->toUrl('/login');
	}
	
	$authService->clearIdentity();
	$form = new Login();
	
	$viewModel = new ViewModel(
	    array(
		'loginMsg' => array('You have been logged out'),
		'form' => $form,
		'title' => 'Log out'
	    )
	);
	
	$viewModel->setTemplate('application/login/login.phtml');
	
	return $viewModel;
	
    }
      
    public function getUserTable() 
    {
    
	if(! $this->userTable) {
	    $sm = $this->getServiceLocator();
	    $this->userTable = $sm->get('Application\Model\UserTable');
	}
    
	return $this->userTable;
    
    }  
      
}
