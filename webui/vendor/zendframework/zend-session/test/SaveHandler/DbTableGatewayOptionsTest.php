<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\SaveHandler;

use PHPUnit\Framework\TestCase;
use Zend\Session\SaveHandler\DbTableGatewayOptions;

/**
 * Unit testing for DbTableGatewayOptions
 *
 * @group      Zend_Session
 * @covers Zend\Session\SaveHandler\DbTableGatewayOptions
 */
class DbTableGatewayOptionsTest extends TestCase
{
    public function testDefaults()
    {
        $options = new DbTableGatewayOptions();
        $this->assertEquals('id', $options->getIdColumn());
        $this->assertEquals('name', $options->getNameColumn());
        $this->assertEquals('modified', $options->getModifiedColumn());
        $this->assertEquals('lifetime', $options->getLifetimeColumn());
        $this->assertEquals('data', $options->getDataColumn());
    }

    public function testSetConstructor()
    {
        $options = new DbTableGatewayOptions([
            'idColumn' => 'testId',
            'nameColumn' => 'testName',
            'modifiedColumn' => 'testModified',
            'lifetimeColumn' => 'testLifetime',
            'dataColumn' => 'testData',
        ]);

        $this->assertEquals('testId', $options->getIdColumn());
        $this->assertEquals('testName', $options->getNameColumn());
        $this->assertEquals('testModified', $options->getModifiedColumn());
        $this->assertEquals('testLifetime', $options->getLifetimeColumn());
        $this->assertEquals('testData', $options->getDataColumn());
    }

    public function testSetters()
    {
        $options = new DbTableGatewayOptions();
        $options->setIdColumn('testId')
                ->setNameColumn('testName')
                ->setModifiedColumn('testModified')
                ->setLifetimeColumn('testLifetime')
                ->setDataColumn('testData');

        $this->assertEquals('testId', $options->getIdColumn());
        $this->assertEquals('testName', $options->getNameColumn());
        $this->assertEquals('testModified', $options->getModifiedColumn());
        $this->assertEquals('testLifetime', $options->getLifetimeColumn());
        $this->assertEquals('testData', $options->getDataColumn());
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidIdColumn()
    {
        $options = new DbTableGatewayOptions([
            'idColumn' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidNameColumn()
    {
        $options = new DbTableGatewayOptions([
            'nameColumn' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidModifiedColumn()
    {
        $options = new DbTableGatewayOptions([
            'modifiedColumn' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidLifetimeColumn()
    {
        $options = new DbTableGatewayOptions([
            'lifetimeColumn' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidDataColumn()
    {
        $options = new DbTableGatewayOptions([
            'dataColumn' => null,
        ]);
    }
}
