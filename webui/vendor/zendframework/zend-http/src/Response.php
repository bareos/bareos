<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Http;

use Zend\Http\Exception\RuntimeException;
use Zend\Stdlib\ErrorHandler;
use Zend\Stdlib\ResponseInterface;

/**
 * HTTP Response
 *
 * @link      http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6
 */
class Response extends AbstractMessage implements ResponseInterface
{
    /**#@+
     * @const int Status codes
     */
    const STATUS_CODE_CUSTOM = 0;
    const STATUS_CODE_100 = 100;
    const STATUS_CODE_101 = 101;
    const STATUS_CODE_102 = 102;
    const STATUS_CODE_200 = 200;
    const STATUS_CODE_201 = 201;
    const STATUS_CODE_202 = 202;
    const STATUS_CODE_203 = 203;
    const STATUS_CODE_204 = 204;
    const STATUS_CODE_205 = 205;
    const STATUS_CODE_206 = 206;
    const STATUS_CODE_207 = 207;
    const STATUS_CODE_208 = 208;
    const STATUS_CODE_226 = 226;
    const STATUS_CODE_300 = 300;
    const STATUS_CODE_301 = 301;
    const STATUS_CODE_302 = 302;
    const STATUS_CODE_303 = 303;
    const STATUS_CODE_304 = 304;
    const STATUS_CODE_305 = 305;
    const STATUS_CODE_306 = 306;
    const STATUS_CODE_307 = 307;
    const STATUS_CODE_308 = 308;
    const STATUS_CODE_400 = 400;
    const STATUS_CODE_401 = 401;
    const STATUS_CODE_402 = 402;
    const STATUS_CODE_403 = 403;
    const STATUS_CODE_404 = 404;
    const STATUS_CODE_405 = 405;
    const STATUS_CODE_406 = 406;
    const STATUS_CODE_407 = 407;
    const STATUS_CODE_408 = 408;
    const STATUS_CODE_409 = 409;
    const STATUS_CODE_410 = 410;
    const STATUS_CODE_411 = 411;
    const STATUS_CODE_412 = 412;
    const STATUS_CODE_413 = 413;
    const STATUS_CODE_414 = 414;
    const STATUS_CODE_415 = 415;
    const STATUS_CODE_416 = 416;
    const STATUS_CODE_417 = 417;
    const STATUS_CODE_418 = 418;
    const STATUS_CODE_422 = 422;
    const STATUS_CODE_423 = 423;
    const STATUS_CODE_424 = 424;
    const STATUS_CODE_425 = 425;
    const STATUS_CODE_426 = 426;
    const STATUS_CODE_428 = 428;
    const STATUS_CODE_429 = 429;
    const STATUS_CODE_431 = 431;
    const STATUS_CODE_451 = 451;
    const STATUS_CODE_444 = 444;
    const STATUS_CODE_499 = 499;
    const STATUS_CODE_500 = 500;
    const STATUS_CODE_501 = 501;
    const STATUS_CODE_502 = 502;
    const STATUS_CODE_503 = 503;
    const STATUS_CODE_504 = 504;
    const STATUS_CODE_505 = 505;
    const STATUS_CODE_506 = 506;
    const STATUS_CODE_507 = 507;
    const STATUS_CODE_508 = 508;
    const STATUS_CODE_510 = 510;
    const STATUS_CODE_511 = 511;
    const STATUS_CODE_599 = 599;
    /**#@-*/

