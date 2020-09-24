<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

class UpdateAlert extends AbstractHelper
{
   protected $value;
   protected $result;

   public function __invoke($product_updates_status=null, $dird_update_available=null)
   {
      if($product_updates_status === false) {
         $this->result = '<a data-toggle="tooltip" data-placement="bottom" href="http://download.bareos.com/" target="_blank"title="Update informaton could not be retrieved"><span class="glyphicon glyphicon-exclamation-sign text-danger" aria-hidden="true"></span></a>';
            return $this->result;
      }

      if($dird_update_available === true) {
            $this->result = '<a data-toggle="tooltip" data-placement="bottom" href="http://download.bareos.com/" target="_blank"title="Updates available"><span class="glyphicon glyphicon-exclamation-sign text-danger" aria-hidden="true"></span></a>';
            return $this->result;
      }
   }
}
