<?php

namespace Log\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class LogController extends AbstractActionController
{

	protected $logTable;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {
				$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'LogId';
						$order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
						$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$paginator = $this->getLogTable()->fetchAll(true, $order_by, $order);
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
				if (!$id) {
					return $this->redirect()->toRoute('log');
				}

				return new ViewModel(array(
						'log' => $this->getLogTable()->getLog($id),
					));
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function jobAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {
				$id = (int) $this->params()->fromRoute('id', 0);
				if (!$id) {
					return $this->redirect()->toRoute('log');
				}

				return new ViewModel(array(
						'log' => $this->getLogTable()->getLogsByJob($id),
						'jobid' => (int) $this->params()->fromRoute('id'),
					));
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function getLogTable()
	{
		if(!$this->logTable) {
			$sm = $this->getServiceLocator();
			$this->logTable = $sm->get('Log\Model\LogTable');
		}
		return $this->logTable;
	}

}

