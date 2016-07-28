<?php

/**
*
* bareos-webui - Bareos Web-Frontend
*
* @link      https://github.com/bareos/bareos-webui for the canonical source repository
* @copyright Copyright (c) 2013-2016 dass-IT GmbH (http://www.dass-it.de/)
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

namespace Storage\Form;

use Zend\Form\Form;
use Zend\Form\Element;

class StorageForm extends Form
{
   protected $storage = null;
   protected $pools = null;
   protected $drives = null;

   public function __construct($storage=null, $pools=null, $drives=null)
   {

      parent::__construct('storage');

      $this->storage = $storage;
      $this->pools = $pools;
      $this->drives = $drives;

      // storage
      $this->add(array(
         'name' => 'storage',
         'type' => 'Zend\Form\Element\Hidden',
         'attributes' => array(
            'value' => $this->storage,
            'id' => 'storage'
         )
      ));

      // pool
      $this->add(array(
         'name' => 'pool',
         'type' => 'select',
         'options' => array(
            'label' => _('Pool'),
            //'empty_option' => _('Please choose a pool'),
            'value_options' => $this->getPoolList()
         ),
         'attributes' => array(
            'class' => 'form-control selectpicker show-tick',
            'data-live-search' => 'true',
            'id' => 'pool'
         )
      ));

      // drive
      $this->add(array(
         'name' => 'drive',
         'type' => 'select',
         'options' => array(
            'label' => _('Drive'),
            //'empty_option' => _('Please choose a drive'),
            'value_options' => $this->getDriveList()
         ),
         'attributes' => array(
            'class' => 'form-control selectpicker show-tick',
            'data-live-search' => 'true',
            'id' => 'drive'
         )
      ));

      // submit button
      $this->add(array(
         'name' => 'submit',
         'type' => 'submit',
         'attributes' => array(
            'value' => _('Submit'),
            'id' => 'submit'
         )
      ));

   }

   private function getPoolList()
   {
      $selectData = array();
      if(!empty($this->pools)) {
         foreach($this->pools as $pool) {
            $selectData[$pool['name']] = $pool['name'];
         }
      }
      return $selectData;
   }

   private function getDriveList()
   {
      $selectData = array();
      if(!empty($this->drives)) {
         foreach($this->drives as $drive) {
            $selectData[$drive] = $drive;
         }
      }
      return $selectData;
   }

}
