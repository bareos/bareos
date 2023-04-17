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

namespace Api\Controller;

use Zend\Mvc\Controller\AbstractRestfulController;
use Zend\View\Model\JsonModel;
use Exception;

class DirectorController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $directorModel = null;
    protected $result = null;

    public function getList()
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

        $this->bsock = $this->getServiceLocator()->get('director');
        $messages = $this->params()->fromQuery('messages');
        $limit = $this->params()->fromQuery('limit');
        $offset = $this->params()->fromQuery('offset');
        $reverse = $this->params()->fromQuery('reverse');

        try{
            if ($messages === "true" && isset($limit)) {
                $this->result = $this->getDirectorModel()->getDirectorMessages($this->bsock, $limit, $offset, $reverse);
            }
        } catch(Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e);
        }

        return new JsonModel($this->result);
    }

    public function getDirectorModel()
    {
        if (!$this->directorModel) {
            $sm = $this->getServiceLocator();
            $this->directorModel = $sm->get('Director\Model\DirectorModel');
        }
        return $this->directorModel;
    }
}
