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
use Zend\ViewModel\JsonModel;
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
		if($_SESSION['bareos']['authenticated'] == true) {

			$this->getRestoreParams();

			$form = new RestoreForm(
				$this->restore_params,
				$this->getJobs("long"),
				$this->getClients("long"),
				$this->getFilesets("long")
			);

			// Set the method attribute for the form
			$form->setAttribute('method', 'post');

			$request = $this->getRequest();

			if($request->isPost()) {

				$restore = new Restore();
				$form->setInputFilter($restore->getInputFilter());
				$form->setData( $request->getPost() );

				if($form->isValid()) {

					$job = $form->getInputFilter()->getValue('job');
					$client = $form->getInputFilter()->getValue('client');
					$restoreclient = $form->getInputFilter()->getValue('restoreclient');
					$fileset = $form->getInputFilter()->getValue('fileset');
					$before = $form->getInputFilter()->getValue('before');
					$where = $form->getInputFilter()->getValue('where');

					return $this->redirect()->toRoute(
						'restore',
						array(
							'controller'=>'restore',
							'action' => 'queued'
						),
						array(
							'query' => array(
								'type' => $this->restore_params['type'],
								'job' => $this->restore_params['job'],
								'client' => $client,
								'restoreclient' => $restoreclient,
								'fileset' => $fileset,
								'where' => $where
							)
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
	 *
	 */
	public function confirmAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {

                        // TODO

			// 1. display restore job summary
			// 2. submit restore job
			// 3. modify restore job
			// 4. cancel restore job

			$this->getRestoreParams();

                        return new ViewModel(
			);

                }
                else {
                        return $this->redirect()->toRoute('auth', array('action' => 'login'));
                }
	}

	/**
	 *
	 */
	public function queuedAction()
	{
		if($_SESSION['bareos']['authenticated'] == true) {

			$this->getRestoreParams();

			$result = $this->restore(
				$this->restore_params['type'],
				$this->restore_params['job'],
				$this->restore_params['client'],
				$this->restore_params['restoreclient'],
				$this->restore_params['fileset'],
				$this->restore_params['where']
			);

			return new ViewModel(array(
				'result' => $result
			));

		}
		else {
			return $this->redirect()->toRoute('auth', array('action' => 'login'));
		}
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
                        $this->restore_params['type'] = null;
                }

		if($this->params()->fromQuery('job')) {
			$this->restore_params['job'] = $this->params()->fromQuery('job');
		}
		else {
			$this->restore_params['job'] = null;
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

		if($this->params()->fromQuery('fileset')) {
                        $this->restore_params['fileset'] = $this->params()->fromQuery('fileset');
                }
                else {
                        $this->restore_params['fileset'] = null;
                }

		if($this->params()->fromQuery('where')) {
                        $this->restore_params['where'] = $this->params()->fromQuery('where');
                }
                else {
                        $this->restore_params['where'] = null;
                }
	}

	/**
	 *
	 */
	private function restore($type=null, $job=null, $client=null, $restoreclient=null, $fileset=null, $where=null)
	{
		$result = null;
		$director = $this->getServiceLocator()->get('director');

		switch ($type) {
			// Restore most recent client (full)
			case 1:
				$cmd = "restore client=$client restoreclient=$restoreclient fileset=$fileset where=$where current select all done yes";
				$result = $director->send_command($cmd, 0);
				break;
			// Restore specific job (jobid)
			case 2:
				$cmd = "restore jobid=$job client=$client restoreclient=$restoreclient where=$where all done yes";
				$result = $director->send_command($cmd, 0);
				break;
			default:
				break;
		}

		return $result;
	}

	/**
	 * Get job list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getJobs($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist jobs", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list jobs", 2);
		}
		$jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $jobs['result'][0]['jobs'];
	}

	/**
	 * Get client list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getClients($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist clients", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list clients", 2);
		}
		$clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $clients['result'][0]['clients'];
	}

	/**
	 * Get fileset list from director in long or short format
	 *
	 * @param $format
	 * @return array
	 */
	private function getFilesets($format="long")
	{
		$director = $this->getServiceLocator()->get('director');
		if($format == "long") {
			$result = $director->send_command("llist filesets", 2);
		}
		elseif($format == "short") {
			$result = $director->send_command("list filesets", 2);
		}
		$filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
		return $filesets['result'][0]['filesets'];
	}

}

