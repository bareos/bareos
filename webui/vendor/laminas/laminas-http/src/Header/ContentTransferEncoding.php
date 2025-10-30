<?php

/**
 * @see       https://github.com/laminas/laminas-http for the canonical source repository
 * @copyright https://github.com/laminas/laminas-http/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-http/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Http\Header;

/**
 * @throws Exception\InvalidArgumentException
 * @see http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.11 @todo find section
 */
class ContentTransferEncoding implements HeaderInterface
{
    /**
     * @var string
     */
    protected $value;

    public static function fromString($headerLine)
    {
        list($name, $value) = GenericHeader::splitHeaderLine($headerLine);

        // check to ensure proper header type for this factory
        if (strtolower($name) !== 'content-transfer-encoding') {
            throw new Exception\InvalidArgumentException(sprintf(
                'Invalid header line for Content-Transfer-Encoding string: "%s"',
                $name
            ));
        }

        // @todo implementation details
        $header = new static(strtolower($value));

        return $header;
    }

    public function __construct($value = null)
    {
        if ($value) {
            HeaderValue::assertValid($value);
            $this->value = $value;
        }
    }

    public function getFieldName()
    {
        return 'Content-Transfer-Encoding';
    }

    public function getFieldValue()
    {
        return $this->value;
    }

    public function toString()
    {
        return 'Content-Transfer-Encoding: ' . $this->getFieldValue();
    }
}
