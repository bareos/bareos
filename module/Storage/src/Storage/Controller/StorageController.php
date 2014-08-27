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

namespace Storage\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Bareos\BConsole\BConsoleConnector;

class StorageController extends AbstractActionController
{

	protected $storageTable;
	protected $bconsoleOutput = array();

	public function indexAction()
	{
		return new ViewModel(
			array(
				'storages' => $this->getStorageTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		if(!$id) {
		    return $this->redirect()->toRoute('storage');
		}
		$result = $this->getStorageTable()->getStorage($id);
		$cmd = "status storage=" . $result->name;
		$config = $this->getServiceLocator()->get('Config');
                $bcon = new BConsoleConnector($config['bconsole']);
		return new ViewModel(array(
				'bconsoleOutput' => $bcon->getBConsoleOutput($cmd),
			)
		);
	}

	public function autochangerAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
        if(!$id) {
            return $this->redirect()->toRoute('storage');
        }
        $result = $this->getStorageTable()->getStorage($id);
        $cmd = "status storage=" . $result->name . " slots";
	$config = $this->getServiceLocator()->get('Config');
        $bcon = new BConsoleConnector($config['bconsole']);
        return new ViewModel(array(
                'bconsoleOutput' => $bcon->getBConsoleOutput($cmd),
            )
        );
	}

	public function getStorageTable() 
	{
		if(!$this->storageTable) {
			$sm = $this->getServiceLocator();
			$this->storageTable = $sm->get('Storage\Model\StorageTable');
		}
		return $this->storageTable;
	}

}

