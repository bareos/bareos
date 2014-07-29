<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
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

namespace File\Model;

class File 
{

	public $fileid;
	public $jobid;
	public $pathid;
	public $filenameid;
	public $filename;

	public function exchangeArray($data)
	{
		$data = array_change_key_case($data, CASE_LOWER);		

		$this->fileid = (!empty($data['fileid'])) ? $data['fileid'] : null;
		$this->jobid = (!empty($data['jobid'])) ? $data['jobid'] : null;
		$this->pathid = (!empty($data['pathid'])) ? $data['pathid'] : null;
		$this->filenameid = (!empty($data['filenameid'])) ? $data['filenameid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
	}

}

