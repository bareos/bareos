<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage\Adapter;

/**
 * These are options specific to the XCache adapter
 */
class XCacheOptions extends AdapterOptions
{
    /**
     * Namespace separator
     *
     * @var string
     */
    protected $namespaceSeparator = ':';

    /**
     * Handle admin authentication
     *
     * @var bool
     */
    protected $adminAuth = false;

    /**
     * Username to call admin functions
     *
     * @var null|string
     */
    protected $adminUser;

    /**
     * Password to call admin functions
     *
     * @var null|string
     */
    protected $adminPass;

    /**
     * Set namespace separator
     *
     * @param  string $namespaceSeparator
     * @return XCacheOptions
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
     * Set username to call admin functions
     *
     * @param  null|string $adminUser
     * @return XCacheOptions
     */
    public function setAdminUser($adminUser)
    {
        $adminUser = ($adminUser === null) ? null : (string) $adminUser;
        if ($this->adminUser !== $adminUser) {
            $this->triggerOptionEvent('admin_user', $adminUser);
            $this->adminUser = $adminUser;
        }
        return $this;
    }

    /**
     * Get username to call admin functions
     *
     * @return string
     */
    public function getAdminUser()
    {
        return $this->adminUser;
    }

    /**
     * Enable/Disable admin authentication handling
     *
     * @param  bool $adminAuth
     * @return XCacheOptions
     */
    public function setAdminAuth($adminAuth)
    {
        $adminAuth = (bool) $adminAuth;
        if ($this->adminAuth !== $adminAuth) {
            $this->triggerOptionEvent('admin_auth', $adminAuth);
            $this->adminAuth = $adminAuth;
        }
        return $this;
    }

    /**
     * Get admin authentication enabled
     *
     * @return bool
     */
    public function getAdminAuth()
    {
        return $this->adminAuth;
    }

    /**
     * Set password to call admin functions
     *
     * @param  null|string $adminPass
     * @return XCacheOptions
     */
    public function setAdminPass($adminPass)
    {
        $adminPass = ($adminPass === null) ? null : (string) $adminPass;
        if ($this->adminPass !== $adminPass) {
            $this->triggerOptionEvent('admin_pass', $adminPass);
            $this->adminPass = $adminPass;
        }
        return $this;
    }

    /**
     * Get password to call admin functions
     *
     * @return string
     */
    public function getAdminPass()
    {
        return $this->adminPass;
    }
}
