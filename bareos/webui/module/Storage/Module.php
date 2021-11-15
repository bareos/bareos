<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2019 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Storage;

use Storage\Model\Storage;
use Storage\Model\StorageModel;

class Module
{

   public function getAutoloaderConfig()
   {
      return array(
         'Zend\Loader\ClassMapAutoloader' => array(
            __DIR__ . '/autoload_classmap.php',
         ),
         'Zend\Loader\StandardAutoloader' => array(
            'namespaces' => array(
               __NAMESPACE__ => __DIR__ . '/src/' . __NAMESPACE__,
            ),
         ),
      );
   }

   public function getConfig()
   {
     return include __DIR__ . '/config/module.config.php';
   }

   public function getServiceConfig()
   {
      return array(
         'factories' => array(
            'Storage\Model\StorageModel' => function() {
               $model = new StorageModel();
               return $model;
            }
         )
      );
   }

}
