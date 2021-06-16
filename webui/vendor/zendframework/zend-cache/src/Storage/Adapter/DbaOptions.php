<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Cache\Storage\Adapter;

use Zend\Cache\Exception;

/**
 * These are options specific to the APC adapter
 */
class DbaOptions extends AdapterOptions
{
    /**
     * Namespace separator
     *
     * @var string
     */
    protected $namespaceSeparator = ':';

    /**
     * Pathname to the database file
     *
     * @var string
     */
    protected $pathname = '';

    /**
     * The mode to open the database
     *
     * @var string
     */
    protected $mode = 'c';

    /**
     * The name of the handler which shall be used for accessing the database.
     *
     * @var string
     */
    protected $handler = 'flatfile';

    /**
     * Set namespace separator
     *
     * @param  string $namespaceSeparator
     * @return DbaOptions Provides a fluent interface
     */
    public function setNamespaceSeparator($namespaceSeparator)
    {
        $namespaceSeparator = (string) $namespaceSeparator;
        $this->triggerOptionEvent('namespace_separator', $namespaceSeparator);
        $this->namespaceSeparator = $namespaceSeparator;
        return $this;
    }

    /**
     * Get namespace separator
     *
     * @return string
     */
    public function getNamespaceSeparator()
    {
        return $this->namespaceSeparator;
    }

    /**
     * Set pathname to database file
     *
     * @param string $pathname
     * @return DbaOptions Provides a fluent interface
     */
    public function setPathname($pathname)
    {
        $this->pathname = (string) $pathname;
        $this->triggerOptionEvent('pathname', $pathname);
        return $this;
    }

    /**
     * Get pathname to database file
     *
     * @return string
     */
    public function getPathname()
    {
        return $this->pathname;
    }

    /**
     *
     *
     * @param string $mode
     * @return DbaOptions Provides a fluent interface
     */
    public function setMode($mode)
    {
        $this->mode = (string) $mode;
        $this->triggerOptionEvent('mode', $mode);
        return $this;
    }

    public function getMode()
    {
        return $this->mode;
    }

    /**
     *
     *
     * @param string $handler
     * @return DbaOptions Provides a fluent interface
     */
    public function setHandler($handler)
    {
        $handler = (string) $handler;

        if (! function_exists('dba_handlers') || ! in_array($handler, dba_handlers())) {
            throw new Exception\ExtensionNotLoadedException("DBA-Handler '{$handler}' not supported");
        }

        if ($handler === 'inifile') {
            throw new Exception\ExtensionNotLoadedException(
                "DBA-Handler 'inifile' does not reliably support write operations"
            );
        }

        $this->triggerOptionEvent('handler', $handler);
        $this->handler = $handler;
        return $this;
    }

    public function getHandler()
    {
        return $this->handler;
    }
}
