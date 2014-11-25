<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 * @author    Frank Bergkemper
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

$file = "/etc/bareos-webui/directors.ini";
$config = null;

if(!file_exists($file)) {
	echo "Error: Missing configuration file ".$file.".";
        exit();
}
else {
	$config = parse_ini_file($file, true, INI_SCANNER_NORMAL);
}

function read_db_config($config, $file)
{

	$arr = array();

	foreach($config as $instance) {

		if(array_key_exists('enabled', $instance) && isset($instance['enabled']) && strtolower($instance['enabled']) == "no") {

			next($config);

                }
		else {

			if(array_key_exists('dbaddress', $instance) && isset($instance['dbaddress'])) {
                                $arr['adapters'][$instance['dbaddress']] = array();
                        }
                        else {
                                if(array_key_exists('diraddress', $instance) && isset($instance['diraddress'])) {
                                        $arr['adapters'][$instance['diraddress']] = array();
                                        $instance['dbaddress'] = $instance['diraddress'];
                                }
                                else {
                                        echo "Error: Missing parameters 'dbaddress' and 'diraddress' in ".$file.", section ".key($config).".";
                                        exit();
                                }
                        }

			if(array_key_exists('dbdriver', $instance) && isset($instance['dbdriver'])) {
                                if(strtolower($instance['dbdriver']) == "postgresql") {
                                        $arr['adapters'][$instance['dbaddress']]['driver'] = "Pdo_Pgsql";
                                }
                                elseif(strtolower($instance['dbdriver']) == "mysql") {
                                        $arr['adapters'][$instance['dbaddress']]['driver'] = "Pdo_Mysql";
                                }
                                else {
                                        echo "Error: Mispelled value for parameter 'dbdriver' in ".$file.", section ".key($config).".";
                                        exit();
                                }
                        }
                        else {
                                $arr['adapters'][$instance['dbaddress']]['driver'] = "Pdo_Pgsql";
                        }

			if(array_key_exists('dbname', $instance) && isset($instance['dbname'])) {
                                $arr['adapters'][$instance['dbaddress']]['dbname'] = $instance['dbname'];
                        }
                        else {
                                $arr['adapters'][$instance['dbaddress']]['dbname'] = "bareos";
                        }

                        if(array_key_exists('dbaddress', $instance) && isset($instance['dbaddress'])) {
                                $arr['adapters'][$instance['dbaddress']]['host'] = $instance['dbaddress'];
                        }
                        else {
                                $arr['adapters'][$instance['dbaddress']]['host'] = "127.0.0.1";
                        }

                        if(array_key_exists('dbport', $instance) && isset($instance['dbport'])) {
                                $arr['adapters'][$instance['dbaddress']]['port'] = $instance['dbport'];
                        }
                        else {
                                if($arr['adapters'][$instance['dbaddress']]['driver'] == "Pdo_Pgsql") {
                                        $arr['adapters'][$instance['dbaddress']]['port'] = 5432;
                                }
                                else {
                                        $arr['adapters'][$instance['dbaddress']]['port'] = 3306;
                                }
                        }

			if(array_key_exists('dbuser', $instance) && isset($instance['dbuser'])) {
                                $arr['adapters'][$instance['dbaddress']]['username'] = $instance['dbuser'];
                        }
                        else {
                                $arr['adapters'][$instance['dbaddress']]['username'] = "bareos";
                        }

                        if(array_key_exists('dbpassword', $instance) && isset($instance['dbpassword'])) {
                                $arr['adapters'][$instance['dbaddress']]['password'] = $instance['dbpassword'];
                        }
                        else {
                                $arr['adapters'][$instance['dbaddress']]['password'] = "";
                        }

		}

	}

	return $arr;

}