    /**
     * @var array Recommended Reason Phrases
     */
    protected $recommendedReasonPhrases = [
        // INFORMATIONAL CODES
        100 => 'Continue',
        101 => 'Switching Protocols',
        102 => 'Processing',
        // SUCCESS CODES
        200 => 'OK',
        201 => 'Created',
        202 => 'Accepted',
        203 => 'Non-Authoritative Information',
        204 => 'No Content',
        205 => 'Reset Content',
        206 => 'Partial Content',
        207 => 'Multi-status',
        208 => 'Already Reported',
        226 => 'IM Used',
        // REDIRECTION CODES
        300 => 'Multiple Choices',
        301 => 'Moved Permanently',
        302 => 'Found',
        303 => 'See Other',
        304 => 'Not Modified',
        305 => 'Use Proxy',
        306 => 'Switch Proxy', // Deprecated
        307 => 'Temporary Redirect',
        308 => 'Permanent Redirect',
        // CLIENT ERROR
        400 => 'Bad Request',
        401 => 'Unauthorized',
        402 => 'Payment Required',
        403 => 'Forbidden',
        404 => 'Not Found',
        405 => 'Method Not Allowed',
        406 => 'Not Acceptable',
        407 => 'Proxy Authentication Required',
        408 => 'Request Time-out',
        409 => 'Conflict',
        410 => 'Gone',
        411 => 'Length Required',
        412 => 'Precondition Failed',
        413 => 'Request Entity Too Large',
        414 => 'Request-URI Too Long',
        415 => 'Unsupported Media Type',
        416 => 'Requested range not satisfiable',
        417 => 'Expectation Failed',
        418 => 'I\'m a teapot',
        422 => 'Unprocessable Entity',
        423 => 'Locked',
        424 => 'Failed Dependency',
        425 => 'Too Early',
        426 => 'Upgrade Required',
        428 => 'Precondition Required',
        429 => 'Too Many Requests',
        431 => 'Request Header Fields Too Large',
        444 => 'Connection Closed Without Response',
        451 => 'Unavailable For Legal Reasons',
        499 => 'Client Closed Request',
        // SERVER ERROR
        500 => 'Internal Server Error',
        501 => 'Not Implemented',
        502 => 'Bad Gateway',
        503 => 'Service Unavailable',
        504 => 'Gateway Time-out',
        505 => 'HTTP Version not supported',
        506 => 'Variant Also Negotiates',
        507 => 'Insufficient Storage',
        508 => 'Loop Detected',
        510 => 'Not Extended',
        511 => 'Network Authentication Required',
        599 => 'Network Connect Timeout Error',
    ];

    /**
     * @var int Status code
     */
    protected $statusCode = 200;

    /**
     * @var string|null Null means it will be looked up from the $reasonPhrase list above
     */
    protected $reasonPhrase;

    /**
     * Populate object from string
     *
     * @param  string $string
     * @return self
     * @throws Exception\InvalidArgumentException
     */
    public static function fromString($string)
    {
        $lines = explode("\r\n", $string);
        if (! is_array($lines) || count($lines) === 1) {
            $lines = explode("\n", $string);
        }

        $firstLine = array_shift($lines);

        $response = new static();
        $response->parseStatusLine($firstLine);

        /**
         * @link https://tools.ietf.org/html/rfc7231#section-6.2.1
         */
        if ($response->statusCode === static::STATUS_CODE_100) {
            $next = array_shift($lines); // take next line
            $next = empty($next) ? array_shift($lines) : $next; // take next or skip if empty
            $response->parseStatusLine($next);
        }

        if (count($lines) === 0) {
            return $response;
        }

        $isHeader = true;
        $headers = $content = [];

        foreach ($lines as $line) {
            if ($isHeader && $line === '') {
                $isHeader = false;
                continue;
            }

            if ($isHeader) {
                if (preg_match("/[\r\n]/", $line)) {
                    throw new Exception\RuntimeException('CRLF injection detected');
                }
                $headers[] = $line;
                continue;
            }

            if (empty($content)
                && preg_match('/^[a-z0-9!#$%&\'*+.^_`|~-]+:$/i', $line)
            ) {
                throw new Exception\RuntimeException('CRLF injection detected');
            }

            $content[] = $line;
        }

        if ($headers) {
            $response->headers = implode("\r\n", $headers);
        }

        if ($content) {
            $response->setContent(implode("\r\n", $content));
        }

        return $response;
    }

