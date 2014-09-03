<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
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

namespace Install\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Version\Version;
use Zend\Db\Adapter\Driver\ConnectionInterface;
use \Exception;

class InstallController extends AbstractActionController 
{
	
	const REQUIRED_PHP_VERSION = "5.3";
	const REQUIRED_ZF_VERSION = "2.2";	
	const REQUIRED_EXT_PDO = "YES";
	const REQUIRED_EXT_INTL = "YES";
	const REQUIRED_DB_DRIVER = "YES";
	const REQUIRED_DB_HOST = "YES";
	const REQUIRED_DB_USER = "YES";
	const REQUIRED_DB_PASSWORD = "YES";
	const REQUIRED_BCONSOLE_EXEC = "YES";
	const REQUIRED_BCONSOLE_CONF = "YES";
	const REQUIRED_BCONSOLE_SUDO = "NO";

	public function indexAction()
	{
		$this->layout('layout/install');
		$viewModel = new ViewModel();
		return $viewModel;
	}

	public function testAction()
	{
		$this->layout('layout/install');
		$viewModel = new ViewModel(array(
			'INSTALLED_PHP_VERSION' => $this->getInstalledPHPVersion(),
			'REQUIRED_PHP_VERSION' => self::REQUIRED_PHP_VERSION,
			'PHP_VERSION_CHECK' => $this->compareVersions($this->getInstalledPHPVersion(), self::REQUIRED_PHP_VERSION),
			'INSTALLED_ZF_VERSION' => $this->getInstalledZFVersion(),
			'REQUIRED_ZF_VERSION' => self::REQUIRED_ZF_VERSION,
			'ZF_VERSION_CHECK' => $this->compareVersions($this->getInstalledZFVersion(), self::REQUIRED_ZF_VERSION),
			'INSTALLED_EXT_PDO' => $this->getInstalledPDOExt(),
			'REQUIRED_EXT_PDO' => self::REQUIRED_EXT_PDO,
			'EXT_PDO_CHECK' => $this->getPDOExtStatus(),
			'INSTALLED_EXT_INTL' => $this->getInstalledIntlExt(),
			'REQUIRED_EXT_INTL' => self::REQUIRED_EXT_INTL,
			'EXT_INTL_CHECK' => $this->getIntlExtStatus(),	
			'REQUIRED_DB_DRIVER' => self::REQUIRED_DB_DRIVER,
			'CONFIGURED_DB_DRIVER' => $this->getConfiguredDbDriver(),
			'DB_DRIVER_CHECK' => $this->getDbDriverStatus(),
			'REQUIRED_DB_HOST' => self::REQUIRED_DB_HOST,
			'CONFIGURED_DB_HOST' => $this->getConfiguredDbHost(),
			'DB_HOST_CHECK' => $this->getDbHostStatus(),
			'REQUIRED_DB_USER' => self::REQUIRED_DB_USER,
			'CONFIGURED_DB_USER' => $this->getConfiguredDbUser(),
			'DB_USER_CHECK' => $this->getDbUserStatus(),
			'REQUIRED_DB_PASSWORD' => self::REQUIRED_DB_PASSWORD,
			'CONFIGURED_DB_PASSWORD' => $this->getConfiguredDbPassword(),
			'DB_PASSWORD_CHECK' => $this->getDbPasswordStatus(),
			'DB_CONNECTION_CHECK' => $this->getDbConnectionStatus(),
			'DB_READACCESS_CHECK' => $this->getDbReadAccessStatus(),
			'REQUIRED_BCONSOLE_EXEC' => self::REQUIRED_BCONSOLE_EXEC,
			'CONFIGURED_BCONSOLE_EXEC' => $this->getConfiguredBConsoleExecPath(),
			'BCONSOLE_EXEC_CHECK' => $this->getBConsoleExecPathStatus(),
			'REQUIRED_BCONSOLE_CONF' => self::REQUIRED_BCONSOLE_CONF,
			'CONFIGURED_BCONSOLE_CONF' => $this->getConfiguredBConsoleConfPath(),
			'BCONSOLE_CONF_CHECK' => $this->getBConsoleConfPathStatus(),
			'REQUIRED_BCONSOLE_SUDO' => self::REQUIRED_BCONSOLE_SUDO,
			'CONFIGURED_BCONSOLE_SUDO' => $this->getConfiguredBConsoleSudo(),
			'BCONSOLE_SUDO_CHECK' => $this->getBConsoleSudoStatus(),
		));
		return $viewModel;
	}	

	private function getInstalledPHPVersion() 
	{
		$version = phpversion();
		return $version;
	}

	private function getInstalledZFVersion() 
	{
		return Version::VERSION; 
	}	
	
	private function compareVersions($installed, $required) 
	{
		if(version_compare($installed, $required, '<')) {
			return -1;
		}
		else {
			return 0;
		}
	}

	private function getInstalledPDOExt()
	{
		if(extension_loaded('PDO')) {
			$pdo = "YES";
                        return $pdo;
                }
                else {
			$pdo = "NO";
                        return $pdo;
                }
	}

