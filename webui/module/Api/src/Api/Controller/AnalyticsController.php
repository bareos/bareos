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
use Zend\Json\Json;
use Exception;

class AnalyticsController extends AbstractRestfulController
{
    protected $bsock = null;
    protected $analyticsModel = null;
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
        $type = $this->params()->fromQuery('type');
        try {
            if ($type === "jobtotals") {
                $this->result = $this->getAnalyticsModel()->getJobTotals($this->bsock);
            } elseif ($type === "overall-jobtotals") {
                $this->result = $this->getAnalyticsModel()->getOverallJobTotals($this->bsock);
            } elseif ($type === "config-resource-graph") {
                $this->result = $this->getAnalyticsModel()->getConfigResourceGraph($this->bsock);
                $response = $this->getResponse();
                $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');
                $response->setContent(JSON::encode($this->result));
                return $response;
            }
        } catch (Exception $e) {
            $this->getResponse()->setStatusCode(500);
            error_log($e);
        }

        return new JsonModel($this->result);
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
