<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
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
use Bareos\BConsole\BConsoleConnector;

class JobController extends AbstractActionController
{

	protected $jobTable;
	protected $logTable;
	protected $bconsoleOutput = array();

	public function indexAction() 
	{
		$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'JobId';
		$order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
		$limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
		$paginator = $this->getJobTable()->fetchAll(true, $order_by, $order);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage($limit);

		return new ViewModel(
			array(
				'paginator' => $paginator,
				'order_by' => $order_by,
				'order' => $order,
				'limit' => $limit,
				'allJobs' => $this->getJobTable()->fetchAll(),
				)
			);

	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		if (!$id) {
		    return $this->redirect()->toRoute('job');
		}
	  
		return new ViewModel(array(
				'job' => $this->getJobTable()->getJob($id),
				'log' => $this->getLogTable()->getLogsByJob($id),
			));
	}

	public function runningAction() 
	{
		$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'JobId';
                $order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
                $limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
		$paginator = $this->getJobTable()->getRunningJobs(true, $order_by, $order);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage($limit);

		return new ViewModel(
			array(
			      	'paginator' => $paginator,
				'order_by' => $order_by,
                                'order' => $order,
                                'limit' => $limit,
			      	'runningJobs' => $this->getJobTable()->getRunningJobs()
			)
		);
	}
	
	public function waitingAction() 
	{
		$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'JobId';
                $order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
                $limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
		$paginator = $this->getJobTable()->getWaitingJobs(true, $order_by, $order);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage($limit);

		return new ViewModel(
			array(
			      	'paginator' => $paginator,
				'order_by' => $order_by,
                                'order' => $order,
                                'limit' => $limit,		
			      	'waitingJobs' => $this->getJobTable()->getWaitingJobs()
			)
		);
	}
	
	public function unsuccessfulAction() 
	{
		$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'JobId';
                $order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
                $limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
		$paginator = $this->getJobTable()->getLast24HoursUnsuccessfulJobs(true, $order_by, $order);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage($limit);

		return new ViewModel(
			array(
			      	'paginator' => $paginator,
				'order_by' => $order_by,
                                'order' => $order,
                                'limit' => $limit,      
			      	'lastUnsuccessfulJobs' => $this->getJobTable()->getLast24HoursUnsuccessfulJobs(),
			)
		);
	}
	
	public function successfulAction() 
	{
		$order_by = $this->params()->fromRoute('order_by') ? $this->params()->fromRoute('order_by') : 'JobId';
                $order = $this->params()->fromRoute('order') ? $this->params()->fromRoute('order') : 'DESC';
                $limit = $this->params()->fromRoute('limit') ? $this->params()->fromRoute('limit') : '25';
		$paginator = $this->getJobTable()->getLast24HoursSuccessfulJobs(true, $order_by, $order);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage($limit);

		return new ViewModel(
			array(
				'paginator' => $paginator,
				'order_by' => $order_by,
				'order' => $order,
				'limit' => $limit,
				'lastSuccessfulJobs' => $this->getJobTable()->getLast24HoursSuccessfulJobs(),
			)
		);
	}

	public function timelineAction() 
	{
		return new ViewModel();
	}
	
	public function rerunAction()
	{
		$jobid = (int) $this->params()->fromRoute('id', 0);
		$cmd = "rerun jobid=" . $jobid . " yes";
		$config = $this->getServiceLocator()->get('Config');
		$bcon = new BConsoleConnector($config['bconsole']);
		return new ViewModel(
			array(
				'bconsoleOutput' => $bcon->getBConsoleOutput($cmd),
				'jobid' => $jobid,
			)
		);
	}

	public function cancelAction()
	{
		$jobid = (int) $this->params()->fromRoute('id', 0);
                $cmd = "cancel jobid=" . $jobid . " yes";
		$config = $this->getServiceLocator()->get('Config');
		$bcon = new BConsoleConnector($config['bconsole']);
                return new ViewModel(
                        array(
				'bconsoleOutput' => $bcon->getBConsoleOutput($cmd)
			)
                );	
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
	
	public function getLogTable() 
	{
		if(!$this->logTable)
		{
			$sm = $this->getServiceLocator();
			$this->logTable = $sm->get('Log\Model\LogTable');
		}
		return $this->logTable;
	}
	
}

