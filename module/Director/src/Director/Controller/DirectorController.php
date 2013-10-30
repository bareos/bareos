<?php

namespace Director\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DirectorController extends AbstractActionController 
{
	protected $directorStatus = array();

	public function indexAction()
	{
		return new ViewModel(array(
				'directorStatus' => $this->getDirectorStatus(),
			));
	}

	public function getDirectorStatus() 
	{
		if(!$this->directorStatus)
		{
			$descriptorspec = array(
			  0 => array("pipe", "r"),
			  1 => array("pipe", "w"),
			  2 => array("pipe", "r")
			);
			$cwd = '/usr/sbin';
			$env = NULL;
			
			$process = proc_open('bconsole', $descriptorspec, $pipes, $cwd, $env);
			
			if(!is_resource($process)) 
			  throw new \Exception("proc_open error");
			
			if(is_resource($process))
			{
			  fwrite($pipes[0], 'status director');
			  fclose($pipes[0]);
			  while(!feof($pipes[1])) {
			    array_push($this->directorStatus, fread($pipes[1],8192));
			  }
			  fclose($pipes[1]);
			}
			$return_value = proc_close($process);
		}
		return $this->directorStatus;
	}
}

