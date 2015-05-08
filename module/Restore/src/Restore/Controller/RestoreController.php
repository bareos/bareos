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

namespace Restore\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\ViewModel\JsonModel;

class RestoreController extends AbstractActionController
{

	/**
	 *
	 */
	public function indexAction()
	{
		return new ViewModel();
	}

	/**
	 * Get job list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getJobs($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist jobs", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list jobs", 2);
		}
		$jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $jobs['result'][0]['jobs'];
	}

	/**
	 * Get client list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getClients($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist clients", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list clients", 2);
		}
		$clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $clients['result'][0]['clients'];
	}

	/**
	 * Get fileset list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getFilesets($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist filesets", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list filesets", 2);
		}
		$filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $filesets['result'][0]['filesets'];
	}

}

