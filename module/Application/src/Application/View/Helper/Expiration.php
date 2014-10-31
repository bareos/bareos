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
class Expiration extends AbstractHelper
{

    protected $result;

    /**
     * @method
     * @return string
     * @param
     * @param
     */
    public function __invoke($retention, $lastwritten, $volstatus)
    {

	if($volstatus == "Used" || $volstatus == "Full") {

		if(empty($lastwritten)) {
			return $this->result = "-";
		}
		else {

			$this->result = "-";
			$lw = explode(" ", $lastwritten);
			$t1 = explode("-", $lw[0]);
			$t2 = explode("-", date("Y-m-d", time("NOW")));
			$d1 = mktime(0, 0, 0, (int)$t1[1],(int)$t1[2],(int)$t1[0]);
			$d2 = mktime(0, 0, 0, (int)$t2[1],(int)$t2[2],(int)$t2[0]);
			$interval = ($d2 - $d1) / (3600 * 24);
			$retention = round(($retention / 60 / 60 / 24 ), 2, PHP_ROUND_HALF_EVEN);
			$this->result = round(($retention - $interval), 2, PHP_ROUND_HALF_EVEN);

			if($this->result <= 0) {
				return $this->result = "<span class='label label-danger'>expired</span>";
			}
			elseif($this->result > 0) {
				return "<span class='label label-warning'>expires in " . $this->result . " days</span>";
			}

		}
	}
	else {
		return $this->result = round(($retention / 60 / 60 / 24 ), 2, PHP_ROUND_HALF_EVEN) . " days";
	}

    }

}

