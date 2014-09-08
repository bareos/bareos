<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
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

return array(

	'controllers' => array(
		'invokables' => array(
			'Storage\Controller\Storage' => 'Storage\Controller\StorageController',
		),
	),

	'router' => array(
		'routes' => array(
			'storage' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/storage[/][:action][/:id][order_by/:order_by][/:order][/][limit/:limit]',
					'constraints' => array(
						'action' => '(?!\blimit\b)(?!\border_by\b)[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
						'id' => '[0-9]+',
                                                'order_by' => '[a-zA-Z][a-zA-Z0-9_-]*',
                                                'order' => 'ASC|DESC',
                                                'limit' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Storage\Controller\Storage',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'storage' => __DIR__ . '/../view',
		),
	),

);
