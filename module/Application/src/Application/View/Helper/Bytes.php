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
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

/**
 * 
 */
class Bytes extends AbstractHelper
{

	protected $bsize;

	/**
     	 * @method 
     	 * @return string 
     	 */
    	public function __invoke($bytes)
    	{

		$units = array('B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB');
		$this->bsize = "0.00 B";

		if($bytes > 0)
		{
			$result = log($bytes) / log(1000);
			$this->bsize = round(pow(1000, $result - ($tmp = floor($result))), 2)." ".$units[$tmp];
		}

		return $this->bsize;

    	}

}

