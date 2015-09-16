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
use Zend\Paginator\Adapter\ArrayAdapter;
use Zend\Paginator\Paginator;

class FilesetController extends AbstractActionController
{

	protected $director;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$filesets = $this->getFilesets();
				$page = (int) $this->params()->fromQuery('page', 1);
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

				$paginator = new Paginator(new ArrayAdapter($filesets));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'paginator' => $paginator,
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

				$id = $this->params()->fromRoute('id', 0);
				$fileset = $this->getFileset($id);

				return new ViewModel(
					array(
						'fileset' => $fileset
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	private function getFilesets()
	{
		$director = $this->getServiceLocator()->get('director');
		$result = $director->send_command("list filesets", 2, null);
		$filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $filesets['result']['filesets'];
	}

	private function getFileset($id) {
		$director = $this->getServiceLocator()->get('director');
		$result = $director->send_command("llist fileset filesetid=".$id, 2, null);
		$fileset = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $fileset['result']['filesets'][0];
	}

}

