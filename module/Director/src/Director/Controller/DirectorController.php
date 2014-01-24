<?php

/**
 *
 * Barbossa - A Web-Frontend to manage Bareos
 * 
 * @link      http://github.com/fbergkemper/barbossa for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
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
		$env = array('/usr/sbin');
			
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

