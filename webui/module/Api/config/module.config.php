<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
            'Api\Controller\Analytics' => 'Api\Controller\AnalyticsController',
            'Api\Controller\Api' => 'Api\Controller\ApiController',
            'Api\Controller\Client' => 'Api\Controller\ClientController',
            'Api\Controller\Console' => 'Api\Controller\ConsoleController',
            'Api\Controller\Director' => 'Api\Controller\DirectorController',
            'Api\Controller\DotJob' => 'Api\Controller\DotJobController',
            'Api\Controller\ExecuteOnDir' => 'Api\Controller\ExecuteOnDirController',
            'Api\Controller\Fileset' => 'Api\Controller\FilesetController',
            'Api\Controller\Job' => 'Api\Controller\JobController',
            'Api\Controller\JobLog' => 'Api\Controller\JobLogController',
            'Api\Controller\JobTotals' => 'Api\Controller\JobTotalsController',
            'Api\Controller\Media' => 'Api\Controller\MediaController',
            'Api\Controller\Pool' => 'Api\Controller\PoolController',
            'Api\Controller\Schedule' => 'Api\Controller\ScheduleController',
            'Api\Controller\Storage' => 'Api\Controller\StorageController',
            'Api\Controller\Timeline' => 'Api\Controller\TimelineController',
        ),
    ),
    'controller_plugins' => array(
        'invokables' => array(
            'SessionTimeoutPlugin' => 'Application\Controller\Plugin\SessionTimeoutPlugin',
        ),
    ),
    'router' => array(
        'routes' => array(
            'api' => array(
                'type' => Literal::class,
                'options' => array(
                    'route' => '/api',
                    'defaults' => array(
                        'controller' => 'Api\Controller\Api',
                    ),
                ),
                'may_terminate' => true,
                'child_routes' => array(
                    'job' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/job[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Job',
                            )
                        ),
                    ),
                    'joblog' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/joblog[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\JobLog',
                            ),
                        ),
                    ),
                    'jobtotals' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/jobtotals[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\JobTotals',
                            ),
                        ),
                    ),
                    'dotjob' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/dotjob[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\DotJob',
                            ),
                        ),
                    ),
                    'timeline' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/timeline[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Timeline',
                            ),
                        ),
                    ),
                    'execute' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/execute[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\ExecuteOnDir',
                            ),
                        ),
                    ),
                    'client' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/client[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Client',
                            ),
                        ),
                    ),
                    'fileset' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/fileset[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Fileset',
                            ),
                        ),
                    ),
                    'schedule' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/schedule[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Schedule',
                            ),
                        ),
                    ),
                    'storage' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/storage[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Storage',
                            ),
                        ),
                    ),
                    'pool' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/pool[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Pool',
                            ),
                        ),
                    ),
                    'media' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/media[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Media',
                            ),
                        ),
                    ),
                    'director' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/director[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Director',
                            ),
                        ),
                    ),
                    'console' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/console[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Console',
                            ),
                        ),
                    ),
                    'analyse' => array(
                        'type' => Segment::class,
                        'options' => array(
                            'route' => '/analyse[/][:id]',
                            'constraints' => array(
                                'id' => '[a-zA-Z0-9][a-zA-Z0-9\._-]*',
                            ),
                            'defaults' => array(
                                'controller' => 'Api\Controller\Analytics',
                            ),
                        ),
                    ),
                ),
            ),
        ),
    ),
    'view_manager' => array(
        'strategies' => array(
            'ViewJsonStrategy',
        ),
    ),
);
