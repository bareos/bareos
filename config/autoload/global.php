<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

/**
 * Global Configuration Override
 *
 * You can use this file for overriding configuration values from modules, etc.
 * You would place values in here that are agnostic to the environment and not
 * sensitive to security.
 *
 * @NOTE: In practice, this file will typically be INCLUDED in your source
 * control, so do not include passwords or other sensitive information in this
 * file.
 */

$directors_ini = "/etc/bareos-webui/directors.ini";
$configuration_ini = "/etc/bareos-webui/configuration.ini";

$directors = null;
$configuration = null;

// Check for existing directors.ini and parse if present
if(!file_exists($directors_ini)) {
   echo "Error: Missing configuration file ".$directors_ini.".";
   exit();
}
else {
   // As of PHP 5.6.1 can also be specified as INI_SCANNER_TYPED. In this mode boolean,
   // null and integer types are preserved when possible. String values "true", "on" and
   // "yes" are converted to TRUE. "false", "off", "no" and "none" are considered FALSE.
   // "null" is converted to NULL in typed mode. Also, all numeric strings are converted
   // to integer type if it is possible.
   //
   // In future we might want to apply the INI_SCANNER_TYPED mode to simplify things.
   //
   // See: http://php.net/manual/en/function.parse-ini-file.php
   $directors = parse_ini_file($directors_ini, true, INI_SCANNER_NORMAL);
}

// Check for existing configuration.ini and parse if present
if(!file_exists($configuration_ini)) {
   echo "Error: Missing configuration file ".$configuration_ini.".";
   exit();
}
else {
   $configuration = parse_ini_file($configuration_ini, true, INI_SCANNER_NORMAL);
}

// read configuration.ini
function read_configuration_ini($configuration, $configuration_ini)
{
   $arr = array();

   if( array_key_exists('session', $configuration) && array_key_exists('timeout', $configuration['session']) && isset($configuration['session']['timeout']) ) {
      $arr['session']['timeout'] = $configuration['session']['timeout'];
   }
   else {
      $arr['session']['timeout'] = 3600;
   }

   if( array_key_exists('tables', $configuration) && array_key_exists('pagination_values', $configuration['tables']) && isset($configuration['tables']['pagination_values']) ) {
      $arr['tables']['pagination_values'] = $configuration['tables']['pagination_values'];
   }
   else {
      $arr['tables']['pagination_values'] = "10,25,50,100";
   }

   if( array_key_exists('tables', $configuration) && array_key_exists('pagination_default_value', $configuration['tables']) && isset($configuration['tables']['pagination_default_value']) ) {
      $arr['tables']['pagination_default_value'] = $configuration['tables']['pagination_default_value'];
   }
   else {
      $arr['tables']['pagination_default_value'] = 25;
   }

   if( array_key_exists('tables', $configuration) && array_key_exists('save_previous_state', $configuration['tables']) && isset($configuration['tables']['save_previous_state']) ) {
      $arr['tables']['save_previous_state'] = $configuration['tables']['save_previous_state'];
   }
   else {
      $arr['tables']['save_previous_state'] = false;
   }

   if( array_key_exists('autochanger', $configuration) && array_key_exists('labelpooltype', $configuration['autochanger']) && isset($configuration['autochanger']['labelpooltype']) ) {
      $arr['autochanger']['labelpooltype'] = $configuration['autochanger']['labelpooltype'];
   }

   return $arr;
}

