<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Zend\ModuleManager\Listener;

/**
 * Abstract listener
 */
abstract class AbstractListener
{
    /**
     * @var ListenerOptions
     */
    protected $options;

    /**
     * __construct
     *
     * @param  ListenerOptions $options
     */
    public function __construct(ListenerOptions $options = null)
    {
        $options = $options ?: new ListenerOptions;
        $this->setOptions($options);
    }

    /**
     * Get options.
     *
     * @return ListenerOptions
     */
    public function getOptions()
    {
        return $this->options;
    }

    /**
     * Set options.
     *
     * @param ListenerOptions $options the value to be set
     * @return AbstractListener
     */
    public function setOptions(ListenerOptions $options)
    {
        $this->options = $options;
        return $this;
    }

    /**
     * Write a simple array of scalars to a file
     *
     * @param  string $filePath
     * @param  array $array
     * @return AbstractListener
     */
    protected function writeArrayToFile($filePath, $array)
    {
        // Write cache file to temporary file first and then rename it.
        // We don't want cache file to be read when it is not written completely.
        // include/require functions require additional lock, see:
        // https://bugs.php.net/bug.php?id=52895
        $tmp = tempnam(sys_get_temp_dir(), md5($filePath));

        $content = "<?php\nreturn " . var_export($array, true) . ';';
        file_put_contents($tmp, $content);
        chmod($tmp, 0666 & ~umask());

        rename($tmp, $filePath);

        return $this;
    }
}
