<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\SaveHandler;

use MongoDB\Client as MongoClient;
use MongoDB\Collection as MongoCollection;
use PHPUnit\Framework\TestCase;
use Zend\Session\SaveHandler\MongoDB;
use Zend\Session\SaveHandler\MongoDBOptions;

/**
 * @group      Zend_Session
 * @covers Zend\Session\SaveHandler\MongoDb
 * @requires extension mongodb
 */
class MongoDBTest extends TestCase
{
    /**
     * @var MongoClient
     */
    protected $mongoClient;

    /**
     * MongoCollection instance
     *
     * @var MongoCollection
     */
    protected $mongoCollection;

    /**
     * @var MongoDBOptions
     */
    protected $options;

    /**
     * Setup performed prior to each test method
     *
     * @return void
     */
    public function setUp()
    {
        $this->options = new MongoDBOptions([
            'database' => 'zf2_tests',
            'collection' => 'sessions',
        ]);

        $this->mongoClient = new MongoClient();
        $this->mongoCollection = $this->mongoClient->selectCollection(
            $this->options->getDatabase(),
            $this->options->getCollection()
        );
    }

    /**
     * Tear-down operations performed after each test method
     *
     * @return void
     */
    public function tearDown()
    {
        if ($this->mongoCollection) {
            $this->mongoCollection->drop();
        }
    }

    public function testReadWrite()
    {
        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $id = '242';
        $data = ['foo' => 'bar', 'bar' => ['foo' => 'bar']];

        $this->assertTrue($saveHandler->write($id, serialize($data)));
        $this->assertEquals($data, unserialize($saveHandler->read($id)));

        $data = ['foo' => [1, 2, 3]];

        $this->assertTrue($saveHandler->write($id, serialize($data)));
        $this->assertEquals($data, unserialize($saveHandler->read($id)));
    }

    /**
     * @runInSeparateProcess
     */
    public function testReadDestroysExpiredSession()
    {
        /* Note: due to the session save handler's open() method reading the
         * "session.gc_maxlifetime" INI value directly, it's necessary to set
         * that to simulate natural session expiration.
         */
        $oldMaxlifetime = ini_get('session.gc_maxlifetime');
        ini_set('session.gc_maxlifetime', 0);

        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $id = '242';
        $data = ['foo' => 'bar'];

        $this->assertNull($this->mongoCollection->findOne(['_id' => $id]));

        $this->assertTrue($saveHandler->write($id, serialize($data)));
        $this->assertNotNull($this->mongoCollection->findOne(['_id' => $id]));
        $this->assertEquals('', $saveHandler->read($id));
        $this->assertNull($this->mongoCollection->findOne(['_id' => $id]));

        ini_set('session.gc_maxlifetime', $oldMaxlifetime);
    }

    public function testGarbageCollection()
    {
        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $data = ['foo' => 'bar'];

        $this->assertTrue($saveHandler->write(123, serialize($data)));
        $this->assertTrue($saveHandler->write(456, serialize($data)));
        $this->assertEquals(2, $this->mongoCollection->count());
        $saveHandler->gc(5);
        $this->assertEquals(2, $this->mongoCollection->count());

        /* Note: MongoDate uses micro-second precision, so even a maximum
         * lifetime of zero would not match records that were just inserted.
         * Use a negative number instead.
         */
        $saveHandler->gc(-1);
        $this->assertEquals(0, $this->mongoCollection->count());
    }

    public function testGarbageCollectionMaxlifetimeIsInSeconds()
    {
        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $data = ['foo' => 'bar'];

        $this->assertTrue($saveHandler->write(123, serialize($data)));
        sleep(1);
        $this->assertTrue($saveHandler->write(456, serialize($data)));
        $this->assertEquals(2, $this->mongoCollection->count());

        // Clear everything what is at least 1 second old.
        $saveHandler->gc(1);
        $this->assertEquals(1, $this->mongoCollection->count());

        $this->assertEmpty($saveHandler->read(123));
        $this->assertNotEmpty($saveHandler->read(456));
    }

    /**
     * @expectedException \MongoDB\Driver\Exception\RuntimeException
     */
    public function testWriteExceptionEdgeCaseForChangedSessionName()
    {
        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $id = '242';
        $data = ['foo' => 'bar'];

        /* Note: a MongoCursorException will be thrown if a record with this ID
         * already exists with a different session name, since the upsert query
         * cannot insert a new document with the same ID and new session name.
         * This should only happen if ID's are not unique or if the session name
         * is altered mid-process.
         */
        $saveHandler->write($id, serialize($data));
        $saveHandler->open('savepath', 'sessionname_changed');
        $saveHandler->write($id, serialize($data));
    }

    public function testReadShouldAlwaysReturnString()
    {
        $saveHandler = new MongoDB($this->mongoClient, $this->options);
        $this->assertTrue($saveHandler->open('savepath', 'sessionname'));

        $id = '242';

        $data = $saveHandler->read($id);

        $this->assertTrue(is_string($data));
    }
}
