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

namespace Job\Form;

use Zend\Form\Form;
use Zend\Form\Element;

class JobForm extends Form
{

   protected $period;
   protected $status;

   public function __construct($period=null, $status=null)
   {
      parent::__construct('job');

      $this->period = $period;
      $this->status = $status;

      if(isset($period)) {
         $this->add(array(
            'name' => 'period',
            'type' => 'select',
            'options' => array(
               'label' => _('Time period'),
               'value_options' => array(
                  '1' => _('last 24 hours'),
                  '3' => _('last 3 days'),
                  '7' => _('last week'),
                  '31' => _('last month'),
                  '365' => _('last year'),
                  'all' => _('unlimited')
               )
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-size' => '6',
               'id' => _('period'),
               'value' => $period
            )
         ));
      }

      if(isset($status)) {
         $this->add(array(
            'name' => 'status',
            'type' => 'select',
            'options' => array(
               'label' => _('Status'),
               'value_options' => array(
                  'all' => _('all'),
                  'running' => _('running'),
                  'waiting' => _('waiting'),
                  'unsuccessful' => _('terminated unsuccessfully'),
                  'successful' => _('terminated successfully')
               )
            ),
            'attributes' => array(
               'class' => 'form-control selectpicker show-tick',
               'data-size' => '5',
               'id' => 'status',
               'value' => $status
            )
         ));
      }

   }

}

