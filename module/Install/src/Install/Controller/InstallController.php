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

namespace Install\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Version\Version;
use Zend\Db\Adapter\Driver\ConnectionInterface;
use Zend\Db\Sql\Sql;
use Zend\Db\ResultSet\ResultSet;
use Bareos\Db\Sql\BareosSqlCompatHelper;
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
	const REQUIRED_DIRECTOR = "YES";

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
			'REQUIRED_DIRECTOR' => self::REQUIRED_DIRECTOR,
			'CONFIGURED_DIRECTOR' => $this->getConfiguredDirector(),
			'DIRECTOR_CHECK' => $this->getDirectorStatus(),
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
		$adapter = array_keys($config['db']['adapters']);
		$backend = $config['db']['adapters'][$adapter[0]]['driver'];
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
		$adapter = array_keys($config['db']['adapters']);
        $host = $config['db']['adapters'][$adapter[0]]['host'];
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
		$adapter = array_keys($config['db']['adapters']);
        $user = $config['db']['adapters'][$adapter[0]]['username'];
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
		$adapter = array_keys($config['db']['adapters']);
		$passwd = $config['db']['adapters'][$adapter[0]]['password'];
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
					$config = $this->getServiceLocator()->get('Config');
					$adapter = array_keys($config['db']['adapters']);
					//$db = $config['db']['adapters'][$adapter[0]]['host'];
					$dbAdapter = $this->getServiceLocator()->get($adapter[0]);
					$connection = $dbAdapter->getDriver()->getConnection()->connect();
					return 0;
				}
				catch(\Exception $e) {
					//echo $e->getMessage() . $e->getTraceAsString();
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
				// no beauty, but will work for the moment
				$config = $this->getServiceLocator()->get('Config');
				$adapter = array_keys($config['db']['adapters']);
				$driver = $config['db']['adapters'][$adapter[0]]['driver'];
				$bsqlch = new BareosSqlCompatHelper($driver);
				$dbAdapter = $this->getServiceLocator()->get($adapter[0]);
				$sql = new Sql($dbAdapter);
				$select = $sql->select();
				$select->columns(array($bsqlch->strdbcompat("VersionId")));
				$select->from($bsqlch->strdbcompat("Version"));
				$statement = $sql->prepareStatementForSqlObject($select);
				$results = $statement->execute();
				$resultSet = new ResultSet;
				$resultSet->initialize($results);
				$result = $resultSet->toArray();
				$versionid = $result[0][$bsqlch->strdbcompat("VersionId")];
				if(is_int($versionid) || $versionid >= 2001) {
					return 0;
				}
				else {
					return -1;
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

	private function getConfiguredDirector()
	{
		$config = $this->getServiceLocator()->get('Config');
		$dir = array_keys($config['directors']);
		$director = $config['directors'][$dir[0]]['host'];
		return $director;
	}

	private function getDirectorStatus()
	{
		if(self::getConfiguredDirector() != "") {
                        return 0;
                }
                else {
                        return -1;
                }

	}

}

