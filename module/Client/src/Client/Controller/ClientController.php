<?php

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class ClientController extends AbstractActionController
{

	protected $clientTable;
	protected $jobTable;
	protected $bconsoleOutput = array();

	public function indexAction()
	{
		$paginator = $this->getClientTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(array('paginator' => $paginator));
	}

	public function detailsAction() 
	{
		
		$id = (int) $this->params()->fromRoute('id', 0);
		if(!$id) {
		    return $this->redirect()->toRoute('client');
		}
		
		$result = $this->getClientTable()->getClient($id);
		$cmd = "status client=" . $result->name;
		
		return new ViewModel(
		    array(
		      'client' => $this->getClientTable()->getClient($id),
		      'job' => $this->getJobTable()->getLastSuccessfulClientJob($id),
		      'bconsoleOutput' => $this->getBConsoleOutput($cmd),
		    )
		);
		
	}

	public function getClientTable() 
	{
		if(!$this->clientTable) {
			$sm = $this->getServiceLocator();
			$this->clientTable = $sm->get('Client\Model\ClientTable');
		}
		return $this->clientTable;
	}
	
	public function getJobTable() 
	{
		if(!$this->jobTable) {
			$sm = $this->getServiceLocator();
			$this->jobTable = $sm->get('Job\Model\JobTable');
		}
		return $this->jobTable;
	}

	public function getBConsoleOutput($cmd) 
	{
		
		$descriptorspec = array(
			0 => array("pipe", "r"),
			1 => array("pipe", "w"),
			2 => array("pipe", "r")
		);
		
		$cwd = '/usr/sbin';
		$env = array('/usr/sbin');
			
		$process = proc_open('sudo /usr/sbin/bconsole', $descriptorspec, $pipes, $cwd, $env);
			
		if(!is_resource($process)) 
			throw new \Exception("proc_open error");
			
		if(is_resource($process))
		{
			fwrite($pipes[0], $cmd);
			fclose($pipes[0]);
			while(!feof($pipes[1])) {
			  array_push($this->bconsoleOutput, fread($pipes[1],8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);
		
		return $this->bconsoleOutput;
		
	}
	
}

