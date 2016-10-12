<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
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

class ACLAlert extends AbstractHelper
{
   private $required_commands = null;
   private $alert = null;

   public function __invoke($required_commands)
   {
      $msg_part_a = _('Sorry, it seems you are not authorized to run this module. If you think this is an error, please contact your local administrator.');
      $msg_part_b = _('Please read the <a href="http://doc.bareos.org/master/html/bareos-manual-main-reference.html#sec:webui-console" target="_blank">Bareos documentation</a> for any additional information on how to configure the Command ACL directive of your Console/Profile resources. Following is a list of required commands which need to be in your Command ACL to run this module properly:');

      $this->required_commands = $required_commands;

      $this->alert = '<div class="container-fluid">';
      $this->alert .= '<div class="row">';
      $this->alert .= '<div class="col-md-6">';
      $this->alert .= '<div class="alert alert-danger"><b>'.$msg_part_a.'</b></div>';
      $this->alert .= $msg_part_b;

      $this->alert .= '</br></br>';
      $this->alert .= '<ul>';

      foreach($this->required_commands as $cmd) {
         $this->alert .= '<li>'.$cmd.'</li>';
      }

      $this->alert .= '</ul>';
      $this->alert .= '</div>';
      $this->alert .= '<div class="col-md-6"></div>';
      $this->alert .= '</div>';
      $this->alert .= '</div>';

      return $this->alert;
   }
}
