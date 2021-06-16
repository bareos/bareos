<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Log;

use Interop\Container\ContainerInterface;

/**
 * PSR Logger abstract service factory.
 *
 * Allow to configure multiple loggers for application.
 */
class PsrLoggerAbstractAdapterFactory extends LoggerAbstractServiceFactory
{
    /**
     * Configuration key holding logger configuration
     *
     * @var string
     */
    protected $configKey = 'psr_log';

    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        $logger = parent::__invoke($container, $requestedName, $options);

        return new PsrLoggerAdapter($logger);
    }
}
