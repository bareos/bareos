<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 * @author    Frank Bergkemper
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

namespace Job\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Paginator\Adapter\ArrayAdapter;
use Zend\Paginator\Paginator;

class JobController extends AbstractActionController
{

	protected $jobTable;
	protected $bconsoleOutput = array();
	protected $director = null;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs = $this->getJobs();
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$page = (int) $this->params()->fromQuery('page');

				$paginator = new Paginator(new ArrayAdapter($jobs));
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

			$jobid = (int) $this->params()->fromRoute('id', 0);
			$job = $this->getJob($jobid);
			$joblog = $this->getJobLog($jobid);

			return new ViewModel(array(
				'job' => $job,
				'joblog' => $joblog,
			));
		}
		else {
			return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function runningAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs_R = $this->getJobsByStatus('R', null, null);
				$jobs_l = $this->getJobsByStatus('l', null, null);

				$jobs = array_merge($jobs_R, $jobs_l);

				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$page = (int) $this->params()->fromQuery('page', 1);

				$paginator = new Paginator(new ArrayAdapter($jobs));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'paginator' => $paginator,
						'limit' => $limit,
						'jobs' => $jobs
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function waitingAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs_F = $this->getJobsByStatus('F', null, null);
				$jobs_S = $this->getJobsByStatus('S', null, null);
				$jobs_m = $this->getJobsByStatus('m', null, null);
				$jobs_M = $this->getJobsByStatus('M', null, null);
				$jobs_s = $this->getJobsByStatus('s', null, null);
				$jobs_j = $this->getJobsByStatus('j', null, null);
				$jobs_c = $this->getJobsByStatus('c', null, null);
				$jobs_d = $this->getJobsByStatus('d', null, null);
				$jobs_t = $this->getJobsByStatus('t', null, null);
				$jobs_p = $this->getJobsByStatus('p', null, null);
				$jobs_q = $this->getJobsByStatus('q', null, null);
				$jobs_C = $this->getJobsByStatus('C', null, null);

				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
				$page = (int) $this->params()->fromQuery('page', 1);

				$jobs = array_merge(
					$jobs_F,$jobs_S,$jobs_m,$jobs_M,
					$jobs_s,$jobs_j,$jobs_c,$jobs_d,
					$jobs_t,$jobs_p,$jobs_q,$jobs_C
				);

				$paginator = new Paginator(new ArrayAdapter($jobs));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'paginator' => $paginator,
						'limit' => $limit,
						'jobs' => $jobs
					)
				);

		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function unsuccessfulAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs_A = $this->getJobsByStatus('A', 1, null); // Canceled jobs last 24h
				$jobs_E = $this->getJobsByStatus('E', 1, null); //
				$jobs_e = $this->getJobsByStatus('e', 1, null); //
				$jobs_f = $this->getJobsByStatus('f', 1, null); //

				$jobs = array_merge($jobs_A, $jobs_E, $jobs_e, $jobs_f);
				$page =  (int) $this->params()->fromQuery('page', 1);
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

				$paginator = new Paginator(new ArrayAdapter($jobs));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'jobs' => $jobs,
						'paginator' => $paginator,
						'limit' => $limit,
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function successfulAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs_T = $this->getJobsByStatus('T', 1, null); // Terminated
				$jobs_W = $this->getJobsByStatus('W', 1, null); // Terminated with warnings

				$jobs = array_merge($jobs_T, $jobs_W);
				$page = (int) $this->params()->fromQuery('page', 1);
				$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

				$paginator = new Paginator(new ArrayAdapter($jobs));
				$paginator->setCurrentPageNumber($page);
				$paginator->setItemCountPerPage($limit);

				return new ViewModel(
					array(
						'paginator' => $paginator,
						'limit' => $limit,
						'jobs' => $jobs,
					)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function rerunAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
				$jobid = (int) $this->params()->fromRoute('id', 0);
				$cmd = "rerun jobid=" . $jobid . " yes";
				$this->director = $this->getServiceLocator()->get('director');
				return new ViewModel(
						array(
							'bconsoleOutput' => $this->director->send_command($cmd),
							'jobid' => $jobid,
						)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function cancelAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
				$jobid = (int) $this->params()->fromRoute('id', 0);
				$cmd = "cancel jobid=" . $jobid . " yes";
				$this->director = $this->getServiceLocator()->get('director');
				return new ViewModel(
						array(
							'bconsoleOutput' => $this->director->send_command($cmd)
						)
				);
		}
		else {
				return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
	}

	public function runAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

                                $jobs = $this->getDefinedBackupJobs();
                                $page = (int) $this->params()->fromQuery('page', 1);
                                $limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';

                                $paginator = new Paginator(new ArrayAdapter($jobs));
                                $paginator->setCurrentPageNumber($page);
                                $paginator->setItemCountPerPage($limit);

                                return new ViewModel(
                                                array(
							'paginator' => $paginator,
							'jobs' => $jobs,
							'limit' => $limit
                                                )
                                );
                }
                else {
                                return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	public function queueAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
			if($this->params()->fromQuery('job')) {
				$jobname = $this->params()->fromQuery('job');
			}
			else {
				$jobname = null;
			}

                        return new ViewModel(
				array(
					'result' => $this->queueJob($jobname)
                                )
                        );
                }
                else {
                        return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	private function getJobs()
	{
		$director = $this->getServiceLocator()->get('director');
                $result = $director->send_command('llist jobs', 2, null);
                $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $jobs['result']['jobs'];
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

	private function getDefinedBackupJobs()
	{
		$director = $this->getServiceLocator()->get('director');
		$result = $director->send_command(".jobs type=B", 2, null);
		$jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $jobs['result']['jobs'];
	}

	private function getJob($jobid)
	{
		$director = $this->getServiceLocator()->get('director');
                $result = $director->send_command('llist jobid="'.$jobid.'"', 2, null);
                $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $jobs['result']['jobs'][0];
	}

	private function getJobLog($jobid)
	{
		$director = $this->getServiceLocator()->get('director');
                $result = $director->send_command('list joblog jobid="'.$jobid.'"', 2, null);
                $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $jobs['result']['joblog'];
	}

	private function queueJob($jobname=null)
	{
		$result = "";
		if($jobname != null) {
			$director = $this->getServiceLocator()->get('director');
			$result = $director->send_command('run job="'.$jobname.'" yes');
		}
		return $result;
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

