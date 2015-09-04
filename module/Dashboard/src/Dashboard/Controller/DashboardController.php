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

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] && $this->SessionTimeoutPlugin()->timeout()) {

			return new ViewModel(
				array(
					'runningJobs' => $this->getJobCount("running", 1, null),
                                        'waitingJobs' => $this->getJobCount("waiting", 1, null),
                                        'successfulJobs' => $this->getJobCount("successful", 1, null),
                                        'unsuccessfulJobs' => $this->getJobCount("unsuccessful", 1, null),
				)
			);
		}
		else {
			return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	private function getJobCount($status=null, $days=null, $hours=null)
	{
		if($status != null) {
			$director = $this->getServiceLocator()->get('director');
			if($status == "running") {
				$jobs_R = $this->getJobsByStatus('R', 1, null);
				$jobs_l = $this->getJobsByStatus('l', 1, null);
				$num = count($jobs_R) + count($jobs_l);
				return $num;
			}
			elseif($status == "waiting") {
				$jobs_F = $this->getJobsByStatus('F', 1, null);
				$jobs_S = $this->getJobsByStatus('S', 1, null);
				$jobs_s = $this->getJobsByStatus('s', 1, null);
				$jobs_m = $this->getJobsByStatus('m', 1, null);
				$jobs_M = $this->getJobsByStatus('M', 1, null);
				$jobs_j = $this->getJobsByStatus('j', 1, null);
				$jobs_c = $this->getJobsByStatus('c', 1, null);
				$jobs_C = $this->getJobsByStatus('C', 1, null);
				$jobs_d = $this->getJobsByStatus('d', 1, null);
				$jobs_t = $this->getJobsByStatus('t', 1, null);
				$jobs_p = $this->getJobsByStatus('p', 1, null);
				$jobs_q = $this->getJobsByStatus('q', 1, null);
				$num = count($jobs_F) + count($jobs_S) +
					count($jobs_s) + count($jobs_m) +
					count($jobs_M) + count($jobs_j) +
					count($jobs_c) + count($jobs_C) +
					count($jobs_d) + count($jobs_t) +
					count($jobs_p) + count($jobs_q);
				return $num;
			}
			elseif($status == "successful") {
				$jobs_T = $this->getJobsByStatus('T', 1, null);
				$jobs_W = $this->getJobsByStatus('W', 1, null);
				$num = count($jobs_T) + count($jobs_W);
				return $num;
			}
			elseif($status == "unsuccessful") {
				$jobs_A = $this->getJobsByStatus('A', 1, null);
                                $jobs_E = $this->getJobsByStatus('E', 1, null);
				$jobs_e = $this->getJobsByStatus('e', 1, null);
				$jobs_f = $this->getJobsByStatus('f', 1, null);
                                $num = count($jobs_A) + count($jobs_E) + count($jobs_e) + count($jobs_f);
                                return $num;
			}
			else {
				return null;
			}
		}
		else {
			return null;
		}
	}

	private function getJobsByStatus($status=null, $days=null, $hours=null)
        {
                if($status != null) {
                        $director = $this->getServiceLocator()->get('director');
                        if($days != null) {
                                $result = $director->send_command('llist jobs jobstatus="'.$status.'" days="'.$days.'"', 2, null);
                        }
                        elseif($hours != null) {
                                $result = $director->send_command('llist jobs jobstatus="'.$status.'" hours="'.$hours.'"', 2, null);
                        }
                        else {
                                $result = $director->send_command('llist jobs jobstatus="'.$status.'"', 2, null);
                        }
                        $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                        return $jobs['result']['jobs'];
                }
                else {
                        return null;
                }
        }

}

