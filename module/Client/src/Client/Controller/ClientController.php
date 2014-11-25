<?php

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Bareos\BSock\BareosBSock;

class ClientController extends AbstractActionController
{

	protected $clientTable;
	protected $jobTable;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {
				$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'ClientId';
				$order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$paginator = $this->getClientTable()->fetchAll(true, $order_by, $order);
				$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'paginator' => $paginator,
						'order_by' => $order_by,
										'order' => $order,
										'limit' => $limit,
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function detailsAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {
				$id = (int) $this->params()->fromRoute('id', 0);
				if(!$id) {
					return $this->redirect()->toRoute('client');
				}

				$result = $this->getClientTable()->getClient($id);
				$cmd = 'status client="' . $result->name . '"';
				$config = $this->getServiceLocator()->get('Config');
				$bsock = new BareosBSock();
				$bsock->set_config($config['directors'][$_SESSION['bareos']['director']]);
				$bsock->set_user_credentials($_SESSION['bareos']['username'], $_SESSION['bareos']['password']);
				$bsock->init();

				return new ViewModel(
					array(
					  'client' => $this->getClientTable()->getClient($id),
					  'job' => $this->getJobTable()->getLastSuccessfulClientJob($id),
					  'bconsoleOutput' => $bsock->send_command($cmd),
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function getClientTable()
	{
		if(!$this->clientTable) {
			$sm = $this->getServiceLocator();
			$this->clientTable = $sm->get('Client\Model\ClientTable');
		}
		return $this->clientTable;
	}

	public function getJobTable()
	{
		if(!$this->jobTable) {
			$sm = $this->getServiceLocator();
			$this->jobTable = $sm->get('Job\Model\JobTable');
		}
		return $this->jobTable;
	}

}

