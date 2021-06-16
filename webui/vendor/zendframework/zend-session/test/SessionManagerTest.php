<?php
/**
 * @see       https://github.com/zendframework/zend-session for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-session/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Session;

use ArrayIterator;
use DateTime;
use PHPUnit\Framework\TestCase;
use Zend\Session\Config\SessionConfig;
use Zend\Session\Config\StandardConfig;
use Zend\Session\Exception\InvalidArgumentException;
use Zend\Session\Exception\RuntimeException;
use Zend\Session\SessionManager;
use Zend\Session\Storage\ArrayStorage;
use Zend\Session\Storage\SessionStorage;
use Zend\Session\Storage\SessionArrayStorage;
use Zend\Session\Storage\StorageInterface;
use Zend\Session\Validator\Id;
use Zend\Session\Validator\RemoteAddr;

/**
 * @preserveGlobalState disabled
 * @covers Zend\Session\SessionManager
 */
class SessionManagerTest extends TestCase
{
    public $error;

    public $cookieDateFormat = 'D, d-M-y H:i:s e';

    /**
     * @var SessionManager
     */
    protected $manager;

    public function setUp()
    {
        $this->error = false;
    }

    public function handleErrors($errno, $errstr)
    {
        $this->error = $errstr;
    }

    public function getTimestampFromCookie($cookie)
    {
        if (preg_match('/expires=([^;]+)/', $cookie, $matches)) {
            $ts = new DateTime($matches[1]);
            return $ts;
        }
        return false;
    }

    public function testManagerUsesSessionConfigByDefault()
    {
        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $this->assertInstanceOf(SessionConfig::class, $config);
    }

    public function testCanPassConfigurationToConstructor()
    {
        $this->manager = new SessionManager();
        $config = new StandardConfig();
        $manager = new SessionManager($config);
        $this->assertSame($config, $manager->getConfig());
    }

    public function testManagerUsesSessionStorageByDefault()
    {
        $this->manager = new SessionManager();
        $storage = $this->manager->getStorage();
        $this->assertInstanceOf(SessionArrayStorage::class, $storage);
    }

    public function testCanPassStorageToConstructor()
    {
        $storage = new ArrayStorage();
        $manager = new SessionManager(null, $storage);
        $this->assertSame($storage, $manager->getStorage());
    }

    public function testCanPassSaveHandlerToConstructor()
    {
        $saveHandler = new TestAsset\TestSaveHandler();
        $manager = new SessionManager(null, null, $saveHandler);
        $this->assertSame($saveHandler, $manager->getSaveHandler());
    }

    public function testCanPassValidatorsToConstructor()
    {
        $validators = [
            'foo',
            'bar',
        ];
        $manager = new SessionManager(null, null, null, $validators);
        foreach ($validators as $validator) {
            $this->assertAttributeContains($validator, 'validators', $manager);
        }
    }

    public function testAttachDefaultValidatorsByDefault()
    {
        $manager = new SessionManager();
        $this->assertAttributeEquals([Id::class], 'validators', $manager);
    }

    public function testCanMergeValidatorsWithDefault()
    {
        $defaultValidators = [
            Id::class,
        ];
        $validators = [
            'foo',
            'bar',
        ];
        $manager = new SessionManager(null, null, null, $validators);
        $this->assertAttributeEquals(array_merge($defaultValidators, $validators), 'validators', $manager);
    }

    public function testCanDisableAttachDefaultValidators()
    {
        $options = [
            'attach_default_validators' => false,
        ];
        $manager = new SessionManager(null, null, null, [], $options);
        $this->assertAttributeEquals([], 'validators', $manager);
    }

    // Session-related functionality

