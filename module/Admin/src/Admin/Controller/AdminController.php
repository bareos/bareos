<?php

namespace Admin\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class AdminController extends AbstractActionController
{

	protected $adminTable;

	public function indexAction()
	{
		return new ViewModel();
	}

	public function rolesAction() 
	{
		return new ViewModel();
	}

	public function usersAction() 
	{
		return new ViewModel();
	}
	
	/*
	public function getAdminTable() 
	{
		if(!$this->adminTable) {
			$sm = $this->getServiceLocator();
			$this->adminTable = $sm->get('Admin\Model\AdminTable');
		}
		return $this->adminTable;
	}
	*/
	
}

