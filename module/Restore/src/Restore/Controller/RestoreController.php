<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Restore\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\View\Model\JsonModel;
use Restore\Model\Restore;
use Restore\Form\RestoreForm;

class RestoreController extends AbstractActionController
{

	protected $restore_params = array();

	/**
	 *
	 */
	public function indexAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {

			$this->getRestoreParams();

			if($this->restore_params['client'] == null) {
				$this->restore_params['client'] = @array_pop($this->getClients(2))['name'];
			}

			if($this->restore_params['type'] == "client" && $this->restore_params['jobid'] == null) {
				$latestbackup = $this->getClientBackups($this->restore_params['client'], "any", "desc", 1);
				$this->restore_params['jobid'] = $latestbackup[0]['jobid'];
			}

			if(isset($this->restore_params['mergejobs']) && $this->restore_params['mergejobs'] == 1) {
				$jobids = $this->restore_params['jobid'];
			}
			else {
				$jobids = $this->getJobIds($this->restore_params['jobid'], $this->restore_params['mergefilesets']);
			}

			if($this->restore_params['type'] == "client") {
				$backups = $this->getClientBackups($this->restore_params['client'], "any", "desc");
			}

			$jobs = $this->getJobs(2);
                        $clients = $this->getClients(2);
                        $filesets = $this->getFilesets(0);
                        $restorejobs = $this->getRestoreJobs(0);

			// Create the form
			$form = new RestoreForm(
				$this->restore_params,
				$jobs,
				$clients,
				$filesets,
				$restorejobs,
				$jobids,
				$backups
			);

			// Set the method attribute for the form
			$form->setAttribute('method', 'post');
			$form->setAttribute('onsubmit','return getFiles()');

			$request = $this->getRequest();

			if($request->isPost()) {

				$restore = new Restore();
				$form->setInputFilter($restore->getInputFilter());
				$form->setData( $request->getPost() );

				if($form->isValid()) {

					if($this->restore_params['type'] == "client") {

						$type = $this->restore_params['type'];
						$jobid = $form->getInputFilter()->getValue('jobid');
						$client = $form->getInputFilter()->getValue('client');
						$restoreclient = $form->getInputFilter()->getValue('restoreclient');
						$restorejob = $form->getInputFilter()->getValue('restorejob');
						$where = $form->getInputFilter()->getValue('where');
						$fileid = $form->getInputFilter()->getValue('checked_files');
						$dirid = $form->getInputFilter()->getValue('checked_directories');
						$jobids = $form->getInputFilter()->getValue('jobids_hidden');
						$replace = $form->getInputFilter()->getValue('replace');

						$result = $this->restore($type, $jobid, $client, $restoreclient, $restorejob, $where, $fileid, $dirid, $jobids, $replace);

					}

					return new ViewModel(array(
						'result' => $result
                                        ));

				}
				else {
					return new ViewModel(array(
						'restore_params' => $this->restore_params,
						'form' => $form
					));
				}

			}
			else {
				return new ViewModel(array(
					'restore_params' => $this->restore_params,
					'form' => $form
				));
			}

                }
                else {
                        return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	/**
	 * Delivers a subtree as Json for JStree
	 */
	public function filebrowserAction()
	{
		if($_SESSION['bareos']['authenticated'] == true && $this->SessionTimeoutPlugin()->timeout()) {
			$this->getRestoreParams();
                        $this->layout('layout/json');
                        return new ViewModel(array(
                                'items' => $this->buildSubtree()
                        ));
                }
                else{
                        return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	/**
	 * Builds a subtree as Json for JStree
	 */
	private function buildSubtree()
	{

		$this->getRestoreParams();

		// Get directories
		if($this->restore_params['type'] == "client") {
			$directories = $this->getDirectories(
						$this->getJobIds($this->restore_params['jobid'],
						$this->restore_params['mergefilesets'],
						$this->restore_params['mergejobs']),
						$this->restore_params['id']
					);
		}
		else {
			$directories = $this->getDirectories(
						$this->restore_params['jobid'],
						$this->restore_params['id']
					);
		}

		// Get files
		if($this->restore_params['type'] == "client") {
			$files = $this->getFiles(
						$this->getJobIds($this->restore_params['jobid'],
						$this->restore_params['mergefilesets'],
						$this->restore_params['mergejobs']),
						$this->restore_params['id']
					);
		}
		else {
			$files = $this->getFiles(
						$this->restore_params['jobid'],
						$this->restore_params['id']
					);
		}

		$dnum = count($directories);
		$fnum = count($files);
		$tmp = $dnum;

		// Build Json for JStree
		$items = '[';

		if($dnum > 0) {

			foreach($directories as $dir) {
				if($dir['name'] == ".") {
					--$dnum;
					next($directories);
				}
				elseif($dir['name'] == "..") {
					--$dnum;
					next($directories);
				}
				else {
					--$dnum;
					$items .= '{';
					$items .= '"id":"-' . $dir['pathid'] . '"';
					$items .= ',"text":"' . $dir["name"] . '"';
					$items .= ',"icon":"glyphicon glyphicon-folder-close"';
					$items .= ',"state":""';
					$items .= ',"data":' . \Zend\Json\Json::encode($dir, \Zend\Json\Json::TYPE_OBJECT);
					$items .= ',"children":true';
					$items .= '}';
					if($dnum > 0) {
						$items .= ",";
					}
				}
			}

		}

		if( $tmp > 2 && $fnum > 0 ) {
			$items .= ",";
		}

		if($fnum > 0) {

			foreach($files as $file) {
				$items .= '{';
				$items .= '"id":"' . $file["fileid"] . '"';
				$items .= ',"text":"' . $file["name"] . '"';
				$items .= ',"icon":"glyphicon glyphicon-file"';
				$items .= ',"state":""';
				$items .= ',"data":' . \Zend\Json\Json::encode($file, \Zend\Json\Json::TYPE_OBJECT);
				$items .= '}';
				--$fnum;
				if($fnum > 0) {
					$items .= ",";
				}
			}

		}

		$items .= ']';

		return $items;
	}

	/**
	 * Retrieve restore parameters
	 */
	private function getRestoreParams()
	{
		if($this->params()->fromQuery('type')) {
                        $this->restore_params['type'] = $this->params()->fromQuery('type');
                }
                else {
                        $this->restore_params['type'] = 'client';
                }

		if($this->params()->fromQuery('jobid')) {
			$this->restore_params['jobid'] = $this->params()->fromQuery('jobid');
		}
		else {
			$this->restore_params['jobid'] = null;
		}

		if($this->params()->fromQuery('client')) {
                        $this->restore_params['client'] = $this->params()->fromQuery('client');
                }
                else {
                        $this->restore_params['client'] = null;
                }

		if($this->params()->fromQuery('restoreclient')) {
                        $this->restore_params['restoreclient'] = $this->params()->fromQuery('restoreclient');
                }
                else {
                        $this->restore_params['restoreclient'] = null;
                }

		if($this->params()->fromQuery('restorejob')) {
                        $this->restore_params['restorejob'] = $this->params()->fromQuery('restorejob');
                }
                else {
                        $this->restore_params['restorejob'] = null;
                }

		if($this->params()->fromQuery('fileset')) {
                        $this->restore_params['fileset'] = $this->params()->fromQuery('fileset');
                }
                else {
                        $this->restore_params['fileset'] = null;
                }

		if($this->params()->fromQuery('before')) {
                        $this->restore_params['before'] = $this->params()->fromQuery('before');
                }
                else {
                        $this->restore_params['before'] = null;
                }

		if($this->params()->fromQuery('where')) {
                        $this->restore_params['where'] = $this->params()->fromQuery('where');
                }
                else {
                        $this->restore_params['where'] = null;
                }

		if($this->params()->fromQuery('fileid')) {
                        $this->restore_params['fileid'] = $this->params()->fromQuery('fileid');
                }
                else {
                        $this->restore_params['fileid'] = null;
                }

		if($this->params()->fromQuery('dirid')) {
                        $this->restore_params['dirid'] = $this->params()->fromQuery('dirid');
                }
                else {
                        $this->restore_params['dirid'] = null;
                }

		if($this->params()->fromQuery('id')) {
                        $this->restore_params['id'] = $this->params()->fromQuery('id');
                }
                else {
                        $this->restore_params['id'] = null;
                }

		if($this->params()->fromQuery('jobids')) {
                        $this->restore_params['jobids'] = $this->params()->fromQuery('jobids');
                }
                else {
                        $this->restore_params['jobids'] = null;
                }

		if($this->params()->fromQuery('mergefilesets')) {
                        $this->restore_params['mergefilesets'] = $this->params()->fromQuery('mergefilesets');
                }
                else {
                        $this->restore_params['mergefilesets'] = 0;
                }

		if($this->params()->fromQuery('mergejobs')) {
                        $this->restore_params['mergejobs'] = $this->params()->fromQuery('mergejobs');
                }
                else {
                        $this->restore_params['mergejobs'] = 0;
                }

		if($this->params()->fromQuery('replace')) {
                        $this->restore_params['replace'] = $this->params()->fromQuery('replace');
                }
                else {
                        $this->restore_params['replace'] = null;
                }

		if($this->params()->fromQuery('replaceoptions')) {
                        $this->restore_params['replaceoptions'] = $this->params()->fromQuery('replaceoptions');
                }
                else {
                        $this->restore_params['replaceoptions'] = null;
                }

		if($this->params()->fromQuery('versions')) {
                        $this->restore_params['versions'] = $this->params()->fromQuery('versions');
                }
                else {
                        $this->restore_params['versions'] = null;
                }

	}

	/**
	 *
	 */
	private function restore($type=null, $jobid=null, $client=null, $restoreclient=null, $restorejob=null, $where=null, $fileid=null, $dirid=null, $jobids=null, $replace=null)
	{
		$result = null;
		$director = $this->getServiceLocator()->get('director');

		if($type == "client") {
                        $result = $director->restore($type, $jobid, $client, $restoreclient, $restorejob, $where, $fileid, $dirid, $jobids, $replace);
		}
		elseif($type == "job") {
			// TODO
                        //$result = $director->restore();
		}

		return $result;
	}

	/**
	 *
	 * @param $jobid
	 * @param $pathid
	 * @return array
	 */
	private function getDirectories($jobid=null, $pathid=null)
	{
		$director = $this->getServiceLocator()->get('director');
		if($pathid == null || $pathid == "#") {
			$result = $director->send_command(".bvfs_lsdirs jobid=$jobid path=/", 2, $jobid);
		}
		else {
			$result = $director->send_command(".bvfs_lsdirs jobid=$jobid pathid=" . abs($pathid), 2, $jobid);
		}
		$directories = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $directories['result']['directories'];
	}

	/**
	 *
	 * @param $jobid
	 * @param $pathid
	 * @return array
	 */
	private function getFiles($jobid=null, $pathid=null)
	{
		$director = $this->getServiceLocator()->get('director');
		if($pathid == null || $pathid == "#") {
			$result = $director->send_command(".bvfs_lsfiles jobid=$jobid path=/", 2);
		}
		else {
			$result = $director->send_command(".bvfs_lsfiles jobid=$jobid pathid=" . abs($pathid), 2);
		}
		$files = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $files['result']['files'];
	}

	/**
	 * TODO
	 */
	private function getRevisions($jobid=null, $filenameid=null)
	{
		$director = $this->getServiceLocator()->get('director');

		// TODO

	}

	/**
	 * Get all JobIds needed to restore a particular job.
	 * With the all option set to true the director will
	 * use all definded filesets for a client.
	 *
	 * @param $jobid
	 * @return mixed
	 */
	private function getJobIds($jobid=null, $mergefilesets=0, $mergejobs=0)
	{
		$result = null;
		$director = $this->getServiceLocator()->get('director');

		if($mergefilesets == 0 && $mergejobs == 0) {
                        $cmd = ".bvfs_get_jobids jobid=$jobid all";
			$result = $director->send_command($cmd, 2);
                }
                elseif($mergefilesets == 1 && $mergejobs == 0) {
                        $cmd = ".bvfs_get_jobids jobid=$jobid";
			$result = $director->send_command($cmd, 2);
                }

		if($mergejobs == 1) {
			return $jobid;
		}

		$jobids = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                $result = "";

		if(!empty($jobids['result'])) {
			$jnum = count($jobids['result']['jobids']);

			foreach($jobids['result']['jobids'] as $jobid) {
				$result .= $jobid['id'];
				--$jnum;
				if($jnum > 0) {
					$result .= ",";
				}
			}
		}

		$this->restore_params['jobids'] = $result;

		return $result;
	}

	/**
	 * Get job list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getJobs($format=2)
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == 2) {
			$result = $director->send_command("llist jobs", 2);
		}
		elseif($format == 1) {
			$result = $director->send_command("list jobs", 2);
		}
		elseif($format == 0) {
		}
		$jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $jobs['result']['jobs'];
	}

	/**
	 * Get client list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getClients($format=2)
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == 2) {
			$result = $director->send_command("llist clients", 2);
		}
		elseif($format == 1) {
			$result = $director->send_command("list clients", 2);
		}
		elseif($format == 0) {
			$result = $director->send_command(".clients", 2);
			return $result;
		}
		$clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
                return $clients['result']['clients'];
	}

	/**
	 * Get fileset list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getFilesets($format=2)
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == 2) {
			$result = $director->send_command("llist filesets", 2);
		}
		elseif($format == 1) {
			$result = $director->send_command("list filesets", 2);
		}
		elseif($format == 0) {
			$result = $director->send_command(".filesets", 2);
		}
		$filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $filesets['result']['filesets'];
	}

	/**
	 * Get restore job list from director
	 */
	private function getRestoreJobs($format=0)
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == 0) {
			$result = $director->send_command(".jobs type=R", 2);
		}
		$restorejobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $restorejobs['result']['jobs'];
	}

	/**
	 * Get client backup list from director
	 */
	private function getClientBackups($client=null, $fileset=null, $order=null, $limit=null)
	{
		$director = $this->getServiceLocator()->get('director');
		if($limit != null) {
			$result = $director->send_command("list backups client=$client fileset=$fileset order=$order limit=$limit", 2);
		}
		else {
			$result = $director->send_command("list backups client=$client fileset=$fileset order=$order", 2);
		}
		$backups = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		if(empty($backups['result'])) {
			return null;
		}
		else {
			return $backups['result']['backups'];
		}
	}

}
