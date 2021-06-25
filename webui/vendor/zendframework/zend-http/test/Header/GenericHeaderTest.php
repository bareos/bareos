<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\GenericHeader;

class GenericHeaderTest extends TestCase
{
    /**
     * @dataProvider validFieldNameChars
     *
     * @param string $name
     */
    public function testValidFieldName($name)
    {
        try {
            new GenericHeader($name);
        } catch (InvalidArgumentException $e) {
            $this->assertEquals(
                $e->getMessage(),
                'Header name must be a valid RFC 7230 (section 3.2) field-name.'
            );
            $this->fail('Allowed char rejected: ' . ord($name)); // For easy debug
        }
    }

    /**
     * @dataProvider invalidFieldNameChars
     *
     * @param string $name
     */
    public function testInvalidFieldName($name)
    {
        try {
            new GenericHeader($name);
            $this->fail('Invalid char allowed: ' . ord($name)); // For easy debug
        } catch (InvalidArgumentException $e) {
            $this->assertEquals(
                $e->getMessage(),
                'Header name must be a valid RFC 7230 (section 3.2) field-name.'
            );
        }
    }

    /**
     * @group 7295
     */
    public function testDoesNotReplaceUnderscoresWithDashes()
    {
        $header = new GenericHeader('X_Foo_Bar');
        $this->assertEquals('X_Foo_Bar', $header->getFieldName());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        GenericHeader::fromString("X_Foo_Bar: Bar\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new GenericHeader('X_Foo_Bar', "Bar\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testProtectsFromCRLFAttackViaSetFieldName()
    {
        $header = new GenericHeader();
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('valid');
        $header->setFieldName("\rX-\r\nFoo-\nBar");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testProtectsFromCRLFAttackViaSetFieldValue()
    {
        $header = new GenericHeader();
        $this->expectException(InvalidArgumentException::class);
        $header->setFieldValue("\rSome\r\nCLRF\nAttack");
    }

    /**
     * Valid field name characters.
     *
     * @return string[]
     */
    public function validFieldNameChars()
    {
        return [
            ['!'],
            ['#'],
            ['$'],
            ['%'],
            ['&'],
            ["'"],
            ['*'],
            ['+'],
            ['-'],
            ['.'],
            ['0'], // Begin numeric range
            ['9'], // End numeric range
            ['A'], // Begin upper range
            ['Z'], // End upper range
            ['^'],
            ['_'],
            ['`'],
            ['a'], // Begin lower range
            ['z'], // End lower range
            ['|'],
            ['~'],
        ];
    }

    /**
     * Invalid field name characters.
     *
     * @return string[]
     */
    public function invalidFieldNameChars()
    {
        return [
            ["\x00"], // Min CTL invalid character range.
            ["\x1F"], // Max CTL invalid character range.
            ['('],
            [')'],
            ['<'],
            ['>'],
            ['@'],
            [','],
            [';'],
            [':'],
            ['\\'],
            ['"'],
            ['/'],
            ['['],
            [']'],
            ['?'],
            ['='],
            ['{'],
            ['}'],
            [' '],
            ["\t"],
            ["\x7F"], // DEL CTL invalid character.
        ];
    }
}