    /**
     * @runInSeparateProcess
     */
    public function testSessionExistsReturnsFalseWhenNoSessionStarted()
    {
        $this->manager = new SessionManager();
        $this->assertFalse($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionExistsReturnsTrueWhenSessionStarted()
    {
        $this->manager = new SessionManager();
        session_start();
        $this->assertTrue($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionExistsReturnsTrueWhenSessionStartedThenWritten()
    {
        $this->manager = new SessionManager();
        session_start();
        session_write_close();
        $this->assertTrue($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionExistsReturnsFalseWhenSessionStartedThenDestroyed()
    {
        $this->manager = new SessionManager();
        session_start();
        session_destroy();
        $this->assertFalse($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionIsStartedAfterCallingStart()
    {
        $this->manager = new SessionManager();
        $this->assertFalse($this->manager->sessionExists());
        $this->manager->start();
        $this->assertTrue($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartDoesNothingWhenCalledAfterWriteCloseOperation()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $id1 = session_id();
        session_write_close();
        $this->manager->start();
        $id2 = session_id();
        $this->assertTrue($this->manager->sessionExists());
        $this->assertEquals($id1, $id2);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartWithOldTraversableSessionData()
    {
        // pre-populate session with data
        $_SESSION['key1'] = 'value1';
        $_SESSION['key2'] = 'value2';
        $storage = new SessionStorage();
        // create session manager with SessionStorage that will populate object with existing session array
        $manager = new SessionManager(null, $storage);
        $this->assertFalse($manager->sessionExists());
        $manager->start();
        $this->assertTrue($manager->sessionExists());
        $this->assertInstanceOf('\Traversable', $_SESSION);
        $this->assertEquals('value1', $_SESSION->key1);
        $this->assertEquals('value2', $_SESSION->key2);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStorageContentIsPreservedByWriteCloseOperation()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $storage = $this->manager->getStorage();
        $storage['foo'] = 'bar';
        $this->manager->writeClose();
        $this->assertArrayHasKey('foo', $storage);
        $this->assertEquals('bar', $storage['foo']);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartCreatesNewSessionIfPreviousSessionHasBeenDestroyed()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $id1 = session_id();
        session_destroy();
        $this->manager->start();
        $id2 = session_id();
        $this->assertTrue($this->manager->sessionExists());
        $this->assertNotEquals($id1, $id2);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartConvertsSessionDataFromStorageInterfaceToArrayBeforeMerging()
    {
        $this->manager = new SessionManager();

        $key            = 'testData';
        $data           = [$key => 'test'];
        $sessionStorage = $this->prophesize(StorageInterface::class);
        $_SESSION       = $sessionStorage->reveal();
        $sessionStorage->toArray()->shouldBeCalledTimes(1)->willReturn($data);

        $this->manager->start();

        $this->assertInternalType('array', $_SESSION);
        $this->assertArrayHasKey($key, $_SESSION);
        $this->assertSame($data[$key], $_SESSION[$key]);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartConvertsSessionDataFromTraversableToArrayBeforeMerging()
    {
        $this->manager = new SessionManager();

        $key      = 'testData';
        $data     = [$key => 'test'];
        $_SESSION = new ArrayIterator($data);

        $this->manager->start();

        $this->assertInternalType('array', $_SESSION);
        $this->assertArrayHasKey($key, $_SESSION);
        $this->assertSame($data[$key], $_SESSION[$key]);
    }

    /**
     * @outputBuffering disabled
     */
    public function testStartWillNotBlockHeaderSentNotices()
    {
        $this->manager = new SessionManager();
        if ('cli' == PHP_SAPI) {
            $this->markTestSkipped('session_start() will not raise headers_sent warnings in CLI');
        }
        set_error_handler([$this, 'handleErrors'], E_WARNING);
        echo ' ';
        $this->assertTrue(headers_sent());
        $this->manager->start();
        restore_error_handler();
        $this->assertInternalType('string', $this->error);
        $this->assertContains('already sent', $this->error);
    }

    /**
     * @runInSeparateProcess
     */
    public function testGetNameReturnsSessionName()
    {
        $this->manager = new SessionManager();
        $ini = ini_get('session.name');
        $this->assertEquals($ini, $this->manager->getName());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSetNameRaisesExceptionOnInvalidName()
    {
        $this->manager = new SessionManager();
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Name provided contains invalid characters; must be alphanumeric only');
        $this->manager->setName('foo bar!');
    }

    /**
     * @runInSeparateProcess
     */
    public function testSetNameSetsSessionNameOnSuccess()
    {
        $this->manager = new SessionManager();
        $this->manager->setName('foobar');
        $this->assertEquals('foobar', $this->manager->getName());
        $this->assertEquals('foobar', session_name());
    }

    /**
     * @runInSeparateProcess
     */
    public function testCanSetNewSessionNameAfterSessionDestroyed()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        session_destroy();
        $this->manager->setName('foobar');
        $this->assertEquals('foobar', $this->manager->getName());
        $this->assertEquals('foobar', session_name());
    }

    /**
     * @runInSeparateProcess
     */
    public function testSettingNameWhenAnActiveSessionExistsRaisesException()
    {
        $this->manager = new SessionManager();
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Cannot set session name after a session has already started');
        $this->manager->start();
        $this->manager->setName('foobar');
    }

    /**
     * @runInSeparateProcess
     */
    public function testDestroyByDefaultSendsAnExpireCookie()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $this->manager->start();
        $this->manager->destroy();

        echo '';

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName)) {
                $found = true;
            }
        }

        $this->assertTrue($found, 'No session cookie found: ' . var_export($headers, true));
    }

    /**
     * @runInSeparateProcess
     */
    public function testSendingFalseToSendExpireCookieWhenCallingDestroyShouldNotSendCookie()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $this->manager->start();
        $this->manager->destroy(['send_expire_cookie' => false]);

        echo '';

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName)) {
                $found = true;
            }
        }

        if ($found) {
            $this->assertNotContains('expires=', $header);
        } else {
            $this->assertFalse($found, 'Unexpected session cookie found: ' . var_export($headers, true));
        }
    }

    /**
     * @runInSeparateProcess
     */
    public function testDestroyDoesNotClearSessionStorageByDefault()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $storage = $this->manager->getStorage();
        $storage['foo'] = 'bar';
        $this->manager->destroy();
        $this->assertTrue(isset($storage['foo']));
        $this->assertEquals('bar', $storage['foo']);
    }

    /**
     * @runInSeparateProcess
     */
    public function testPassingClearStorageOptionWhenCallingDestroyClearsStorage()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $storage = $this->manager->getStorage();
        $storage['foo'] = 'bar';
        $this->manager->destroy(['clear_storage' => true]);
        $this->assertFalse(isset($storage['foo']));
    }

    /**
     * @runInSeparateProcess
     */
    public function testCallingWriteCloseMarksStorageAsImmutable()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $storage = $this->manager->getStorage();
        $storage['foo'] = 'bar';
        $this->manager->writeClose();
        $this->assertTrue($storage->isImmutable());
    }

    /**
     * @runInSeparateProcess
     */
    public function testCallingWriteCloseShouldNotAlterSessionExistsStatus()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $this->manager->writeClose();
        $this->assertTrue($this->manager->sessionExists());
    }

