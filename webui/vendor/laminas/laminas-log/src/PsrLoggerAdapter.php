<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log;

use Psr\Log\AbstractLogger as PsrAbstractLogger;
use Psr\Log\InvalidArgumentException;
use Psr\Log\LogLevel;

/**
 * PSR-3 logger adapter for Laminas\Log\LoggerInterface
 *
 * Decorates a LoggerInterface to allow it to be used anywhere a PSR-3 logger
 * is expected.
 */
class PsrLoggerAdapter extends PsrAbstractLogger
{
    /**
     * Laminas\Log logger
     *
     * @var LoggerInterface
     */
    protected $logger;

    /**
     * Map PSR-3 LogLevels to priority
     *
     * @var array
     */
    protected $psrPriorityMap = [
        LogLevel::EMERGENCY => Logger::EMERG,
        LogLevel::ALERT     => Logger::ALERT,
        LogLevel::CRITICAL  => Logger::CRIT,
        LogLevel::ERROR     => Logger::ERR,
        LogLevel::WARNING   => Logger::WARN,
        LogLevel::NOTICE    => Logger::NOTICE,
        LogLevel::INFO      => Logger::INFO,
        LogLevel::DEBUG     => Logger::DEBUG,
    ];

    /**
     * Constructor
     *
     * @param LoggerInterface $logger
     */
    public function __construct(LoggerInterface $logger)
    {
        $this->logger = $logger;
    }

    /**
     * Returns composed LoggerInterface instance.
     *
     * @return LoggerInterface
     */
    public function getLogger()
    {
        return $this->logger;
    }

    /**
     * Logs with an arbitrary level.
     *
     * @param mixed  $level
     * @param string $message
     * @param array  $context
     * @return null
     * @throws InvalidArgumentException if log level is not recognized
     */
    public function log($level, $message, array $context = [])
    {
        if (! array_key_exists($level, $this->psrPriorityMap)) {
            throw new InvalidArgumentException(sprintf(
                '$level must be one of PSR-3 log levels; received %s',
                var_export($level, 1)
            ));
        }

        $priority = $this->psrPriorityMap[$level];
        $this->logger->log($priority, $message, $context);
    }
}
