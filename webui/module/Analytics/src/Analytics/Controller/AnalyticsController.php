<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Analytics\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Exception;

class AnalyticsController extends AbstractActionController
{
    protected $analyticsModel = null;

    public function indexAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Analytics']['mandatory']
        );

        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        return new ViewModel();
    }

    public function getDataAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $result = null;

        $data = $this->params()->fromQuery('data');

        if ($data == "jobtotals") {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
                $result = $this->getAnalyticsModel()->getJobTotals($this->bsock);
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        } elseif ($data == "overall-jobtotals") {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
                $result = $this->getAnalyticsModel()->getOverallJobTotals($this->bsock);
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        }

        $response = $this->getResponse();
        $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

        if (isset($result)) {
            $response->setContent(JSON::encode($result));
        }

        return $response;
    }

    public function getAnalyticsModel()
    {
        if (!$this->analyticsModel) {
            $sm = $this->getServiceLocator();
            $this->analyticsModel = $sm->get('Analytics\Model\AnalyticsModel');
        }
        return $this->analyticsModel;
    }
}