    /**
     * @runInSeparateProcess
     */
    public function testIdShouldBeEmptyPriorToCallingStart()
    {
        $this->manager = new SessionManager();
        $this->assertSame('', $this->manager->getId());
    }

    /**
     * @runInSeparateProcess
     */
    public function testIdShouldBeMutablePriorToCallingStart()
    {
        $this->manager = new SessionManager();
        $this->manager->setId(__CLASS__);
        $this->assertSame(__CLASS__, $this->manager->getId());
        $this->assertSame(__CLASS__, session_id());
    }

    /**
     * @runInSeparateProcess
     */
    public function testIdShouldNotBeMutableAfterSessionStarted()
    {
        $this->manager = new SessionManager();
        $this->expectException(RuntimeException::class, 'Session has already been started);
        $this->expectExceptionMessage(to change the session ID call regenerateId()');
        $this->manager->start();
        $origId = $this->manager->getId();
        $this->manager->setId(__METHOD__);
    }

    /**
     * @runInSeparateProcess
     */
    public function testRegenerateIdShouldWorkAfterSessionStarted()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $origId = $this->manager->getId();
        $this->manager->regenerateId();
        $this->assertNotSame($origId, $this->manager->getId());
    }

    /**
     * @runInSeparateProcess
     */
    public function testRegenerateIdDoesNothingWhenSessioIsNotStarted()
    {
        $this->manager = new SessionManager();
        $origId = $this->manager->getId();
        $this->manager->regenerateId();
        $this->assertEquals($origId, $this->manager->getId());
        $this->assertEquals('', $this->manager->getId());
    }

    /**
     * @runInSeparateProcess
     */
    public function testRegeneratingIdAfterSessionStartedShouldSendExpireCookie()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $this->manager->start();
        $origId = $this->manager->getId();
        $this->manager->regenerateId();

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName)) {
                $found = true;
            }
        }

        $this->assertTrue($found, 'No session cookie found: ' . var_export($headers, true));
    }

    /**
     * @runInSeparateProcess
     */
    public function testRememberMeShouldSendNewSessionCookieWithUpdatedTimestamp()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $this->manager->start();
        $this->manager->rememberMe(18600);

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();
        $cookie  = false;

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName) && ! stristr($header, '=deleted')) {
                $found  = true;
                $cookie = $header;
            }
        }

        $this->assertTrue($found, 'No session cookie found: ' . var_export($headers, true));

        $ts = $this->getTimestampFromCookie($cookie);
        if (! $ts) {
            $this->fail('Cookie did not contain expiry? ' . var_export($headers, true));
        }

        $this->assertGreaterThan(
            $_SERVER['REQUEST_TIME'],
            $ts->getTimestamp(),
            'Session cookie: ' . var_export($headers, 1)
        );
    }

    /**
     * @runInSeparateProcess
     */
    public function testRememberMeShouldSetTimestampBasedOnConfigurationByDefault()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $config->setRememberMeSeconds(3600);
        $ttl = $config->getRememberMeSeconds();
        $this->manager->start();
        $this->manager->rememberMe();

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();
        $cookie  = false;

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName) && ! stristr($header, '=deleted')) {
                $found  = true;
                $cookie = $header;
            }
        }

        $this->assertTrue($found, 'No session cookie found: ' . var_export($headers, true));

        $ts = $this->getTimestampFromCookie($cookie);
        if (! $ts) {
            $this->fail('Cookie did not contain expiry? ' . var_export($headers, true));
        }

        $compare  = $_SERVER['REQUEST_TIME'] + $ttl;
        $cookieTs = $ts->getTimestamp();
        $this->assertContains($cookieTs, range($compare, $compare + 10), 'Session cookie: ' . var_export($headers, 1));
    }

    /**
     * @runInSeparateProcess
     */
    public function testForgetMeShouldSendCookieWithZeroTimestamp()
    {
        if (! extension_loaded('xdebug')) {
            $this->markTestSkipped('Xdebug required for this test');
        }

        $this->manager = new SessionManager();
        $config = $this->manager->getConfig();
        $config->setUseCookies(true);
        $this->manager->start();
        $this->manager->forgetMe();

        $headers = xdebug_get_headers();
        $found   = false;
        $sName   = $this->manager->getName();

        foreach ($headers as $header) {
            if (stristr($header, 'Set-Cookie:') && stristr($header, $sName) && ! stristr($header, '=deleted')) {
                $found = true;
            }
        }

        $this->assertTrue($found, 'No session cookie found: ' . var_export($headers, true));
        $this->assertNotContains('expires=', $header);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartingSessionThatFailsAValidatorShouldRaiseException()
    {
        $this->manager = new SessionManager();
        $chain = $this->manager->getValidatorChain();
        $chain->attach('session.validate', [new TestAsset\TestFailingValidator(), 'isValid']);
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('failed');
        $this->manager->start();
    }

    /**
     * @runInSeparateProcess
     */
    public function testResumeSessionThatFailsAValidatorShouldRaiseException()
    {
        $this->manager = new SessionManager();
        $this->manager->setSaveHandler(new TestAsset\TestSaveHandlerWithValidator);
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('failed');
        $this->manager->start();
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionWriteCloseStoresMetadata()
    {
        $this->manager = new SessionManager();
        $this->manager->start();
        $storage = $this->manager->getStorage();
        $storage->setMetadata('foo', 'bar');
        $metaData = $storage->getMetadata();
        $this->manager->writeClose();
        $this->assertSame($_SESSION['__ZF'], $metaData);
    }

    /**
     * @runInSeparateProcess
     */
    public function testSessionValidationDoesNotHaltOnNoopListener()
    {
        $this->manager = new SessionManager();
        $validatorCalled = false;
        $validator = function () use (& $validatorCalled) {
            $validatorCalled = true;
        };

        $this->manager->getValidatorChain()->attach('session.validate', $validator);

        $this->assertTrue($this->manager->isValid());
        $this->assertTrue($validatorCalled);
    }

    /**
     * @runInSeparateProcess
     */
    public function testProducedSessionManagerWillNotReplaceSessionSuperGlobalValues()
    {
        $this->manager = new SessionManager();
        $_SESSION['foo'] = 'bar';

        $this->manager->start();

        $this->assertArrayHasKey('foo', $_SESSION);
        $this->assertSame('bar', $_SESSION['foo']);
    }

    /**
     * @runInSeparateProcess
     */
    public function testValidatorChainSessionMetadataIsPreserved()
    {
        $this->manager = new SessionManager();
        $this->manager->getValidatorChain()
            ->attach('session.validate', [new RemoteAddr(), 'isValid']);

        $this->assertFalse($this->manager->sessionExists());

        $this->manager->start();

        $this->assertInternalType('array', $_SESSION['__ZF']['_VALID']);
        $this->assertArrayHasKey(RemoteAddr::class, $_SESSION['__ZF']['_VALID']);
        $this->assertEquals('', $_SESSION['__ZF']['_VALID'][RemoteAddr::class]);
    }

    /**
     * @runInSeparateProcess
     */
    public function testRemoteAddressValidationWillFailOnInvalidAddress()
    {
        $this->manager = new SessionManager();
        $this->manager->getValidatorChain()
            ->attach('session.validate', [new RemoteAddr('123.123.123.123'), 'isValid']);

        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Session validation failed');
        $this->manager->start();
    }

    /**
     * @runInSeparateProcess
     */
    public function testRemoteAddressValidationWillSucceedWithValidPreSetData()
    {
        $this->manager = new SessionManager();
        $_SESSION = [
            '__ZF' => [
                '_VALID' => [
                    RemoteAddr::class => '',
                ],
            ],
        ];

        $this->manager->start();

        $this->assertTrue($this->manager->isValid());
    }

    /**
     * @runInSeparateProcess
     */
    public function testRemoteAddressValidationWillFailWithInvalidPreSetData()
    {
        $this->manager = new SessionManager();
        $_SESSION = [
            '__ZF' => [
                '_VALID' => [
                    RemoteAddr::class => '123.123.123.123',
                ],
            ],
        ];

        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Session validation failed');
        $this->manager->start();
    }

    /**
     * @runInSeparateProcess
     */
    public function testIdValidationWillFailOnInvalidData()
    {
        $this->manager = new SessionManager();
        $this->manager->getValidatorChain()
            ->attach('session.validate', [new Id('invalid-value'), 'isValid']);

        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Session validation failed');
        $this->manager->start();
    }
}