function read_dir_config($config, $file)
{

	$arr = array();

	foreach($config as $instance) {

                if(array_key_exists('enabled', $instance) && isset($instance['enabled']) && strtolower($instance['enabled']) == "no") {

                        next($config);

                }
                else {

			if(array_key_exists('diraddress', $instance) && isset($instance['diraddress'])) {
                                $arr[$instance['diraddress']] = array();
                                $arr[$instance['diraddress']]['host'] = $instance['diraddress'];
                        }
                        else {
                                echo "Error: Missing parameter 'diraddress' in ".$file.", section ".key($config).".";
                                exit();
                        }

                        if(array_key_exists('dirport', $instance) && isset($instance['dirport'])) {
                                $arr[$instance['diraddress']]['port'] = $instance['dirport'];
                        }
                        else {
                                $arr[$instance['diraddress']]['port'] = 9101;
                        }

			if(array_key_exists('tls_verify_peer', $instance) && isset($instance['tls_verify_peer'])) {
                                $arr[$instance['diraddress']]['tls_verify_peer'] = $instance['tls_verify_peer'];
                        }
                        else {
                                $arr[$instance['diraddress']]['tls_verify_peer'] = false;
                        }

                        if(array_key_exists('server_can_do_tls', $instance) && isset($instance['server_can_do_tls'])) {
                                $arr[$instance['diraddress']]['server_can_do_tls'] = $instance['server_can_do_tls'];
                        }
                        else {
                                $arr[$instance['diraddress']]['server_can_do_tls'] = false;
                        }

                        if(array_key_exists('server_requires_tls', $instance) && isset($instance['server_requires_tls'])) {
                                $arr[$instance['diraddress']]['server_requires_tls'] = $instance['server_requires_tls'];
                        }
                        else {
                                $arr[$instance['diraddress']]['server_requires_tls'] = false;
                        }

                        if(array_key_exists('client_can_do_tls', $instance) && isset($instance['client_can_do_tls'])) {
                                $arr[$instance['diraddress']]['client_can_do_tls'] = $instance['client_can_do_tls'];
                        }
                        else {
                                $arr[$instance['diraddress']]['client_can_do_tls'] = false;
                        }

                        if(array_key_exists('client_requires_tls', $instance) && isset($instance['client_requires_tls'])) {
                                $arr[$instance['diraddress']]['client_requires_tls'] = $instance['client_requires_tls'];
                        }
                        else {
                                $arr[$instance['diraddress']]['client_requires_tls'] = false;
                        }

			if(array_key_exists('ca_file', $instance) && isset($instance['ca_file'])) {
                                $arr[$instance['diraddress']]['ca_file'] = $instance['ca_file'];
                        }
                        else {
                                $arr[$instance['diraddress']]['ca_file'] = "";
                        }

                        if(array_key_exists('cert_file', $instance) && isset($instance['cert_file'])) {
                                $arr[$instance['diraddress']]['cert_file'] = $instance['cert_file'];
                        }
                        else {
                                $arr[$instance['diraddress']]['cert_file'] = "";
                        }

                        if(array_key_exists('cert_file_passphrase', $instance) && isset($instance['cert_file_passphrase'])) {
                                $arr[$instance['diraddress']]['cert_file_passphrase'] = $instance['cert_file_passphrase'];
                        }
                        else {
                                $arr[$instance['diraddress']]['cert_file_passphrase'] = "";
                        }

                        if(array_key_exists('allowed_cns', $instance) && isset($instance['allowed_cns'])) {
                                $arr[$instance['diraddress']]['allowed_cns'] = $instance['allowed_cns'];
                        }
                        else {
                                $arr[$instance['diraddress']]['allowed_cns'] = "";
                        }

		}

	}

	return $arr;

}

return array(
	'db' => read_db_config($config, $file),
	'directors' => read_dir_config($config, $file),
	'service_manager' => array(
		'factories' => array(
			'Zend\Db\Adapter\Adapter' => 'Zend\Db\Adapter\AdapterServiceFactory',
		),
		'abstract_factories' => array(
			// to allow other adapters to be called by $sm->get('adaptername')
			'Zend\Db\Adapter\AdapterAbstractServiceFactory',
		),
	),
	'session' => array(
        'config' => array(
            'class' => 'Zend\Session\Config\SessionConfig',
            'options' => array(
                'name' => 'Bareos-WebUI',
            ),
        ),
        'storage' => 'Zend\Session\Storage\SessionArrayStorage',
        'validators' => array(
            'Zend\Session\Validator\RemoteAddr',
            'Zend\Session\Validator\HttpUserAgent',
        ),
    ),
);

