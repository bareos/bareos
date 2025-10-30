<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2025 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Media\Controller;

use Laminas\Mvc\Controller\AbstractActionController;
use Laminas\View\Model\ViewModel;
use Exception;

class MediaController extends AbstractActionController
{
    /**
     * Variables
     */
    protected $mediaModel = null;
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
            $module_config['console_commands']['Media']['mandatory']
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
            $volumes = $this->getMediaModel()->getVolumes($this->bsock);
            $this->bsock->disconnect();
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        return new ViewModel(
            array(
                'volumes' => $volumes,
            )
        );
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
            $module_config['console_commands']['Media']['mandatory']
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

        $volumename = $this->params()->fromQuery('volume');

        return new ViewModel(array(
            'volume' => $volumename,
        ));
    }

    /**
     * Get Media Model
     *
     * @return object
     */
    public function getMediaModel()
    {
        if (!$this->mediaModel) {
            $sm = $this->getServiceLocator();
            $this->mediaModel = $sm->get('Media\Model\MediaModel');
        }
        return $this->mediaModel;
    }
}
