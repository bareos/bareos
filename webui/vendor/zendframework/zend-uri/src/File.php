<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Uri;

/**
 * File URI handler
 *
 * The 'file:...' scheme is loosely defined in RFC-1738
 */
class File extends Uri
{
    protected static $validSchemes = array('file');

    /**
     * Check if the URI is a valid File URI
     *
     * This applies additional specific validation rules beyond the ones
     * required by the generic URI syntax.
     *
     * @return bool
     * @see    Uri::isValid()
     */
    public function isValid()
    {
        if ($this->query) {
            return false;
        }

        return parent::isValid();
    }

    /**
     * User Info part is not used in file URIs
     *
     * @see    Uri::setUserInfo()
     * @param  string $userInfo
     * @return File
     */
    public function setUserInfo($userInfo)
    {
        return $this;
    }

    /**
     * Fragment part is not used in file URIs
     *
     * @see    Uri::setFragment()
     * @param  string $fragment
     * @return File
     */
    public function setFragment($fragment)
    {
        return $this;
    }

    /**
     * Convert a UNIX file path to a valid file:// URL
     *
     * @param  string $path
     * @return File
     */
    public static function fromUnixPath($path)
    {
        $url = new static('file:');
        if (substr($path, 0, 1) == '/') {
            $url->setHost('');
        }

        $url->setPath($path);
        return $url;
    }

    /**
     * Convert a Windows file path to a valid file:// URL
     *
     * @param  string $path
     * @return File
     */
    public static function fromWindowsPath($path)
    {
        $url = new static('file:');

        // Convert directory separators
        $path = str_replace(array('/', '\\'), array('%2F', '/'), $path);

        // Is this an absolute path?
        if (preg_match('|^([a-zA-Z]:)?/|', $path)) {
            $url->setHost('');
        }

        $url->setPath($path);
        return $url;
    }
}
