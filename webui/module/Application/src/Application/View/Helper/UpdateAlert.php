<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2024 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
    public function __invoke($version_info = null)
    {
        $result = null;

        if (!$version_info) {
            $result = '<a data-toggle="tooltip" data-placement="bottom" href="https://download.bareos.com/" target="_blank" title="Update information could not be retrieved"><span class="glyphicon glyphicon-exclamation-sign text-danger" aria-hidden="true"></span></a>';
        } elseif ($version_info['status'] != "uptodate") {
            $message = "Bareos Director (" . $version_info["requested_version"] . "): " . $version_info['package_update_info'];
            $result = '<a data-toggle="tooltip" data-placement="bottom" href="https://download.bareos.com/" target="_blank" title="' . $message . '"><span class="glyphicon glyphicon-exclamation-sign text-danger" aria-hidden="true"></span></a>';
        }
        return $result;
    }
}
