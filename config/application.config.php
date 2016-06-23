<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

$env = getenv('APPLICATION_ENV') ?: 'production';

// Use the $env value to dtermine which module to load
$modules = array(
   'Application',
   'Dashboard',
   'Director',
   'Fileset',
   'Pool',
   'Media',
   'Storage',
   'Client',
   'Schedule',
   'Job',
   'Restore',
   'Auth',
);


if($env == 'development') {
   //$modules[] = 'ZendDeveloperTools';  // you may comment only this line out if ZendDeveloperTools are not installed e.g.
   error_reporting(E_ALL);
   ini_set("display_errors", 1);
}

return array(
    // This should be an array of module namespaces used in the application.
    'modules' => $modules,

    // These are various options for the listeners attached to the ModuleManager
    'module_listener_options' => array(
   // This should be an array of paths in which modules reside.
   // If a string key is provided, the listener will consider that a module
   // namespace, the value of that key the specific path to that module's
   // Module class.
   'module_paths' => array(
       './module',
       './vendor',
   ),

   // An array of paths from which to glob configuration files after
   // modules are loaded. These effectively override configuration
   // provided by modules themselves. Paths may use GLOB_BRACE notation.
   'config_glob_paths' => array(
       'config/autoload/{,*.}{global,local}.php',
   ),

   // Whether or not to enable a configuration cache.
   // If enabled, the merged configuration will be cached and used in
   // subsequent requests.
   //'config_cache_enabled' => $booleanValue,
      //'config_cache_enabled' => ($env == 'production'),

   // The key used to create the configuration cache file name.
   //'config_cache_key' => $stringKey,
      //'config_cache_key' => 'app_config',

   // Whether or not to enable a module class map cache.
   // If enabled, creates a module class map cache which will be used
   // by in future requests, to reduce the autoloading process.
   //'module_map_cache_enabled' => $booleanValue,
      //'module_map_cache_enabled' => ($env == 'production'),

   // The key used to create the class map cache file name.
   //'module_map_cache_key' => $stringKey,
      //'module_map_cache_key' => 'module_map',

   // The path in which to cache merged configuration.
   //'cache_dir' => $stringPath,
      //'cache_dir' => 'data/config/',

   // Whether or not to enable modules dependency checking.
   // Enabled by default, prevents usage of modules that depend on other modules
   // that weren't loaded.
   // 'check_dependencies' => true,
      //'check_dependencies' => ($env != 'production'),
    ),

    // Used to create an own service manager. May contain one or more child arrays.
    //'service_listener_options' => array(
    //     array(
    //    'service_manager' => $stringServiceManagerName,
    //    'config_key'      => $stringConfigKey,
    //    'interface'       => $stringOptionalInterface,
    //    'method'     => $stringRequiredMethodName,
    //     ),
    // )

   // Initial configuration with which to seed the ServiceManager.
   // Should be compatible with Zend\ServiceManager\Config.
   // 'service_manager' => array(),
);

