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

namespace Fileset\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class FilesetController extends AbstractActionController
{

	protected $filesetTable;
	protected $bconsoleOutput = array();

	public function indexAction()
	{
		$paginator = $this->getFilesetTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(array('paginator' => $paginator));
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
		$env = array('/usr/sbin');

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