    /**
     * @param string $line
     * @throws Exception\InvalidArgumentException
     * @throws Exception\RuntimeException
     */
    protected function parseStatusLine($line)
    {
        $regex   = '/^HTTP\/(?P<version>1\.[01]) (?P<status>\d{3})(?:[ ]+(?P<reason>.*))?$/';
        $matches = [];
        if (! preg_match($regex, $line, $matches)) {
            throw new Exception\InvalidArgumentException(
                'A valid response status line was not found in the provided string'
            );
        }

        $this->version = $matches['version'];
        $this->setStatusCode($matches['status']);
        $this->setReasonPhrase((isset($matches['reason']) ? $matches['reason'] : ''));
    }

    /**
     * @return Header\SetCookie[]
     */
    public function getCookie()
    {
        return $this->getHeaders()->get('Set-Cookie');
    }

    /**
     * Set HTTP status code and (optionally) message
     *
     * @param  int $code
     * @throws Exception\InvalidArgumentException
     * @return self
     */
    public function setStatusCode($code)
    {
        $const = get_class($this) . '::STATUS_CODE_' . $code;
        if (! is_numeric($code) || ! defined($const)) {
            $code = is_scalar($code) ? $code : gettype($code);
            throw new Exception\InvalidArgumentException(sprintf(
                'Invalid status code provided: "%s"',
                $code
            ));
        }

        return $this->saveStatusCode($code);
    }

    /**
     * Retrieve HTTP status code
     *
     * @return int
     */
    public function getStatusCode()
    {
        return $this->statusCode;
    }

    /**
     * Set custom HTTP status code
     *
     * @param  int $code
     * @throws Exception\InvalidArgumentException
     * @return self
     */
    public function setCustomStatusCode($code)
    {
        if (! is_numeric($code)) {
            $code = is_scalar($code) ? $code : gettype($code);
            throw new Exception\InvalidArgumentException(sprintf(
                'Invalid status code provided: "%s"',
                $code
            ));
        }

        return $this->saveStatusCode($code);
    }

    /**
     * Assign status code
     *
     * @param int $code
     * @return self
     */
    protected function saveStatusCode($code)
    {
        $this->reasonPhrase = null;
        $this->statusCode = (int) $code;
        return $this;
    }

    /**
     * @param string $reasonPhrase
     * @return self
     */
    public function setReasonPhrase($reasonPhrase)
    {
        $this->reasonPhrase = trim($reasonPhrase);
        return $this;
    }

    /**
     * Get HTTP status message
     *
     * @return string
     */
    public function getReasonPhrase()
    {
        if (null == $this->reasonPhrase && isset($this->recommendedReasonPhrases[$this->statusCode])) {
            $this->reasonPhrase = $this->recommendedReasonPhrases[$this->statusCode];
        }
        return $this->reasonPhrase;
    }

    /**
     * Get the body of the response
     *
     * @return string
     */
    public function getBody()
    {
        $body = (string) $this->getContent();

        $transferEncoding = $this->getHeaders()->get('Transfer-Encoding');

        if (! empty($transferEncoding)) {
            if (strtolower($transferEncoding->getFieldValue()) === 'chunked') {
                $body = $this->decodeChunkedBody($body);
            }
        }

        $contentEncoding = $this->getHeaders()->get('Content-Encoding');

        if (! empty($contentEncoding)) {
            $contentEncoding = $contentEncoding->getFieldValue();
            if ($contentEncoding === 'gzip') {
                $body = $this->decodeGzip($body);
            } elseif ($contentEncoding === 'deflate') {
                $body = $this->decodeDeflate($body);
            }
        }

        return $body;
    }

    /**
     * Does the status code indicate a client error?
     *
     * @return bool
     */
    public function isClientError()
    {
        $code = $this->getStatusCode();
        return ($code < 500 && $code >= 400);
    }

    /**
     * Is the request forbidden due to ACLs?
     *
     * @return bool
     */
    public function isForbidden()
    {
        return (403 === $this->getStatusCode());
    }

    /**
     * Is the current status "informational"?
     *
     * @return bool
     */
    public function isInformational()
    {
        $code = $this->getStatusCode();
        return ($code >= 100 && $code < 200);
    }

    /**
     * Does the status code indicate the resource is not found?
     *
     * @return bool
     */
    public function isNotFound()
    {
        return (404 === $this->getStatusCode());
    }

