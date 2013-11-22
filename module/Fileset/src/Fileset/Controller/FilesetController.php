<?php

namespace Fileset\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class FilesetController extends AbstractActionController
{

	protected $filesetTable;
	protected $bconsoleOutput = array();

	public function indexAction()
	{
		return new ViewModel(
			array(
				'filesets' => $this->getFilesetTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		
		if (!$id) {
		    return $this->redirect()->toRoute('fileset');
		}
	
		return new ViewModel(
			array(
				'fileset' => $this->getFilesetTable()->getFileset($id),
				'history' => $this->getFilesetTable()->getFilesetHistory($id),
				'configuration' => $this->getFilesetConfig($id),
			)
		);
	}

	public function getFilesetTable() 
	{
		if(!$this->filesetTable) {
			$sm = $this->getServiceLocator();
			$this->filesetTable = $sm->get('Fileset\Model\FilesetTable');
		}
		return $this->filesetTable;
	}

	public function getFilesetConfig($id) 
	{	

		$fset = $this->getFilesetTable()->getFileSet($id);
		$cmd = "show fileset=" . $fset->fileset;

		$descriptorspec = array(
			0 => array("pipe", "r"),
			1 => array("pipe", "w"),
			2 => array("pipe", "r")
		);

		$cwd = '/usr/sbin';
		$env = NULL;

		$process = proc_open('bconsole', $descriptorspec, $pipes, $cwd, $env);

		if(!is_resource($process)) {
			throw new \Exception("proc_open error");
		}

		if(is_resource($process)) {
			fwrite($pipes[0], $cmd);
			fclose($pipes[0]);
			while(!feof($pipes[1])) {
				array_push($this->bconsoleOutput, fread($pipes[1], 8192));
			}
			fclose($pipes[1]);
		}
		
		$return_value = proc_close($process);

		return $this->bconsoleOutput;

	}

}

