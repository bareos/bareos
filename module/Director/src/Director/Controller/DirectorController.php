<?php

namespace Director\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DirectorController extends AbstractActionController 
{
	protected $directorOutput = array();

	public function indexAction()
	{
		return new ViewModel(array(
				'directorOutput' => $this->getDirectorStatus(),
			));
	}

	public function messagesAction()
	{
		return new ViewModel(array(
				'directorOutput' => $this->getDirectorMessages(),
			));
	}
	
	public function schedulerAction()
	{
		return new ViewModel(array(
				'directorOutput' => $this->getDirectorSchedulerStatus(),
			));
	}
	
	public function versionAction()
	{
		return new ViewModel(array(
				'directorOutput' => $this->getDirectorVersion(),
			));
	}
	
	public function getDirectorStatus() 
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
			  array_push($this->directorOutput, fread($pipes[1],8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);
		
		return $this->directorOutput;
		
	}
	
	public function getDirectorMessages() 
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
			fwrite($pipes[0], 'messages');
			fclose($pipes[0]);
			while(!feof($pipes[1])) {
			  array_push($this->directorOutput, fread($pipes[1],8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);
		
		return $this->directorOutput;
		
	}
	
	public function getDirectorSchedulerStatus() 
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
			fwrite($pipes[0], 'status scheduler');
			fclose($pipes[0]);
			while(!feof($pipes[1])) {
			  array_push($this->directorOutput, fread($pipes[1],8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);
		
		return $this->directorOutput;
		
	}
	
	public function getDirectorVersion() 
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
			fwrite($pipes[0], 'version');
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

