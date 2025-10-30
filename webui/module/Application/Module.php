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

namespace Application;

use Laminas\Mvc\ModuleRouteListener;
use Laminas\Mvc\MvcEvent;
use Laminas\Session\SessionManager;
use Laminas\Session\Container;
use Bareos\BSock\BareosBsock;

class Module
{
    public function onBootstrap(MvcEvent $e)
    {
        $eventManager = $e->getApplication()->getEventManager();
        $moduleRouteListener = new ModuleRouteListener();
        $moduleRouteListener->attach($eventManager);
        $this->initSession($e);

        $translator = $e->getApplication()->getServiceManager()->get('MVCTranslator');
        $translator->setLocale($_SESSION['bareos']['locale'] ?? 'en_EN')->setFallbackLocale('en_EN');

        $viewModel = $e->getApplication()->getMVCEvent()->getViewModel();
    }

    public function getConfig()
    {
        $config = array();
        $configFiles = array(
            include __DIR__ . '/config/module.config.php',
            include __DIR__ . '/config/module.commands.php'
        );
        foreach ($configFiles as $file) {
            $config = \Laminas\Stdlib\ArrayUtils::merge($config, $file);
        }
        return $config;
    }

    public function getAutoloaderConfig()
    {
        return array(
            'Laminas\Loader\ClassMapAutoloader' => array(
                'application' => __DIR__ . '/autoload_classmap.php',
            ),
            'Laminas\Loader\StandardAutoloader' => array(
                'namespaces' => array(
                    __NAMESPACE__ => __DIR__ . '/src/' . __NAMESPACE__,
                    'Bareos' => __DIR__ . '/../../vendor/Bareos/library/Bareos',
                ),
            ),
        );
    }

    public function initSession($e)
    {
        $session = $e->getApplication()->getServiceManager()->get('Laminas\Session\SessionManager');

        if ($session->isValid()) {
            // do nothing
        } else {
            $session->expireSessionCookie();
            $session->destroy();
            $session->start();
        }

        $container = new Container('bareos');

        if (!isset($container->init)) {
            $serviceManager = $e->getApplication()->getServiceManager();
            $request = $serviceManager->get('Request');

            $session->regenerateId(true);
            $container->init      = 1;
            $container->remoteAddr      = $request->getServer()->get('REMOTE_ADDR');
            $container->httpUserAgent   = $request->getServer()->get('HTTP_USER_AGENT');
            $container->username       = "";
            $container->authenticated   = false;

            $config = $serviceManager->get('Config');

            if (!isset($config['session'])) {
                return;
            }

            $sessionConfig = $config['session'];

            if (isset($sessionConfig['validators'])) {
                $chain = $session->getValidatorChain();
                foreach ($sessionConfig['validators'] as $validator) {
                    switch ($validator) {
                        case 'Laminas\Session\Validator\HttpUserAgent':
                            $validator = new $validator($container->httpUserAgent);
                            break;
                        case 'Laminas\Session\Validator\RemoteAddr':
                            $validator = new $validator($container->remoteAddr);
                            break;
                        default:
                            $validator = new $validator();
                    }
                    $chain->attach('session.validate', array($validator, 'isValid'));
                }
            }
        }
    }

    public function getServiceConfig()
    {
        return array(
            'factories' => array(
                'Laminas\Session\SessionManager' => function ($sm) {
                    $config = $sm->get('config');

                    if (isset($config['session'])) {
                        $session = $config['session'];

                        $sessionConfig = null;

                        if (isset($session['config'])) {
                            $class = isset($session['config']['class']) ? $session['config']['class'] : 'Laminas\Session\Config\SessionConfig';
                            $options = isset($session['config']['options']) ? $session['config']['options'] : array();
                            $sessionConfig = new $class();
                            $sessionConfig->setOptions($options);
                        }

                        $sessionStorage = null;

                        if (isset($session['storage'])) {
                            $class = $session['storage'];
                            $sessionStorage = new $class();
                        }

                        $sessionSaveHandler = null;

                        if (isset($session['save_handler'])) {
                            // class should be fetched from service manager since it will require constructor arguments
                            $sessionSaveHandler = $sm->get($session['save_handler']);
                        }

                        $sessionManager = new SessionManager($sessionConfig, $sessionStorage, $sessionSaveHandler);
                    } else {
                        $sessionManager = new SessionManager();
                    }

                    Container::setDefaultManager($sessionManager);

                    return $sessionManager;
                },

            ),

        );
    }
}
