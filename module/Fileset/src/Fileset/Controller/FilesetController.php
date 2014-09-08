<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
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
use Bareos\BConsole\BConsoleConnector;

class FilesetController extends AbstractActionController
{

	protected $filesetTable;

	public function indexAction()
	{
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

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		$fset = $this->getFilesetTable()->getFileSet($id);
                $cmd = 'show fileset="' . $fset->fileset . '"';
		$config = $this->getServiceLocator()->get('Config');
                $bcon = new BConsoleConnector($config['bconsole']);		
	
		if (!$id) {
		    return $this->redirect()->toRoute('fileset');
		}
	
		return new ViewModel(
			array(
				'fileset' => $this->getFilesetTable()->getFileset($id),
				'history' => $this->getFilesetTable()->getFilesetHistory($id),
				'configuration' => $bcon->getBConsoleOutput($cmd),
			)
		);
	}

	public function getFilesetTable() 
	{
		if(!$this->filesetTable) {
			$sm = $this->getServiceLocator();
			$this->filesetTable = $sm->get('Fileset\Model\FilesetTable');
		}
		return $this->filesetTable;
	}

}

