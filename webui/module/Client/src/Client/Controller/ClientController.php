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

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Exception;

class ClientController extends AbstractActionController
{
    protected $clientModel = null;
    protected $bsock = null;
    protected $acl_alert = false;

    /**
     * Index Action
     *
     * @return object
     */
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
            $module_config['console_commands']['Client']['mandatory']
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

        $result = null;

        $action = $this->params()->fromQuery('action');

        if (empty($action)) {
            return new ViewModel();
        } else {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            if ($action == "enable") {
                $clientname = $this->params()->fromQuery('client');
                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Client']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('enable', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'enable'
                            )
                        );
                    } else {
                        $result = $this->getClientModel()->enableClient($this->bsock, $clientname);
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            } elseif ($action == "disable") {
                $clientname = $this->params()->fromQuery('client');
                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Client']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('disable', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'disable'
                            )
                        );
                    } else {
                        $result = $this->getClientModel()->disableClient($this->bsock, $clientname);
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }
            }

            try {
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }

            return new ViewModel(
                array(
                    'result' => $result
                )
            );
        }
    }

    /**
     * Details Action
     *
     * @return object
     */
    public function detailsAction()
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
            $module_config['console_commands']['Client']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => $invalid_commands
                )
            );
        }

        return new ViewModel(
            array(
                'client' => $this->params()->fromQuery('client')
            )
        );
    }

    /**
     * Status Action
     *
     * @return object
     */
    public function statusAction()
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
            $module_config['console_commands']['Client']['optional']
        );
        if (count($invalid_commands) > 0 && in_array('status', $invalid_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => 'status'
                )
            );
        }

        $result = null;

        $clientname = $this->params()->fromQuery('client');

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getClientModel()->statusClient($this->bsock, $clientname);
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(
            array(
                'result' => $result
            )
        );
    }

    public function timelineAction()
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
            $module_config['console_commands']['Job']['mandatory']
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

    public function getClientModel()
    {
        if (!$this->clientModel) {
            $sm = $this->getServiceLocator();
            $this->clientModel = $sm->get('Client\Model\ClientModel');
        }
        return $this->clientModel;
    }

}
