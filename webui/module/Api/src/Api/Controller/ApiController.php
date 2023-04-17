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

class ApiController extends AbstractRestfulController
{
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

        return new JsonModel();
    }
/*
    // Action used for POST requests
    public function create($data)
    {
        return $this->response->setStatusCode(405);
    }

    // Action used for DELETE requests
    public function delete($id)
    {
        retur $this->response->setStatusCode(405);
    }

    public function deleteList()
    {
        return $this->response->setStatusCode(405);
    }

    // Action used for GET requests with resource Id
    public function get($id)
    {
        return $this->response->setStatusCode(405);
    }

    // Action used for GET requests without resource Id
    public function getList()
    {
        return $this->response->setStatusCode(405);
    }

    public function head($id = null)
    {
        return $this->response->setStatusCode(405);
    }

    public function options()
    {
        return $this->response->setStatusCode(405);
    }

    public function patch($id, $data)
    {
        return $this->response->setStatusCode(405);
    }

    public function replaceList($data)
    {
        return $this->response->setStatusCode(405);
    }

    public function patchList($data)
    {
        return $this->response->setStatusCode(405);
    }

    // Action used for PUT requests
    public function update($id, $data)
    {
        return $this->response->setStatusCode(405);
    }
*/
}
