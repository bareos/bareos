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

namespace Fileset\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class FilesetController extends AbstractActionController
{

	protected $filesetTable;
	protected $director;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
				$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'FileSetId';
				$order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$paginator = $this->getFilesetTable()->fetchAll(true, $order_by, $order);
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
				$fset = $this->getFilesetTable()->getFileSet($id);
				$cmd = 'show fileset="' . $fset->fileset . '"';
				$this->director = $this->getServiceLocator()->get('director');

				if (!$id) {
					return $this->redirect()->toRoute('fileset');
				}

				return new ViewModel(
					array(
						'fileset' => $this->getFilesetTable()->getFileset($id),
						'history' => $this->getFilesetTable()->getFilesetHistory($id),
						'configuration' => $this->director->send_command($cmd),
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	private function getFilesetTable()
	{
		if(!$this->filesetTable) {
			$sm = $this->getServiceLocator();
			$this->filesetTable = $sm->get('Fileset\Model\FilesetTable');
		}
		return $this->filesetTable;
	}

}

