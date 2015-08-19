<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Log\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class LogController extends AbstractActionController
{

	protected $logTable;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
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
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
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
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
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

