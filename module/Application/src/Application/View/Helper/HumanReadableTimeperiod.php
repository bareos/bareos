<?php

/**
 *
 * Barbossa - A Web-Frontend to manage Bareos
 * 
 * @link      http://github.com/fbergkemper/barbossa for the canonical source repository
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
class HumanReadableTimeperiod extends AbstractHelper
{

    protected $result;

    /**
     * A function for making timeperiods human readable
     * @method 
     * @return string 
     * @param
     * @param
     */
    public function __invoke($time, $format="short")
    {

	if(empty($time)) {
		$this->result = "-";
	}
	else {

		$this->result = "-";
		$dateTime = date_create($time); 
		$timestamp = date_format($dateTime, 'U');
		$seconds = time() - $timestamp;

		if($format == "short") {

			$units = array(
				'y' => $seconds / 31556926 % 12, 
				'w' => $seconds / 604800 % 52, 
				'd' => $seconds / 86400 % 7,
				'h' => $seconds / 3600 % 24, 
				'm' => $seconds / 60 % 60, 
				's' => $seconds % 60
			);  

			foreach($units as $key => $value) {
				if($value > 0) {
					$res[] = $value . $key;
				}   
			}   
			
			$this->result = join(' ', $res) . " ago";

		} 		
		elseif($format == "long") {
			
			$units = array(
				'Year(s)' => $seconds / 31556926 % 12,
				'Week(s)' => $seconds / 604800 % 52,
				'Day(s)' => $seconds / 86400 % 7,
				'Hour(s)' => $seconds / 3600 % 24,
				'Minute(s)' => $seconds / 60 % 60,
				'Second(s)' => $seconds % 60
			);

			foreach($units as $key => $value) {
				if($value > 0) {
					$res[] = $value . $key;
				}
			}		
			
			$this->result = join(' ', $res) . " ago";

		}
		elseif($format == "fuzzy") {

			// TODO

			$this->result = "~TODO ago";

		}
		
		return $this->result;

		}

    }

}

