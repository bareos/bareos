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
use Zend\Session\SaveHandler\MongoDBOptions;

/**
 * @group      Zend_Session
 * @covers Zend\Session\SaveHandler\MongoDbOptions
 */
class MongoDBOptionsTest extends TestCase
{
    public function testDefaults()
    {
        $options = new MongoDBOptions();
        $this->assertNull($options->getDatabase());
        $this->assertNull($options->getCollection());
        $mongoVersion = phpversion('mongo') ?: '0.0.0';
        $defaultSaveOptions = version_compare($mongoVersion, '1.3.0', '<') ? ['safe' => true] : ['w' => 1];
        $this->assertEquals($defaultSaveOptions, $options->getSaveOptions());
        $this->assertEquals('name', $options->getNameField());
        $this->assertEquals('data', $options->getDataField());
        $this->assertEquals('lifetime', $options->getLifetimeField());
        $this->assertEquals('modified', $options->getModifiedField());
    }

    public function testSetConstructor()
    {
        $options = new MongoDBOptions([
            'database' => 'testDatabase',
            'collection' => 'testCollection',
            'saveOptions' => ['w' => 2],
            'nameField' => 'testName',
            'dataField' => 'testData',
            'lifetimeField' => 'testLifetime',
            'modifiedField' => 'testModified',
        ]);

        $this->assertEquals('testDatabase', $options->getDatabase());
        $this->assertEquals('testCollection', $options->getCollection());
        $this->assertEquals(['w' => 2], $options->getSaveOptions());
        $this->assertEquals('testName', $options->getNameField());
        $this->assertEquals('testData', $options->getDataField());
        $this->assertEquals('testLifetime', $options->getLifetimeField());
        $this->assertEquals('testModified', $options->getModifiedField());
    }

    public function testSetters()
    {
        $options = new MongoDBOptions();
        $options->setDatabase('testDatabase')
                ->setCollection('testCollection')
                ->setSaveOptions(['w' => 2])
                ->setNameField('testName')
                ->setDataField('testData')
                ->setLifetimeField('testLifetime')
                ->setModifiedField('testModified');

        $this->assertEquals('testDatabase', $options->getDatabase());
        $this->assertEquals('testCollection', $options->getCollection());
        $this->assertEquals(['w' => 2], $options->getSaveOptions());
        $this->assertEquals('testName', $options->getNameField());
        $this->assertEquals('testData', $options->getDataField());
        $this->assertEquals('testLifetime', $options->getLifetimeField());
        $this->assertEquals('testModified', $options->getModifiedField());
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidDatabase()
    {
        $options = new MongoDBOptions([
            'database' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidCollection()
    {
        $options = new MongoDBOptions([
            'collection' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidSaveOptions()
    {
        $options = new MongoDBOptions([
            'saveOptions' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidNameField()
    {
        $options = new MongoDBOptions([
            'nameField' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidModifiedField()
    {
        $options = new MongoDBOptions([
            'modifiedField' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidLifetimeField()
    {
        $options = new MongoDBOptions([
            'lifetimeField' => null,
        ]);
    }

    /**
     * @expectedException Zend\Session\Exception\InvalidArgumentException
     */
    public function testInvalidDataField()
    {
        $options = new MongoDBOptions([
            'dataField' => null,
        ]);
    }
}
