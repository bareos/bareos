<?php

/**
*
* bareos-webui - Bareos Web-Frontend
*
* @link      https://github.com/bareos/bareos-webui for the canonical source repository
* @copyright Copyright (c) 2013-2017 dass-IT GmbH (http://www.dass-it.de/)
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

namespace Job\Form;

use Zend\Form\Form;
use Zend\Form\Element;

class RunJobForm extends Form
{
   protected $clients;
   protected $jobs;
   protected $storages;
   protected $pools;
   protected $jobdefaults;

   public function __construct($clients=null, $jobs=null, $filesets=null, $storages=null, $pools=null, $jobdefaults=null)
   {
      parent::__construct('runjob');

      $this->clients = $clients;
      $this->jobs = $jobs;
      $this->filesets = $filesets;
      $this->storages = $storages;
      $this->pools = $pools;
      $this->jobdefaults = $jobdefaults;

      // Client
      if(isset($jobdefaults['client'])) {
         $this->add(array(
            'name' => 'client',
            'type' => 'select',
            'options' => array(
               'label' => _('Client'),
               'empty_option' => '',
               'value_options' => $this->getClientList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'client',
               'value' => $jobdefaults['client']
            )
         ));
      }
      else {
         $this->add(array(
            'name' => 'client',
            'type' => 'select',
            'options' => array(
               'label' => _('Client'),
               'empty_option' => '',
               'value_options' => $this->getClientList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'client',
               'value' => null
            )
         ));
      }

      // Job
      if(isset($jobdefaults['job'])) {
         $this->add(array(
            'name' => 'job',
            'type' => 'select',
            'options' => array(
               'label' => _('Job'),
               'empty_option' => '',
               'value_options' => $this->getJobList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'job',
               'value' => $jobdefaults['job']
            )
         ));
      } else {
         $this->add(array(
            'name' => 'job',
            'type' => 'select',
            'options' => array(
               'label' => _('Job'),
               'empty_option' => '',
               'value_options' => $this->getJobList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'job',
               'value' => null
            )
         ));
      }

      // Fileset
      if(isset($jobdefaults['fileset'])) {
         $this->add(array(
            'name' => 'fileset',
            'type' => 'select',
            'options' => array(
               'label' => _('Fileset'),
               'empty_option' => '',
               'value_options' => $this->getFilesetList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'fileset',
               'value' => $jobdefaults['fileset']
            )
         ));
      } else {
         $this->add(array(
            'name' => 'fileset',
            'type' => 'select',
            'options' => array(
               'label' => _('Fileset'),
               'empty_option' => '',
               'value_options' => $this->getFilesetList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'fileset',
               'value' => null
            )
         ));
      }

      // Storage
      if(isset($jobdefaults['storage'])) {
         $this->add(array(
            'name' => 'storage',
            'type' => 'select',
            'options' => array(
               'label' => _('Storage'),
               'empty_option' => '',
               'value_options' => $this->getStorageList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'storage',
               'value' => $jobdefaults['storage']
            )
         ));
      } else {
         $this->add(array(
            'name' => 'storage',
            'type' => 'select',
            'options' => array(
               'label' => _('Storage'),
               'empty_option' => '',
               'value_options' => $this->getStorageList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'storage',
               'value' => null
            )
         ));
      }

      // Pool
      if(isset($jobdefaults['pool'])) {
         $this->add(array(
            'name' => 'pool',
            'type' => 'select',
            'options' => array(
               'label' => _('Pool'),
               'empty_option' => '',
               'value_options' => $this->getPoolList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'pool',
               'value' => $jobdefaults['pool']
            )
         ));
      } else {
         $this->add(array(
            'name' => 'pool',
            'type' => 'select',
            'options' => array(
               'label' => _('Pool'),
               'empty_option' => '',
               'value_options' => $this->getPoolList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'pool',
               'value' => null
            )
         ));
      }

      // Level
      if(isset($jobdefaults['level'])) {
         $this->add(array(
            'name' => 'level',
            'type' => 'select',
            'options' => array(
               'label' => _('Level'),
               'empty_option' => '',
               'value_options' => $this->getLevelList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'level',
               'value' => $jobdefaults['level']
            )
         ));
      } else {
         $this->add(array(
            'name' => 'level',
            'type' => 'select',
            'options' => array(
               'label' => _('Level'),
               'empty_option' => '',
               'value_options' => $this->getLevelList()
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-live-search' => 'true',
               'id' => 'level',
               'value' => null
            )
         ));
      }

      // Priority
      $this->add(array(
         'name' => 'priority',
         'type' => 'Zend\Form\Element\Text',
         'options' => array(
            'label' => _('Priority'),
         ),
         'attributes' => array(
            'class' => 'form-control',
            'id' => 'priority',
            'placeholder' => '10'
         )
      ));

      // Type
      if(isset($jobdefaults['type'])) {
         $this->add(array(
            'name' => 'type',
            'type' => 'Zend\Form\Element\Text',
            'options' => array(
               'label' => _('Type'),
               'empty_option' => '',
            ),
            'attributes' => array(
               'class' => 'form-control',
               'id' => 'type',
               'value' => $jobdefaults['type'],
               'readonly' => true
            )
         ));
      }
      else {
         $this->add(array(
            'name' => 'type',
            'type' => 'Zend\Form\Element\Text',
            'options' => array(
               'label' => _('Type'),
               'empty_option' => '',
            ),
            'attributes' => array(
               'class' => 'form-control',
               'id' => 'type',
               'value' => null,
               'readonly' => true
            )
         ));
      }

/*
      // Backup Format
      $this->add(array(
         'name' => 'backupformat',
         'type' => 'Zend\Form\Element\Text',
         'options' => array(
            'label' => _('Backup Format'),
         ),
         'attributes' => array(
            'class' => 'form-control',
            'id' => 'backupformat',
            'placeholder' => 'Native'
         )
      ));
*/

      // When
      $this->add(array(
         'name' => 'when',
         'type' => 'Zend\Form\Element\Text',
         'options' => array(
            'label' => _('When'),
         ),
         'attributes' => array(
            'class' => 'form-control',
            'id' => 'when',
            'placeholder' => 'YYYY-MM-DD HH:MM:SS'
         )
      ));

      // Submit button
      $this->add(array(
         'name' => 'submit',
         'type' => 'submit',
         'attributes' => array(
            'class' => 'form-control',
            'value' => _('Submit'),
            'id' => 'submit'
         )
      ));

   }

   private function getClientList()
   {
      $selectData = array();
      if(!empty($this->clients)) {
         foreach($this->clients as $client) {
            $selectData[$client['name']] = $client['name'];
         }
      }
      ksort($selectData);
      return $selectData;
   }

   private function getJobList()
   {
      $selectData = array();
      if(!empty($this->jobs)) {
         foreach($this->jobs as $job) {
            $selectData[$job['name']] = $job['name'];
         }
      }
      ksort($selectData);
      return $selectData;
   }

   private function getFilesetList()
   {
      $selectData = array();
      if(!empty($this->filesets)) {
         foreach($this->filesets as $fileset) {
            $selectData[$fileset['name']] = $fileset['name'];
         }
      }
      ksort($selectData);
      return $selectData;
   }

   private function getStorageList()
   {
      $selectData = array();
      if(!empty($this->storages)) {
         foreach($this->storages as $storage) {
            $selectData[$storage['name']] = $storage['name'];
         }
      }
      ksort($selectData);
      return $selectData;
   }

   private function getPoolList()
   {
      $selectData = array();
      if(!empty($this->pools)) {
         foreach($this->pools as $pool) {
            $selectData[$pool['name']] = $pool['name'];
         }
      }
      ksort($selectData);
      return $selectData;
   }

   private function getLevelList()
   {
      $selectData = array();
      $selectData['Full'] = 'Full';
      $selectData['Differential'] = 'Differential';
      $selectData['Incremental'] = 'Incremental';
      return $selectData;
   }
}
