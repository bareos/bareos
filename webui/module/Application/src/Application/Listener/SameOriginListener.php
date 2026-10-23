<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2026-2026 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Application\Listener;

use Application\Http\RequestOrigin;
use Zend\EventManager\AbstractListenerAggregate;
use Zend\EventManager\EventManagerInterface;
use Zend\Http\Request as HttpRequest;
use Zend\Http\Response as HttpResponse;
use Zend\Mvc\MvcEvent;
use Traversable;

/**
 * Rejects state-changing requests that cannot prove they come from this webui's
 * own origin. This is the CSRF defense.
 *
 * SameSite=Strict alone cannot express "this must be us, not merely our
 * domain": a sibling subdomain or another port of this host is same-site and
 * gets the cookie.
 *
 * Proof is Sec-Fetch-Site: same-origin, or Origin matching this request's
 * origin where Fetch Metadata is unavailable. Neither present is a rejection.
 * Safe methods are exempt, see isSafeMethod().
 *
 * Scheme and host come from Application\Http\RequestOrigin, which
 * Application\Module also uses for the cookie's Secure flag.
 */
class SameOriginListener extends AbstractListenerAggregate
{
    /**
     * Methods that change nothing, so need no proof.
     */
    const SAFE_METHODS = array('GET', 'HEAD', 'OPTIONS');

    /**
     * The only accepted Fetch Metadata value. 'none' is rejected: no browser
     * produces it for a POST.
     */
    const FETCH_METADATA_SAME_ORIGIN = 'same-origin';

    /**
     * What classify() decided; the values appear verbatim in the rejection log.
     *
     * OK is a value rather than null so that a branch falling through is a
     * rejection, not an accidental allow.
     */
    const OK = 'ok';
    const CROSS_SITE = 'cross-site';
    const SAME_SITE = 'same-site';
    const MISMATCH = 'origin-mismatch';
    const NO_PROOF = 'no-proof';

    /**
     * @var array
     */
    protected $config;

    /**
     * @param array $config
     *   Merged configuration; trusted_proxies is read from it.
     */
    public function __construct(array $config = array())
    {
        $this->config = $config;
    }

    /**
     * @param EventManagerInterface $events
     * @param int $priority
     * @return void
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        // After RouteListener (priority 1), so routing still happens.
        $this->listeners[] = $events->attach(MvcEvent::EVENT_ROUTE, array($this, 'onRoute'), -100);
    }

    /**
     * @param MvcEvent $event
     * @return HttpResponse|null
     */
    public function onRoute(MvcEvent $event)
    {
        $request = $event->getRequest();
        if (!$request instanceof HttpRequest) {
            return null;
        }

        if (self::isSafeMethod($request->getMethod())) {
            return null;
        }

        $status = self::classify($this->getServerParams($request), $this->config);
        if ($status === self::OK) {
            return null;
        }

        error_log(sprintf(
            'Rejected %s request to %s: same-origin check failed (%s).',
            $request->getMethod(),
            $request->getUri()->getPath(),
            $status
        ));

        $response = $event->getResponse();
        if (!$response instanceof HttpResponse) {
            $response = new HttpResponse();
        }
        $response->setStatusCode(403);
        $response->getHeaders()->addHeaderLine('Cache-Control', 'no-store');
        $response->setContent('Forbidden.');
        $event->setResponse($response);

        return $response;
    }

    /**
     * Whether the method is exempt because it changes nothing. Scope, not
     * policy: it decides which requests the check applies to at all.
     *
     * @param string $method
     * @return bool
     */
    public static function isSafeMethod($method): bool
    {
        return in_array(strtoupper((string)$method), self::SAFE_METHODS, true);
    }
    /**
     * Whether a state-changing request proved it came from this origin.
     *
     * @param array $server
     * @param array $config
     * @return string one of the status constants above
     */
    public static function classify(array $server, array $config = array()): string
    {
        if (isset($server['HTTP_SEC_FETCH_SITE'])) {
            $site = strtolower(trim($server['HTTP_SEC_FETCH_SITE']));
            if ($site === self::FETCH_METADATA_SAME_ORIGIN) {
                return self::OK;
            }
            return $site === 'cross-site' ? self::CROSS_SITE : self::SAME_SITE;
        }

        if (isset($server['HTTP_ORIGIN'])) {
            $origin = strtolower(trim($server['HTTP_ORIGIN']));
            $expected = (new RequestOrigin($config, $server))->origin();
            // An opaque origin is serialized as the literal string 'null' by
            // the browser and can never equal a real origin, so it lands here.
            if ($expected !== null && hash_equals($expected, $origin)) {
                return self::OK;
            }
            return self::MISMATCH;
        }

        return self::NO_PROOF;
    }

    /**
     * getServer() returns a Parameters container, not an array.
     *
     * @param HttpRequest $request
     * @return array
     */
    protected function getServerParams(HttpRequest $request)
    {
        $server = $request->getServer();

        if ($server instanceof Traversable) {
            return iterator_to_array($server);
        }

        return is_array($server) ? $server : array();
    }
}