	private function getPDOExtStatus() 
	{
		if(extension_loaded('PDO')) {
			return 0;
		}
		else {
			return -1;
		}
	}


	private function getIntlExtStatus() 
	{
		if(extension_loaded('Intl')) {
                        return 0;
                }
                else {
                        return -1;
                }
	}

	private function getInstalledIntlExt() 
	{
		if(extension_loaded('Intl')) {
                        $intl = "YES";
                        return $intl;
                }
                else {
                        $intl = "NO";
                        return $intl;
                }
	}

	private function getConfiguredDbDriver()
	{
		$config = $this->getServiceLocator()->get('Config');
		$backend = $config['db']['driver'];
		return $backend;
	}

	private function getDbDriverStatus() 
	{
		if(self::getConfiguredDbDriver() == "Pdo_Pgsql" || 
			self::getConfiguredDbDriver() == "Pdo_Mysql" ||
			self::getConfiguredDbDriver() == "Mysqli" || 
			self::getConfiguredDbDriver() == "Pgsql") {
			return 0;
		}
		else {
			return -1;
		}
	}

	private function getConfiguredDbHost() 
	{
		$config = $this->getServiceLocator()->get('Config');
                $host = $config['db']['host'];
                return $host;
	}

	private function getDbHostStatus() 
	{
		if(self::getConfiguredDbHost() != "") {
			return 0;
		}
		else {
			return -1;
		}
	}

	private function getConfiguredDbUser() 
	{
		$config = $this->getServiceLocator()->get('Config');
                $user = $config['db']['username'];
                return $user;
	}

	private function getDbUserStatus() 
	{
		if(self::getConfiguredDbUser() != "") {
                        return 0;
                }
                else {
                        return -1;
                }
	}

	private function getConfiguredDbPassword() 
	{
		$config = $this->getServiceLocator()->get('Config');
                $passwd = $config['db']['password'];
		if($passwd != "") {
			$passwd = "SET";
                	return $passwd;
		}
		else {
			$passwd = "NOT SET";
			return $passwd;
		}
	}

	private function getDbPasswordStatus() 
	{
		if(self::getConfiguredDbPassword() == "SET") {
                        return 0;
                }
                else {
                        return -1;
                }
	}

	private function getDbConnectionStatus() 
	{
		if(self::getDbDriverStatus() ==  0 &&
			self::getDbHostStatus() == 0 && 
			self::getDbUserStatus() == 0 &&
			self::getDbPasswordStatus() == 0
			) {
				try {
					$dbAdapter = $this->getServiceLocator()->get('Zend\Db\Adapter\Adapter');
					$connection = $dbAdapter->getDriver()->getConnection()->connect();
					return 0;
				}
				catch(\Exception $e) {
					return -1;
				}
		}
		else {
			return -1;
		}
	}

	private function getDbReadAccessStatus() 
	{
		if(self::getDbConnectionStatus() == 0) {
			try {
				$dbAdapter = $this->getServiceLocator()->get('Zend\Db\Adapter\Adapter');
                                $connection = $dbAdapter->getDriver()->getConnection()->connect();	

				if($connection->getCurrentSchema() == "bareos") {
					return 0;
				}
			}
			catch(\Exception $e) {
				return -1;
			}
		}
		else {
			return -1;
		}
	}

	private function getConfiguredBConsoleExecPath() 
	{
		$config = $this->getServiceLocator()->get('Config');
                $exec_path = $config['bconsole']['exec_path'];
		if($exec_path != "") {
			return $exec_path;
		}
		else {
			$err = "NOT SET";
			return $err; 
		}
	}

	private function getBConsoleExecPathStatus() 
	{
		if(self::getConfiguredBConsoleExecPath() == "NOT SET") {
			return -1;
		}
		else {
			return 0;
		}
	}

	private function getConfiguredBConsoleConfPath()
	{
		$config = $this->getServiceLocator()->get('Config');
                $config_path = $config['bconsole']['config_path'];
                if($config_path != "") {
                        return $config_path;
                }
                else {
                        $err = "NOT SET";
                        return $err; 
                }
	}
	
	private function getBConsoleConfPathStatus()
	{
		if(self::getConfiguredBConsoleConfPath() == "NOT SET") {
			return -1;
		}
		else {
			return 0;
		}
	}

	private function getConfiguredBConsoleSudo()
	{
		$config = $this->getServiceLocator()->get('Config');
                $sudo = $config['bconsole']['sudo'];
		if($sudo == true) {
			$msg = "TRUE";
		}
		elseif($sudo == false) {
			$msg = "FALSE";
		}
		else {
			$msg = "NOT SET";
		}
		return $msg;
	}

	private function getBConsoleSudoStatus() 
	{
		if(self::getConfiguredBConsoleSudo() == "TRUE" || self::getConfiguredBConsoleSudo() == "FALSE") {
			return 0;
		}
		else {
			return -1;
		}
	}

}

