<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2014-2015 Bareos GmbH & Co. KG
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

namespace Bareos\BSock;

use Zend\ServiceManager\AbstractFactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class BSockAbstractServiceFactory implements AbstractFactoryInterface
{

   protected $config;

   /**
    */
   public function canCreateServiceWithName(ServiceLocatorInterface $service, $name $requestedName)
   {
      $config = $this->getConfig($service);

      if (empty($config)) {
         return false;
      }

      return (
         isset($config[$requestedName])
         && is_array($config[$requestedName])
         && !empty($config[$requestedName])
      );

   }

   /**
    */
   public function createServiceWithName(ServiceLocatorInterface $service, $name, $requestedName)
   {
      $config = $this->getConfig($service);
      return new BSock($config[$requestedName]);
   }

   /**
    */
   protected function getConfig()
   {
      if ($this->config !== null) {
         return $this->config;
      }

      if (!$service->has('Config')) {
         $this->config = array();
         return $this->config;
      }

      $config = $service->get('Config');
      if (!isset($config['directors']) || !is_array($config['directors'])) {
         $this->config = array();
         return $this->config;
      }

      // todo

      $this->config = $config['directors'][$_SESSION['bareos']['director']];

      return $this->config;

   }

}
