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
	protected $restorejobs;
	protected $jobids;
	protected $backups;

	public function __construct($restore_params=null, $jobs=null, $clients=null, $filesets=null, $restorejobs=null, $jobids=null, $backups=null)
	{

		parent::__construct('restore');

		$this->restore_params = $restore_params;
		$this->jobs = $jobs;
		$this->clients = $clients;
		$this->filesets = $filesets;
		$this->restorejobs = $restorejobs;
		$this->jobids = $jobids;
		$this->backups = $backups;

		// Mode
                if(isset($restore_params['mode'])) {
                        $this->add(array(
                                'name' => 'mode',
                                'type' => 'checkbox',
                                'options' => array(
                                        'label' => 'Expert mode',
					'use_hidden_element' => true,
                                        'checked_value' => '1',
					'unchecked_value' => '0',
                                        ),
                                'attributes' => array(
                                                'id' => 'mode',
                                                'value' => $restore_params['mode']
                                        )
                                )
                        );
                }
                else {
			$this->add(array(
                                'name' => 'mode',
                                'type' => 'checkbox',
                                'options' => array(
                                        'label' => 'Expert mode',
                                        'use_hidden_element' => true,
                                        'checked_value' => '1',
                                        'unchecked_value' => '0',
                                        ),
                                'attributes' => array(
                                                'id' => 'mode',
                                        )
                                )
                        );
                }

		// Job
		if(isset($restore_params['jobid'])) {
			$this->add(array(
				'name' => 'jobid',
				'type' => 'select',
				'options' => array(
					'label' => 'Job',
					'empty_option' => 'Please choose a job',
					'value_options' => $this->getJobList()
				),
				'attributes' => array(
					'id' => 'jobid',
					'value' => $restore_params['jobid']
				)
			));
		}
		else {
			$this->add(array(
				'name' => 'jobid',
				'type' => 'select',
				'options' => array(
					'label' => 'Job',
					'empty_option' => 'Please choose a job',
					'value_options' => $this->getJobList()
				),
				'attributes' => array(
					'id' => 'jobid'
				)
			));
		}

		// Backup
                if(isset($restore_params['jobid'])) {
                        $this->add(array(
                                'name' => 'backups',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Backups',
                                        'empty_option' => 'Please choose a backup',
                                        'value_options' => $this->getBackupList()
                                ),
                                'attributes' => array(
                                        'id' => 'jobid',
                                        'value' => $restore_params['jobid']
                                )
                        ));
                }
                else {
                        $this->add(array(
                                'name' => 'backups',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Backups',
                                        'empty_option' => 'Please choose a backup',
                                        'value_options' => $this->getBackupList()
                                ),
                                'attributes' => array(
                                        'id' => 'jobid'
                                )
                        ));
                }

		// Client
		if(isset($restore_params['client'])) {
			if($restore_params['type'] == "client") {
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
						'id' => 'client',
						'value' => $restore_params['client']
					)
				));
			}
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
                                        'label' => 'Restore to client',
                                        'empty_option' => 'Please choose a client',
                                        'value_options' => $this->getClientList()
                                ),
                                'attributes' => array(
                                        'id' => 'restoreclient',
                                        'value' => $restore_params['restoreclient']
                                )
                        ));
                }
		elseif(!isset($restore_params['restoreclient']) && isset($restore_params['client']) ) {
			$this->add(array(
                                'name' => 'restoreclient',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Restore to client',
                                        'empty_option' => 'Please choose a client',
                                        'value_options' => $this->getClientList()
                                ),
                                'attributes' => array(
                                        'id' => 'restoreclient',
                                        'value' => $restore_params['client']
                                )
                        ));
		}
                else {
                        $this->add(array(
                                'name' => 'restoreclient',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Restore to another client',
                                        'empty_option' => 'Please choose a client',
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

		// Restore Job
                if(isset($restore_params['restorejob'])) {
                        $this->add(array(
                                'name' => 'restorejob',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Restore job',
                                        'empty_option' => 'Please choose a restore job',
                                        'value_options' => $this->getRestoreJobList()
                                ),
                                'attributes' => array(
                                        'id' => 'restorejob',
                                        'value' => $restore_params['restorejob']
                                )
                        ));
                }
                else {
			if(count($this->getRestoreJobList()) == 1) {
				$this->add(array(
                                        'name' => 'restorejob',
                                        'type' => 'select',
                                        'options' => array(
                                                'label' => 'Restore job',
                                                'empty_option' => 'Please choose a restore job',
                                                'value_options' => $this->getRestoreJobList()
                                        ),
                                        'attributes' => array(
                                                'id' => 'restorejob',
						'value' => @array_pop($this->getRestoreJobList())
                                        )
                                ));
			}
			else {
				$this->add(array(
					'name' => 'restorejob',
					'type' => 'select',
					'options' => array(
						'label' => 'Restore job',
						'empty_option' => 'Please choose a restore job',
						'value_options' => $this->getRestoreJobList()
					),
					'attributes' => array(
						'id' => 'restorejob'
					)
				));
			}
                }

		// Merge filesets
		if(isset($restore_params['mergefilesets'])) {
			$this->add(array(
				'name' => 'mergefilesets',
				'type' => 'select',
				'options' => array(
					'label' => 'Merge all client filesets?',
					'value_options' => array(
							'0' => 'Yes',
							'1' => 'No'
						)
					),
				'attributes' => array(
						'id' => 'mergefilesets',
						'value' => $restore_params['mergefilesets']
					)
				)
			);
		}
		else {
			$this->add(array(
                                'name' => 'mergefilesets',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Merge all client filesets?',
                                        'value_options' => array(
                                                        '0' => 'Yes',
                                                        '1' => 'No'
                                                )
                                        ),
                                'attributes' => array(
                                                'id' => 'mergefilesets',
                                                'value' => '0'
                                        )
                                )
                        );
		}

		// Merge jobs
		if(isset($restore_params['mergejobs'])) {
			$this->add(array(
                                'name' => 'mergejobs',
                                'type' => 'select',
                                'options' => array(
                                        'label' => 'Merge jobs?',
                                        'value_options' => array(
                                                        '0' => 'Yes',
                                                        '1' => 'No'
                                                )
                                        ),
                                'attributes' => array(
                                                'id' => 'mergejobs',
                                                'value' => $restore_params['mergejobs']
                                        )
                                )
                        );
		}
		else {
			$this->add(array(
				'name' => 'mergejobs',
				'type' => 'select',
				'options' => array(
					'label' => 'Merge jobs?',
					'value_options' => array(
							'0' => 'Yes',
							'1' => 'No'
						)
					),
				'attributes' => array(
						'id' => 'mergejobs',
						'value' => '0'
					)
				)
			);
		}

		// Replace
		$this->add(array(
                        'name' => 'replace',
                        'type' => 'select',
                        'options' => array(
                                'label' => 'Replace files on client?',
                                'value_options' => array(
                                                'always' => 'always',
                                                'never' => 'never',
						'ifolder' => 'if older',
						'ifnewer' => 'if newer'
                                        )
                                ),
                        'attributes' => array(
					'id' => 'replace',
					'value' => 'never'
                                )
                        )
                );

		// Where
		$this->add(array(
			'name' => 'where',
                        'type' => 'text',
                        'options' => array(
                                'label' => 'Restore location on client'
                                ),
                        'attributes' => array(
                                'value' => '/tmp/bareos-restores/',
				'placeholder' => 'e.g. / or /tmp/bareos-restores/'
                                )
                        )
		);

		// JobIds hidden
                $this->add(array(
                        'name' => 'jobids_hidden',
                        'type' => 'Zend\Form\Element\Hidden',
                        'attributes' => array(
                                'value' => $this->getJobIds(),
                                'id' => 'jobids_hidden'
                                )
                        )
                );

		// JobIds display (for debugging)
		$this->add(array(
			'name' => 'jobids_display',
			'type' => 'text',
			'options' => array(
				'label' => 'Related jobs for a most recent full restore'
				),
			'attributes' => array(
				'value' => $this->getJobIds(),
				'id' => 'jobids_display',
				'readonly' => true
				)
			)
		);

		// filebrowser checked files
		$this->add(array(
			'name' => 'checked_files',
			'type' => 'Zend\Form\Element\Hidden',
			'attributes' => array(
				'value' => '',
				'id' => 'checked_files'
				)
			)
		);

		// filebrowser checked directories
		$this->add(array(
			'name' => 'checked_directories',
			'type' => 'Zend\Form\Element\Hidden',
			'attributes' => array(
				'value' => '',
				'id' => 'checked_directories'
				)
			)
		);

		// Submit button
		$this->add(array(
			'name' => 'submit',
			'type' => 'submit',
			'attributes' => array(
				'value' => 'Restore',
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
				$selectData[$job['jobid']] = $job['jobid'] . " - " . $job['name'];
			}
		}
		return $selectData;
	}

	/**
	 *
	 */
	private function getBackupList()
	{
		$selectData = array();
                if(!empty($this->backups)) {
                        foreach($this->backups as $backup) {
				switch($backup['level']) {
					case 'I':
						$level = "Incremental";
						break;
					case 'D':
						$level = "Differential";
						break;
					case 'F':
						$level = "Full";
						break;
				}
                                $selectData[$backup['jobid']] = "(" . $backup['jobid']  . ") " . $backup['starttime'] . " - " . $backup['fileset'] . " - " . $level;
                        }
                }
                return $selectData;
	}

	/**
	 *
	 */
	private function getJobIds()
	{
		return $this->jobids;
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
				$selectData[$fileset['name']] = $fileset['name'];
			}
		}
		return $selectData;
	}

	/**
         *
         */
        private function getRestoreJobList()
        {
                $selectData = array();
                if(!empty($this->restorejobs)) {
                        foreach($this->restorejobs as $restorejob) {
                                $selectData[$restorejob['name']] = $restorejob['name'];
                        }
                }
                return $selectData;
        }

}
