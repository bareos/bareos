<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Dashboard\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DashboardController extends AbstractActionController
{

	protected $jobTable;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {
			return new ViewModel(
				array(
					'runningJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("running"),
                                        'waitingJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("waiting"),
                                        'successfulJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("successful"),
                                        'unsuccessfulJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("unsuccessful"),
				)
			);
		}
		else {
			return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function getJobTable()
	{
		if(!$this->jobTable)
		{
			$sm = $this->getServiceLocator();
			$this->jobTable = $sm->get('Job\Model\JobTable');
		}
		return $this->jobTable;
	}

}

