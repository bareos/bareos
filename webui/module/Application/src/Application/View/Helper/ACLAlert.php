<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2019 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class ACLAlert extends AbstractHelper
{
  private $alert = null;

  public function __invoke($commands=null)
  {
    if($commands != null) {
      $msg_part_a = _('<b>Access denied</b><br><br>Permission to execute the following commands is required:<br><br><kbd>'.$commands.'</kbd>');
    } else {
      $msg_part_a = _('Access denied');
    }
    $msg_part_b = _('Read the <a href="%s" target="_blank">Bareos documentation</a> on how to configure ACL settings in your Console/Profile resources.');
    $msg_url = 'https://docs.bareos.org/IntroductionAndTutorial/InstallingBareosWebui.html#configuration-details';
    $msg_part_c = _('Back');

    $this->alert = '<div class="container-fluid">';
    $this->alert .= '<div class="row">';
    $this->alert .= '<div class="col-md-6">';
    $this->alert .= '<div class="alert alert-danger">'.$msg_part_a.'<br><br>'.sprintf($msg_part_b, $msg_url).'</div>';
    $this->alert .= '</div>';
    $this->alert .= '<div class="col-md-6"></div>';
    $this->alert .= '</div>';
    $this->alert .= '</div>';

    $this->alert .= '<a class="btn btn-primary" href="javascript: window.history.back();" role="button">'.$msg_part_c.'</a>';

    return $this->alert;
  }
}
