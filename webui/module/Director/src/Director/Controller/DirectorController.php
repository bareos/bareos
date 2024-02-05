<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2024 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Director\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Exception;

class DirectorController extends AbstractActionController
{
    /**
     * Variables
     */
    protected $directorModel = null;
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
            $module_config['console_commands']['Director']['optional']
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

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getDirectorModel()->getDirectorStatus($this->bsock);
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(array(
            'directorOutput' => $result
        ));
    }

    public function subscriptionAction()
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
            $module_config['console_commands']['Director']['optional']
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

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getDirectorModel()->getBackupUnitReport($this->bsock, 0, "detail");
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(array(
            'directorOutput' => $result
        ));
    }

    public function backupunitreportAction()
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
            $module_config['console_commands']['Director']['optional']
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

        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getDirectorModel()->getBackupUnitReport($this->bsock, 2, "all");
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        $response = $this->getResponse();
        $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');
        $response->getHeaders()->addHeaderLine('Content-Disposition', 'attachment; filename="bareos-backup-unit-report.json"');

        if (isset($result)) {
            $json_backupunitreport = json_encode($result, JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT);
            $response->setContent($json_backupunitreport);
        }

        return $response;
    }

    /**
     * Message Action
     *
     * @return object
     */
    public function messagesAction()
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
            $module_config['console_commands']['Director']['optional']
        );
        if (count($invalid_commands) > 0 && in_array('messages', $invalid_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", 'messages')
                )
            );
        }

        return new ViewModel();
    }

    /**
     * Get Director Model
     *
     * @return object
     */
    public function getDirectorModel()
    {
        if (!$this->directorModel) {
            $sm = $this->getServiceLocator();
            $this->directorModel = $sm->get('Director\Model\DirectorModel');
        }
        return $this->directorModel;
    }
}
