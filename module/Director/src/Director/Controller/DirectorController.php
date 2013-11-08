<?php

namespace Director\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DirectorController extends AbstractActionController 
{
	protected $directorOutput = array();
	
	public function indexAction()
	{
		$cmd = "status director";
		return new ViewModel(array(
				'directorOutput' => $this->getBConsoleOutput($cmd),
			));
	}

	public function messagesAction()
	{
		$cmd = "messages";
		return new ViewModel(array(
				'directorOutput' => $this->getBConsoleOutput($cmd),
			));
	}
	
	public function schedulerAction()
	{
		$cmd = "status scheduler";
		return new ViewModel(array(
				'directorOutput' => $this->getBConsoleOutput($cmd),
			));
	}
	
	public function versionAction()
	{
		$cmd = "version";
		return new ViewModel(array(
				'directorOutput' => $this->getBConsoleOutput($cmd),
			));
	}
	
	public function getBConsoleOutput($cmd) 
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
			fwrite($pipes[0], $cmd);
			fclose($pipes[0]);
			while(!feof($pipes[1])) {
			  array_push($this->directorOutput, fread($pipes[1],8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);
		
		return $this->directorOutput;
		
	}
	
}