    /**
     * Does the status code indicate the resource is gone?
     *
     * @return bool
     */
    public function isGone()
    {
        return (410 === $this->getStatusCode());
    }

    /**
     * Do we have a normal, OK response?
     *
     * @return bool
     */
    public function isOk()
    {
        return (200 === $this->getStatusCode());
    }

    /**
     * Does the status code reflect a server error?
     *
     * @return bool
     */
    public function isServerError()
    {
        $code = $this->getStatusCode();
        return (500 <= $code && 600 > $code);
    }

    /**
     * Do we have a redirect?
     *
     * @return bool
     */
    public function isRedirect()
    {
        $code = $this->getStatusCode();
        return (300 <= $code && 400 > $code);
    }

    /**
     * Was the response successful?
     *
     * @return bool
     */
    public function isSuccess()
    {
        $code = $this->getStatusCode();
        return (200 <= $code && 300 > $code);
    }

    /**
     * Render the status line header
     *
     * @return string
     */
    public function renderStatusLine()
    {
        $status = sprintf(
            'HTTP/%s %d %s',
            $this->getVersion(),
            $this->getStatusCode(),
            $this->getReasonPhrase()
        );
        return trim($status);
    }

    /**
     * Render entire response as HTTP response string
     *
     * @return string
     */
    public function toString()
    {
        $str  = $this->renderStatusLine() . "\r\n";
        $str .= $this->getHeaders()->toString();
        $str .= "\r\n";
        $str .= $this->getContent();
        return $str;
    }

    /**
     * Decode a "chunked" transfer-encoded body and return the decoded text
     *
     * @param  string $body
     * @return string
     * @throws Exception\RuntimeException
     */
    protected function decodeChunkedBody($body)
    {
        $decBody = '';

        $offset = 0;

        while (true) {
            if (! preg_match("/^([\da-fA-F]+)[^\r\n]*\r\n/sm", $body, $m, 0, $offset)) {
                if (trim(substr($body, $offset))) {
                    // Message was not consumed completely!
                    throw new Exception\RuntimeException(
                        'Error parsing body - doesn\'t seem to be a chunked message'
                    );
                }
                // Message was consumed completely
                break;
            }

            $length   = hexdec(trim($m[1]));
            $cut      = strlen($m[0]);
            $decBody .= substr($body, $offset + $cut, $length);
            $offset += $cut + $length + 2;
        }

        return $decBody;
    }

    /**
     * Decode a gzip encoded message (when Content-encoding = gzip)
     *
     * Currently requires PHP with zlib support
     *
     * @param  string $body
     * @return string
     * @throws Exception\RuntimeException
     */
    protected function decodeGzip($body)
    {
        if (! function_exists('gzinflate')) {
            throw new Exception\RuntimeException(
                'zlib extension is required in order to decode "gzip" encoding'
            );
        }

        ErrorHandler::start();
        $return = gzinflate(substr($body, 10));
        $test = ErrorHandler::stop();
        if ($test) {
            throw new Exception\RuntimeException(
                'Error occurred during gzip inflation',
                0,
                $test
            );
        }
        return $return;
    }

    /**
     * Decode a zlib deflated message (when Content-encoding = deflate)
     *
     * Currently requires PHP with zlib support
     *
     * @param  string $body
     * @return string
     * @throws Exception\RuntimeException
     */
    protected function decodeDeflate($body)
    {
        if (! function_exists('gzuncompress')) {
            throw new Exception\RuntimeException(
                'zlib extension is required in order to decode "deflate" encoding'
            );
        }

        /**
         * Some servers (IIS ?) send a broken deflate response, without the
         * RFC-required zlib header.
         *
         * We try to detect the zlib header, and if it does not exist we
         * teat the body is plain DEFLATE content.
         *
         * This method was adapted from PEAR HTTP_Request2 by (c) Alexey Borzov
         *
         * @link http://framework.zend.com/issues/browse/ZF-6040
         */
        $zlibHeader = unpack('n', substr($body, 0, 2));

        if ($zlibHeader[1] % 31 === 0) {
            return gzuncompress($body);
        }
        return gzinflate($body);
    }
}
