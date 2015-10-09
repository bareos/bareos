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

	protected $jobModel;

	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

				$jobs = $this->getJobModel()->getJobs();
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
			$job = $this->getJobModel()->getJob($jobid);
			$joblog = $this->getJobModel()->getJobLog($jobid);

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

				$jobs_R = $this->getJobModel()->getJobsByStatus('R', null, null);
				$jobs_l = $this->getJobModel()->getJobsByStatus('l', null, null);

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

				$jobs_F = $this->getJobModel()->getJobsByStatus('F', null, null);
				$jobs_S = $this->getJobModel()->getJobsByStatus('S', null, null);
				$jobs_m = $this->getJobModel()->getJobsByStatus('m', null, null);
				$jobs_M = $this->getJobModel()->getJobsByStatus('M', null, null);
				$jobs_s = $this->getJobModel()->getJobsByStatus('s', null, null);
				$jobs_j = $this->getJobModel()->getJobsByStatus('j', null, null);
				$jobs_c = $this->getJobModel()->getJobsByStatus('c', null, null);
				$jobs_d = $this->getJobModel()->getJobsByStatus('d', null, null);
				$jobs_t = $this->getJobModel()->getJobsByStatus('t', null, null);
				$jobs_p = $this->getJobModel()->getJobsByStatus('p', null, null);
				$jobs_q = $this->getJobModel()->getJobsByStatus('q', null, null);
				$jobs_C = $this->getJobModel()->getJobsByStatus('C', null, null);

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

				$jobs_A = $this->getJobModel()->getJobsByStatus('A', 1, null); // Canceled jobs last 24h
				$jobs_E = $this->getJobModel()->getJobsByStatus('E', 1, null); //
				$jobs_e = $this->getJobModel()->getJobsByStatus('e', 1, null); //
				$jobs_f = $this->getJobModel()->getJobsByStatus('f', 1, null); //

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

				$jobs_T = $this->getJobModel()->getJobsByStatus('T', 1, null); // Terminated
				$jobs_W = $this->getJobModel()->getJobsByStatus('W', 1, null); // Terminated with warnings

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
				$result = $this->getJobModel()->rerunJob($jobid);
				return new ViewModel(
						array(
							'bconsoleOutput' => $result,
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
				$result = $this->getJobModel()->cancelJob($jobid);
				return new ViewModel(
						array(
							'bconsoleOutput' => $result
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

                                $jobs = $this->getJobModel()->getBackupJobs();
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
			$jobname = $this->params()->fromQuery('job');
			$result = $this->getJobModel()->runJob($jobname);
                        return new ViewModel(
				array(
					'result' => $result
                                )
                        );
                }
                else {
                        return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	public function getJobModel()
	{
		if(!$this->jobModel) {
			$sm = $this->getServiceLocator();
			$this->jobModel = $sm->get('Job\Model\JobModel');
		}
		return $this->jobModel;
	}
}
