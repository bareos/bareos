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

namespace Pool\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Paginator\Adapter\ArrayAdapter;
use Zend\Paginator\Paginator;

class PoolController extends AbstractActionController
{

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$pools = $this->getPools();
				$page = (int) $this->params()->fromQuery('page');
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

				$paginator = new Paginator(new ArrayAdapter($pools));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'limit' => $limit,
						//'pools' => $pools,
						'paginator' => $paginator,
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


				$poolname = $this->params()->fromRoute('id');
				$page = $this->params()->fromQuery('page');
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

				$pool = $this->getPool($poolname);
				$media = $this->getPoolMedia($poolname);

                                $paginator = new Paginator(new ArrayAdapter($media));
                                $paginator->setCurrentPageNumber($page);
                                $paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'poolname' => $poolname,
						'limit' => $limit,
						'pool' => $pool,
						//'media' => $media,
						'paginator' => $paginator,
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	private function getPool($pool)
	{
		$director = $this->getServiceLocator()->get('director');
                $result = $director->send_command("llist pool=".$pool, 2, null);
                $pools = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $pools['result']['pools'][0];
	}

	private function getPools()
	{
		$director = $this->getServiceLocator()->get('director');
		$result = $director->send_command("llist pools", 2, null);
		$pools = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $pools['result']['pools'];
	}

	private function getPoolMedia($pool)
	{
		$director = $this->getServiceLocator()->get('director');
                $result = $director->send_command("llist media pool=".$pool, 2, null);
                $media = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $media['result']['media'];
	}

}

