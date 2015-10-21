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
					'label' => 'Period',
					'value_options' => array(
						'1' => 'past 24 hours',
						'7' => 'last week',
						'31' => 'last month',
						'365' => 'last year',
						'all' => 'all'
					)
				),
				'attributes' => array(
					'class' => 'selectpicker',
					'data-size' => '5',
					'id' => 'period',
					'value' => $period
				)
			));
		}

		if(isset($status)) {
			$this->add(array(
				'name' => 'status',
				'type' => 'select',
				'options' => array(
					'label' => 'Status',
					'value_options' => array(
						'all' => 'all',
						'running' => 'running',
						'waiting' => 'waiting',
						'unsuccessful' => 'unsuccessful',
						'successful' => 'successful'
					)
				),
				'attributes' => array(
					'class' => 'selectpicker',
                                        'data-size' => '5',
					'id' => 'status',
					'value' => $status
				)
			));
		}

	}

}

