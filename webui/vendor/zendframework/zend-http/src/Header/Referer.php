<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Http\Header;

use Zend\Uri\Http as HttpUri;

/**
 * Content-Location Header
 *
 * @link       http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.36
 */
class Referer extends AbstractLocation
{
    /**
     * Set the URI/URL for this header
     * according to RFC Referer URI should not have fragment
     *
     * @param  string|HttpUri $uri
     * @return Referer
     */
    public function setUri($uri)
    {
        parent::setUri($uri);
        $this->uri->setFragment(null);

        return $this;
    }

    /**
     * Return header name
     *
     * @return string
     */
    public function getFieldName()
    {
        return 'Referer';
    }
}
