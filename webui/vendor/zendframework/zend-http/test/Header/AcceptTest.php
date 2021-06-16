<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Accept;
use Zend\Http\Header\Accept\FieldValuePart\AbstractFieldValuePart;
use Zend\Http\Header\Accept\FieldValuePart\AcceptFieldValuePart;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class AcceptTest extends TestCase
{
    public function testInvalidHeaderLine()
    {
        $this->expectException(InvalidArgumentException::class);
        Accept::fromString('');
    }

    public function testAcceptFromStringCreatesValidAcceptHeader()
    {
        $acceptHeader = Accept::fromString('Accept: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $acceptHeader);
        $this->assertInstanceOf(Accept::class, $acceptHeader);
    }

    public function testAcceptGetFieldNameReturnsHeaderName()
    {
        $acceptHeader = new Accept();
        $this->assertEquals('Accept', $acceptHeader->getFieldName());
    }

    public function testAcceptGetFieldValueReturnsProperValue()
    {
        $acceptHeader = Accept::fromString('Accept: xxx');
        $this->assertEquals('xxx', $acceptHeader->getFieldValue());
    }

    public function testAcceptGetFieldValueReturnsProperValueWithAHeaderWithoutSpaces()
    {
        $acceptHeader = Accept::fromString('Accept:xxx');
        $this->assertEquals('xxx', $acceptHeader->getFieldValue());
    }

    public function testAcceptToStringReturnsHeaderFormattedString()
    {
        $acceptHeader = new Accept();
        $acceptHeader->addMediaType('text/html', 0.8)
                     ->addMediaType('application/json', 1)
                     ->addMediaType('application/atom+xml', 0.9);

        // @todo set some values, then test output
        $this->assertEquals(
            'Accept: text/html;q=0.8, application/json, application/atom+xml;q=0.9',
            $acceptHeader->toString()
        );

        $this->expectException(InvalidArgumentException::class);
        $acceptHeader->addMediaType('\\', 0.9);
    }

    /** Implementation specific tests here */

    public function testCanParseCommaSeparatedValues()
    {
        $header = Accept::fromString('Accept: text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c');
        $this->assertTrue($header->hasMediaType('text/plain'));
        $this->assertTrue($header->hasMediaType('text/html'));
        $this->assertTrue($header->hasMediaType('text/x-dvi'));
        $this->assertTrue($header->hasMediaType('text/x-c'));
    }

    public function testPrioritizesValuesBasedOnQParameter()
    {
        $header   = Accept::fromString(
            'Accept: text/plain; q=0.5, text/html, text/xml; q=0, text/x-dvi; q=0.8, text/x-c'
        );
        $expected = [
            'text/html',
            'text/x-c',
            'text/x-dvi',
            'text/plain',
            'text/xml',
        ];

        foreach ($header->getPrioritized() as $type) {
            $this->assertEquals(array_shift($expected), $type->typeString);
        }
    }

    public function testLevel()
    {
        $acceptHeader = new Accept();
        $acceptHeader->addMediaType('text/html', 0.8, ['level' => 1])
                     ->addMediaType('text/html', 0.4, ['level' => 2])
                     ->addMediaType('application/atom+xml', 0.9);

        $this->assertEquals(
            'Accept: text/html;q=0.8;level=1, '
            . 'text/html;q=0.4;level=2, application/atom+xml;q=0.9',
            $acceptHeader->toString()
        );
    }

    public function testPrioritizedLevel()
    {
        $header = Accept::fromString(
            'Accept: text/*;q=0.3, text/html;q=0.7, text/html;level=1,'
            . 'text/html;level=2;q=0.4, */*;q=0.5'
        );

        $expected = [
            'text/html;level=1',
            'text/html;q=0.7',
            '*/*;q=0.5',
            'text/html;level=2;q=0.4',
            'text/*;q=0.3',
        ];

        foreach ($header->getPrioritized() as $type) {
            $this->assertEquals(array_shift($expected), $type->raw);
        }
    }

    public function testPrios()
    {
        $values = [
            'invalidPrio' => false,
            -0.0001       => false,
            1.0001        => false,
            1.000         => true,
            0.999         => true,
            0.000         => true,
            0.001         => true,
            1             => true,
            0             => true,
        ];

        $header = new Accept();
        foreach ($values as $prio => $shouldPass) {
            try {
                $header->addMediaType('text/html', $prio);
                if (! $shouldPass) {
                    $this->fail('Exception expected');
                }
            } catch (InvalidArgumentException $e) {
                if ($shouldPass) {
                    throw $e;
                }
            }
        }
    }

    public function testWildcharMediaType()
    {
        $acceptHeader = new Accept();
        $acceptHeader->addMediaType('text/*', 0.8)
                     ->addMediaType('application/*', 1)
                     ->addMediaType('*/*', 0.4);

        $this->assertTrue($acceptHeader->hasMediaType('text/html'));
        $this->assertTrue($acceptHeader->hasMediaType('application/atom+xml'));
        $this->assertTrue($acceptHeader->hasMediaType('audio/basic'));
        $this->assertEquals('Accept: application/*, text/*;q=0.8, */*;q=0.4', $acceptHeader->toString());
    }

    public function testMatchWildCard()
    {
        $acceptHeader = Accept::fromString('Accept: */*');
        $this->assertTrue($acceptHeader->hasMediaType('application/vnd.foobar+json'));

        $acceptHeader = Accept::fromString('Accept: application/*');
        $this->assertTrue($acceptHeader->hasMediaType('application/vnd.foobar+json'));
        $this->assertTrue($acceptHeader->hasMediaType('application/vnd.foobar+*'));

        $acceptHeader = Accept::fromString('Accept: application/vnd.foobar+html');
        $this->assertTrue($acceptHeader->hasMediaType('*/html'));
        $this->assertTrue($acceptHeader->hasMediaType('application/vnd.foobar+*'));

        $acceptHeader = Accept::fromString('Accept: text/html');
        $this->assertTrue($acceptHeader->hasMediaType('*/html'));
        $this->assertTrue($acceptHeader->hasMediaType('*/*+html'));

        $this->assertTrue($acceptHeader->hasMediaType('text/*'));
    }

    public function testParsingAndAssemblingQuotedStrings()
    {
        $acceptStr = 'Accept: application/vnd.foobar+html;q=1;version="2'
                   . '\"";level="foo;, bar", text/json;level=1, text/xml;level=2;q=0.4';
        $acceptHeader = Accept::fromString($acceptStr);

        $this->assertEquals($acceptStr, $acceptHeader->getFieldName() . ': ' . $acceptHeader->getFieldValue());
    }

    public function testMatchReturnsMatchedAgainstObject()
    {
        $acceptStr = 'Accept: text/html;q=1; version=23; level=5, text/json;level=1,text/xml;level=2;q=0.4';
        $acceptHeader = Accept::fromString($acceptStr);

        $res = $acceptHeader->match('text/html; _randomValue=foobar');
        $this->assertInstanceOf(AbstractFieldValuePart::class, $res->getMatchedAgainst());
        $this->assertEquals(
            'foobar',
            $res->getMatchedAgainst()->getParams()->_randomValue
        );

        $acceptStr = 'Accept: */*; ';
        $acceptHeader = Accept::fromString($acceptStr);

        $res = $acceptHeader->match('text/html; _foo=bar');
        $this->assertInstanceOf(AbstractFieldValuePart::class, $res->getMatchedAgainst());

        $this->assertEquals(
            'bar',
            $res->getMatchedAgainst()->getParams()->_foo
        );
    }

    public function testVersioning()
    {
        $acceptStr = 'Accept: text/html;q=1; version=23; level=5, text/json;level=1,text/xml;level=2;q=0.4';
        $acceptHeader = Accept::fromString($acceptStr);

        $expected = [
            'typeString' => 'text/html',
            'type'       => 'text',
            'subtype'    => 'html',
            'subtypeRaw' => 'html',
            'format'     => 'html',
            'priority'   => 1,
            'params'     => ['q' => 1, 'version' => 23, 'level' => 5],
            'raw'        => 'text/html;q=1; version=23; level=5',
        ];

        $this->assertFalse($acceptHeader->match('text/html; version=22'));

        $res = $acceptHeader->match('text/html; version=23');
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }

        $this->assertFalse($acceptHeader->match('text/html; version=24'));

        $res = $acceptHeader->match('text/html; version=22-24');
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }

        $this->assertFalse($acceptHeader->match('text/html; version=20|22|24'));

        $res = $acceptHeader->match('text/html; version=22|23|24');
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }
    }

    public function testWildcardWithDifferentParamsAndRanges()
    {
        $acceptHeader = Accept::fromString('Accept: */*; version=21');
        $this->assertFalse($acceptHeader->match('*/*; version=20|22'));

        $acceptHeader = Accept::fromString('Accept: */*; version=19');
        $this->assertFalse($acceptHeader->match('*/*; version=20-22'));

        $acceptHeader = Accept::fromString('Accept: */*; version=23');
        $this->assertFalse($acceptHeader->match('*/*; version=20-22'));

        $acceptHeader = Accept::fromString('Accept: */*; version=21');
        $res = $acceptHeader->match('*/*; version=20-22');
        $this->assertInstanceOf(AcceptFieldValuePart::class, $res);
        $this->assertEquals('21', $res->getParams()->version);
    }

    /**
     * @group 3739
     * @covers Zend\Http\Header\AbstractAccept::matchAcceptParams()
     */
    public function testParamRangesWithDecimals()
    {
        $acceptHeader = Accept::fromString('Accept: application/vnd.com.example+xml; version=10');
        $this->assertFalse($acceptHeader->match('application/vnd.com.example+xml; version="\"3.1\"-\"3.1.1-DEV\""'));
    }

    /**
     * @group 3740
     * @covers Zend\Http\Header\AbstractAccept::matchAcceptParams()
     * @covers Zend\Http\Header\AbstractAccept::getParametersFromFieldValuePart()
     *
     * @dataProvider provideParamRanges
     *
     * @param string $range
     * @param string $input
     * @param bool $success
     */
    public function testParamRangesSupportDevStage($range, $input, $success)
    {
        $acceptHeader = Accept::fromString(
            'Accept: application/vnd.com.example+xml; version="' . addslashes($input) . '"'
        );

        $res = $acceptHeader->match(
            'application/vnd.com.example+xml; version="' . addslashes($range) . '"'
        );

        if ($success) {
            $this->assertInstanceOf(AcceptFieldValuePart::class, $res);
        } else {
            $this->assertFalse($res);
        }
    }

    /**
     * @group 3740
     * @return array
     */
    public function provideParamRanges()
    {
        return [
            ['"3.1.1-DEV"-3.1.1', '3.1.1', true],
            ['3.1.0-"3.1.1-DEV"', '3.1.1', false],
            ['3.1.0-"3.1.1-DEV"', '3.1.1-DEV', true],
            ['3.1.0-"3.1.1-DEV"', '3.1.2-DEV', false],
            ['3.1.0-"3.1.1"', '3.1.2-DEV', false],
            ['3.1.0-"3.1.1"', '3.1.0-DEV', false],
            ['"3.1.0-DEV"-"3.1.1-BETA"', '3.1.0', true],
            ['"3.1.0-DEV"-"3.1.1-BETA"', '3.1.1', false],
            ['"3.1.0-DEV"-"3.1.1-BETA"', '3.1.1-BETA', true],
            ['"3.1.0-DEV"-"3.1.1-BETA"', '3.1.0-DEV', true],
        ];
    }

    public function testVersioningAndPriorization()
    {
        $acceptStr = 'Accept: text/html; version=23, text/json; version=15.3; q=0.9,text/html;level=2;q=0.4';
        $acceptHeader = Accept::fromString($acceptStr);

        $expected = [
            'typeString' => 'text/json',
            'type'       => 'text',
            'subtype'    => 'json',
            'subtypeRaw' => 'json',
            'format'     => 'json',
            'priority'   => 0.9,
            'params'     => ['q' => 0.9, 'version' => 15.3],
            'raw'        => 'text/json; version=15.3; q=0.9',
        ];

        $str = 'text/html; version=17, text/json; version=15-16';
        $res = $acceptHeader->match($str);
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }

        $expected = (object) [
            'typeString' => 'text/html',
            'type'       => 'text',
            'subtype'    => 'html',
            'subtypeRaw' => 'html',
            'format'     => 'html',
            'priority'   => 0.4,
            'params'     => ['q' => 0.4, 'level' => 2],
            'raw'        => 'text/html;level=2;q=0.4',
        ];

        $str = 'text/html; version=17,text/json; version=15-16; q=0.5';
        $res = $acceptHeader->match($str);
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }
    }

    public function testPrioritizing()
    {
        // Example is copy/paste from rfc2616
        $acceptStr = 'Accept: text/*;q=0.3, */*,text/html;q=1, text/html;level=1,text/html;level=2;q=0.4, */*;q=0.5';
        $acceptHdr = Accept::fromString($acceptStr);

        $expected = [
            'typeString' => 'text/html',
            'type'       => 'text',
            'subtype'    => 'html',
            'subtypeRaw' => 'html',
            'format'     => 'html',
            'priority'   => 1,
            'params'     => ['level' => 1],
            'raw'        => 'text/html;level=1',
        ];

        $res = $acceptHdr->match('text/html');
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }

        $res = $acceptHdr->match('text');
        foreach ($expected as $key => $value) {
            $this->assertEquals($value, $res->$key);
        }
    }

    public function testPrioritizing2()
    {
        $accept = Accept::fromString("Accept: application/text, \tapplication/*");
        $res = $accept->getPrioritized();
        $this->assertEquals('application/text', $res[0]->raw);
        $this->assertEquals('application/*', $res[1]->raw);

        $accept = Accept::fromString("Accept: \tapplication/*, application/text");
        $res = $accept->getPrioritized();
        $this->assertEquals('application/text', $res[0]->raw);
        $this->assertEquals('application/*', $res[1]->raw);

        $accept = Accept::fromString('Accept: text/xml, application/xml');
        $res = $accept->getPrioritized();
        $this->assertEquals('application/xml', $res[0]->raw);
        $this->assertEquals('text/xml', $res[1]->raw);

        $accept = Accept::fromString('Accept: application/xml, text/xml');
        $res = $accept->getPrioritized();
        $this->assertEquals('application/xml', $res[0]->raw);
        $this->assertEquals('text/xml', $res[1]->raw);

        $accept = Accept::fromString('Accept: application/vnd.foobar+xml; q=0.9, text/xml');
        $res = $accept->getPrioritized();
        $this->assertEquals(1.0, $res[0]->getPriority());
        $this->assertEquals(0.9, $res[1]->getPriority());
        $this->assertEquals('application/vnd.foobar+xml', $res[1]->getTypeString());
        $this->assertEquals('vnd.foobar+xml', $res[1]->getSubtypeRaw());
        $this->assertEquals('vnd.foobar', $res[1]->getSubtype());
        $this->assertEquals('xml', $res[1]->getFormat());

        $accept = Accept::fromString('Accept: text/xml, application/vnd.foobar+xml; version="\'Ѿ"');
        $res = $accept->getPrioritized();
        $this->assertEquals('application/vnd.foobar+xml; version="\'Ѿ"', $res[0]->getRaw());
    }

    public function testWildcardDefaults()
    {
        $this->markTestIncomplete('No wildcard defaults implemented yet');

        $expected = (object) [
            'typeString' => 'image',
            'type'       => 'image',
            'subtype'    => '*',
            'subtypeRaw' => '',
            'format'     => 'jpeg',
            'priority'   => 1,
            'params'     => [],
            'raw'        => 'image',
        ];

        $this->assertEquals($expected, $acceptHdr->match('image'));
        //  $this->assertEquals($expected, $this->_handler->match('text'));
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Accept::fromString("Accept: application/text\r\n\r\nevilContent");
    }

    public function testGetEmptyFieldValue()
    {
        $accept = new Accept();
        $accept->getFieldValue();
    }
}