// read directors.ini
function read_directors_ini($directors, $directors_ini)
{

   $arr = array();

   foreach($directors as $instance) {

      if(array_key_exists('enabled', $instance) && isset($instance['enabled']) &&
         (strtolower($instance['enabled']) === "yes" || strtolower($instance['enabled']) === "true" || $instance['enabled'] == 1)) {

         if(array_key_exists('diraddress', $instance) && isset($instance['diraddress'])) {
            $arr[key($directors)] = array();
            $arr[key($directors)]['host'] = $instance['diraddress'];
         }
         else {
            echo "Error: Missing parameter 'diraddress' in ".$file.", section ".key($directors).".";
            exit();
         }

         if(array_key_exists('dirport', $instance) && isset($instance['dirport'])) {
            $arr[key($directors)]['port'] = $instance['dirport'];
         }
         else {
            $arr[key($directors)]['port'] = 9101;
         }

         if(array_key_exists('tls_verify_peer', $instance) && isset($instance['tls_verify_peer'])) {
            $arr[key($directors)]['tls_verify_peer'] = $instance['tls_verify_peer'];
         }
         else {
            $arr[key($directors)]['tls_verify_peer'] = false;
         }

         if(array_key_exists('server_can_do_tls', $instance) && isset($instance['server_can_do_tls'])) {
            $arr[key($directors)]['server_can_do_tls'] = $instance['server_can_do_tls'];
         }
         else {
         }

         if(array_key_exists('server_requires_tls', $instance) && isset($instance['server_requires_tls'])) {
            $arr[key($directors)]['server_requires_tls'] = $instance['server_requires_tls'];
         }
         else {
            $arr[key($directors)]['server_requires_tls'] = false;
         }

         if(array_key_exists('client_can_do_tls', $instance) && isset($instance['client_can_do_tls'])) {
            $arr[key($directors)]['client_can_do_tls'] = $instance['client_can_do_tls'];
         }
         else {
            $arr[key($directors)]['client_can_do_tls'] = false;
         }

         if(array_key_exists('client_requires_tls', $instance) && isset($instance['client_requires_tls'])) {
            $arr[key($directors)]['client_requires_tls'] = $instance['client_requires_tls'];
         }
         else {
            $arr[key($directors)]['client_requires_tls'] = false;
         }

         if(array_key_exists('ca_file', $instance) && isset($instance['ca_file'])) {
            $arr[key($directors)]['ca_file'] = $instance['ca_file'];
         }
         else {
            $arr[key($directors)]['ca_file'] = null;
         }

         if(array_key_exists('cert_file', $instance) && isset($instance['cert_file'])) {
            $arr[key($directors)]['cert_file'] = $instance['cert_file'];
         }
         else {
            $arr[key($directors)]['cert_file'] = null;
         }

         if(array_key_exists('cert_file_passphrase', $instance) && isset($instance['cert_file_passphrase'])) {
            $arr[key($directors)]['cert_file_passphrase'] = $instance['cert_file_passphrase'];
         }
         else {
            $arr[key($directors)]['cert_file_passphrase'] = null;
         }

         if(array_key_exists('allowed_cns', $instance) && isset($instance['allowed_cns'])) {
            $arr[key($directors)]['allowed_cns'] = $instance['allowed_cns'];
         }
         else {
            $arr[key($directors)]['allowed_cns'] = null;
         }

         if(array_key_exists('catalog', $instance) && isset($instance['catalog'])) {
            $arr[key($directors)]['catalog'] = $instance['catalog'];
         }
         else {
            $arr[key($directors)]['catalog'] = null;
         }
      }

      next($directors);

   }

   return $arr;

}

return array(
   'directors' => read_directors_ini($directors, $directors_ini),
   'configuration' => read_configuration_ini($configuration, $configuration_ini),
   'service_manager' => array(
      'factories' => array(
         'Zend\Session\Config\ConfigInterface' => 'Zend\Session\Service\SessionConfigFactory',
         //'Zend\Db\Adapter\Adapter' => 'Zend\Db\Adapter\AdapterServiceFactory',
      ),
      'abstract_factories' => array(
         // to allow other adapters to be called by $sm->get('adaptername')
         //'Zend\Db\Adapter\AdapterAbstractServiceFactory',
      ),
   ),
   'session' => array(
      'config' => array(
         'class' => 'Zend\Session\Config\SessionConfig',
         'options' => array(
            'name' => 'bareos',
            'use_cookies' => true,
            'cookie_lifetime' => '0', // to reset lifetime to maximum at every click
            'gc_maxlifetime' => '3600',
            'cache_expire' => 3600,
            'remember_me_seconds' => 3600,
            'use_cookies' => true
         ),
   ),
   'storage' => 'Zend\Session\Storage\SessionArrayStorage',
   'validators' => array(
       'Zend\Session\Validator\RemoteAddr',
       'Zend\Session\Validator\HttpUserAgent',
   ),
    ),
);

