<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
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

namespace Fileset\Model;

class Fileset
{

   public $filesetid;
   public $fileset;
   public $md5;
   public $createtime;

   public function exchangeArray($data)
   {
      $data = array_change_key_case($data, CASE_LOWER);

      $this->filesetid = (!empty($data['filesetid'])) ? $data['filesetid'] : null;
      $this->fileset = (!empty($data['fileset'])) ? $data['fileset'] : null;
      $this->md5 = (!empty($data['md5'])) ? $data['md5'] : null;
      $this->createtime = (!empty($data['createtime'])) ? $data['createtime'] : null;
   }

}

