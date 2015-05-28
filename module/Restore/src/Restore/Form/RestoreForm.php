<?php

/**
*
* bareos-webui - Bareos Web-Frontend
*
* @link      https://github.com/bareos/bareos-webui for the canonical source repository
* @copyright Copyright (c) 2013-2015 dass-IT GmbH (http://www.dass-it.de/)
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

namespace Restore\Form;

use Zend\Form\Form;
use Zend\Form\Element;

class RestoreForm extends Form
{

	protected $restore_params;
	protected $jobs;
	protected $clients;
	protected $filesets;

	public function __construct($restore_params=null, $jobs=null, $clients=null, $filesets=null)
	{

		parent::__construct('restore');

		$this->restore_params = $restore_params;
		$this->jobs = $jobs;
		$this->clients = $clients;
		$this->filesets = $filesets;

		// Job
		if(isset($restore_params['job'])) {
			$this->add(array(
				'name' => 'job',
				'type' => 'select',
				'options' => array(
					'label' => 'Job',
					'empty_option' => 'Please choose a job',
					'value_options' => $this->getJobList()
				),
				'attributes' => array(
					'id' => 'job',
					'value' => $restore_params['job']
				)
			));
		}
		else {
			$this->add(array(
				'name' => 'job',
				'type' => 'select',
				'options' => array(
					'label' => 'Job',
					'empty_option' => 'Please choose a job',
					'value_options' => $this->getJobList()
				),
				'attributes' => array(
					'id' => 'job'
				)
			));
		}

		// Client
		if(isset($restore_params['client'])) {
			$this->add(array(
				'name' => 'client',
				'type' => 'select',
				'options' => array(
					'label' => 'Client',
					'empty_option' => 'Please choose a client',
					'value_options' => $this->getClientList()
				),
				'attributes' => array(
					'id' => 'client',
					'value' => $restore_params['client']
				)
			));
		}
		else {
			$this->add(array(
                                'name' => 'client',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Client',
                                        'empty_option' => 'Please choose a client',
                                        'value_options' => $this->getClientList()
                                ),
                                'attributes' => array(
                                        'id' => 'client'
                                )
                        ));
		}

		// Restore client
		if(isset($restore_params['restoreclient'])) {
                        $this->add(array(
                                'name' => 'restoreclient',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Restore client',
                                        'empty_option' => 'Please choose a restore client',
                                        'value_options' => $this->getClientList()
                                ),
                                'attributes' => array(
                                        'id' => 'restoreclient',
                                        'value' => $restore_params['restoreclient']
                                )
                        ));
                }
                else {
                        $this->add(array(
                                'name' => 'restoreclient',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Restore client',
                                        'empty_option' => 'Please choose a restore client',
                                        'value_options' => $this->getClientList()
                                ),
                                'attributes' => array(
                                        'id' => 'restoreclient'
                                )
                        ));
                }

		// Fileset
		if(isset($restore_params['fileset'])) {
                        $this->add(array(
                                'name' => 'fileset',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Fileset',
                                        'empty_option' => 'Please choose a fileset',
                                        'value_options' => $this->getFilesetList()
                                ),
                                'attributes' => array(
                                        'id' => 'fileset',
                                        'value' => $restore_params['fileset']
                                )
                        ));
                }
                else {
                        $this->add(array(
                                'name' => 'fileset',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Fileset',
                                        'empty_option' => 'Please choose a fileset',
                                        'value_options' => $this->getFilesetList()
                                ),
                                'attributes' => array(
                                        'id' => 'fileset'
                                )
                        ));
                }

		// Before
		$this->add(array(
                        'name' => 'before',
                        'type' => 'date',
                        'options' => array(
                                'label' => 'Before',
				'format' => 'Y-m-d'
                                ),
                        'attributes' => array(
                                'min' => '1970-01-01',
                                'max' => '2020-01-01',
                                'step' => '1',
                                )
                        )
                );

		// Where
		$this->add(array(
			'name' => 'where',
                        'type' => 'text',
                        'options' => array(
                                'label' => 'Where'
                                ),
                        'attributes' => array(
                                'value' => '/tmp/bareos-restores/'
                                )
                        )
		);

		// TODO - Cross site request forgery
                //$this->add(new Element\Csrf('security'));

		// Submit button
		$this->add(array(
			'name' => 'submit',
			'type' => 'submit',
			'attributes' => array(
				'value' => 'Submit',
				'id' => 'submit'
			)
		));

	}

	/**
	 *
	 */
	private function getJobList()
	{
		$selectData = array();
		if(!empty($this->jobs)) {
			foreach($this->jobs as $job) {
				$selectData[$job['jobid']] = $job['jobid'];
			}
		}
		return $selectData;
	}

	/**
	 *
	 */
	private function getClientList()
	{
		$selectData = array();
		if(!empty($this->clients)) {
			foreach($this->clients as $client) {
				$selectData[$client['name']] = $client['name'];
			}
		}
		return $selectData;
	}

	/**
	 *
	 */
	private function getFilesetList()
	{
		$selectData = array();
		if(!empty($this->filesets)) {
			foreach($this->filesets as $fileset) {
				$selectData[$fileset['fileset']] = $fileset['fileset'];
			}
		}
		return $selectData;
	}

}
