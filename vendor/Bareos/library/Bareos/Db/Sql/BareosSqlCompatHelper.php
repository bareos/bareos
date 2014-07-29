<?php

namespace Bareos\Db\Sql;

class BareosSqlCompatHelper 
{

	protected $dbDriverConfig;

	public function __construct($dbDriverConfig) 
	{
		$this->dbDriverConfig = $dbDriverConfig;
	}

	public function strdbcompat($str) 
	{
		// PostgreSQL
                if($this->dbDriverConfig == "Pdo_Pgsql" || $this->dbDriverConfig == "Pgsql") {
                        $str = strtolower($str);
                }
                // MySQL
                if($this->dbDriverConfig == "Pdo_Mysql" || $this->dbDriverConfig == "Mysqli") {
                        return $str;
                }
                return $str;
	}

}
