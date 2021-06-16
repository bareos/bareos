<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\Config;

use phpmock\phpunit\PHPMock;
use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use SessionHandlerInterface;
use stdClass;
use Zend\Session\Config\SessionConfig;
use Zend\Session\Exception;
use Zend\Session\SaveHandler\SaveHandlerInterface;
use ZendTest\Session\TestAsset\TestSaveHandler;

/**
 * @runTestsInSeparateProcesses
 * @covers \Zend\Session\Config\SessionConfig
 */
class SessionConfigTest extends TestCase
{
    use PHPMock;

    /**
     * @var SessionConfig
     */
    protected $config;

    protected function setUp()
    {
        $this->config = new SessionConfig;
    }

    protected function tearDown()
    {
        $this->config = null;
    }

    public function assertPhpLessThan71($message = 'This test requires a PHP version less than 7.1.0')
    {
        if (PHP_VERSION_ID < 70100) {
            return true;
        }
        $this->markTestSkipped($message);
    }

    // session.save_path

    public function testSetSavePathErrorsOnInvalidPath()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid save_path provided');
        $this->config->setSavePath(__DIR__ . '/foobarboguspath');
    }

    public function testSavePathDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.save_path'), $this->config->getSavePath());
    }

    public function testSavePathIsMutable()
    {
        $this->config->setSavePath(__DIR__);
        $this->assertEquals(__DIR__, $this->config->getSavePath());
    }

    public function testSavePathAltersIniSetting()
    {
        $this->config->setSavePath(__DIR__);
        $this->assertEquals(__DIR__, ini_get('session.save_path'));
    }

    public function testSavePathCanBeNonDirectoryWhenSaveHandlerNotFiles()
    {
        $this->config->setPhpSaveHandler(TestSaveHandler::class);
        $this->config->setSavePath('/tmp/sessions.db');
        $this->assertEquals('/tmp/sessions.db', ini_get('session.save_path'));
    }

    // session.name

    public function testNameDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.name'), $this->config->getName());
    }

    public function testNameIsMutable()
    {
        $this->config->setName('FOOBAR');
        $this->assertEquals('FOOBAR', $this->config->getName());
    }

    public function testNameAltersIniSetting()
    {
        $this->config->setName('FOOBAR');
        $this->assertEquals('FOOBAR', ini_get('session.name'));
    }

    public function testNameAltersIniSettingAfterSessionStart()
    {
        session_start();

        $this->config->setName('FOOBAR');
        $this->assertEquals('FOOBAR', ini_get('session.name'));
    }

    public function testIdempotentNameAltersIniSettingWithSameValueAfterSessionStart()
    {
        $this->config->setName('FOOBAR');
        session_start();

        $this->config->setName('FOOBAR');
        $this->assertEquals('FOOBAR', ini_get('session.name'));
    }

    // session.save_handler

    public function testSaveHandlerDefaultsToIniSettings()
    {
        $this->assertSame(
            ini_get('session.save_handler'),
            $this->config->getSaveHandler(),
            var_export($this->config->toArray(), 1)
        );
    }

    public function testSaveHandlerIsMutable()
    {
        $this->config->setSaveHandler(TestSaveHandler::class);
        $this->assertSame(TestSaveHandler::class, $this->config->getSaveHandler());
    }

    public function testSaveHandlerDoesNotAlterIniSetting()
    {
        $this->config->setSaveHandler(TestSaveHandler::class);
        $this->assertNotSame(TestSaveHandler::class, ini_get('session.save_handler'));
    }

    public function testSettingInvalidSaveHandlerRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid save handler specified');
        $this->config->setPhpSaveHandler('foobar_bogus');
    }

    // session.gc_probability

    public function testGcProbabilityDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.gc_probability'), $this->config->getGcProbability());
    }

    public function testGcProbabilityIsMutable()
    {
        $this->config->setGcProbability(20);
        $this->assertEquals(20, $this->config->getGcProbability());
    }

    public function testGcProbabilityAltersIniSetting()
    {
        $this->config->setGcProbability(24);
        $this->assertEquals(24, ini_get('session.gc_probability'));
    }

    public function testSettingInvalidGcProbabilityRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_probability; must be numeric');
        $this->config->setGcProbability('foobar_bogus');
    }

    public function testSettingInvalidGcProbabilityRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_probability; must be a percentage');
        $this->config->setGcProbability(-1);
    }

    public function testSettingInvalidGcProbabilityRaisesException3()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_probability; must be a percentage');
        $this->config->setGcProbability(101);
    }

    // session.gc_divisor

    public function testGcDivisorDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.gc_divisor'), $this->config->getGcDivisor());
    }

    public function testGcDivisorIsMutable()
    {
        $this->config->setGcDivisor(20);
        $this->assertEquals(20, $this->config->getGcDivisor());
    }

    public function testGcDivisorAltersIniSetting()
    {
        $this->config->setGcDivisor(24);
        $this->assertEquals(24, ini_get('session.gc_divisor'));
    }

    public function testSettingInvalidGcDivisorRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_divisor; must be numeric');
        $this->config->setGcDivisor('foobar_bogus');
    }

    public function testSettingInvalidGcDivisorRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_divisor; must be a positive integer');
        $this->config->setGcDivisor(-1);
    }

    // session.gc_maxlifetime

    public function testGcMaxlifetimeDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.gc_maxlifetime'), $this->config->getGcMaxlifetime());
    }

    public function testGcMaxlifetimeIsMutable()
    {
        $this->config->setGcMaxlifetime(20);
        $this->assertEquals(20, $this->config->getGcMaxlifetime());
    }

    public function testGcMaxlifetimeAltersIniSetting()
    {
        $this->config->setGcMaxlifetime(24);
        $this->assertEquals(24, ini_get('session.gc_maxlifetime'));
    }

    public function testSettingInvalidGcMaxlifetimeRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_maxlifetime; must be numeric');
        $this->config->setGcMaxlifetime('foobar_bogus');
    }

    public function testSettingInvalidGcMaxlifetimeRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid gc_maxlifetime; must be a positive integer');
        $this->config->setGcMaxlifetime(-1);
    }

    // session.serialize_handler

    public function testSerializeHandlerDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.serialize_handler'), $this->config->getSerializeHandler());
    }

    public function testSerializeHandlerIsMutable()
    {
        $value = extension_loaded('wddx') ? 'wddx' : 'php_binary';
        $this->config->setSerializeHandler($value);
        $this->assertEquals($value, $this->config->getSerializeHandler());
    }

    public function testSerializeHandlerAltersIniSetting()
    {
        $value = extension_loaded('wddx') ? 'wddx' : 'php_binary';
        $this->config->setSerializeHandler($value);
        $this->assertEquals($value, ini_get('session.serialize_handler'));
    }

    public function testSettingInvalidSerializeHandlerRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid serialize handler specified');
        $this->config->setSerializeHandler('foobar_bogus');
    }

    // session.cookie_lifetime

    public function testCookieLifetimeDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cookie_lifetime'), $this->config->getCookieLifetime());
    }

    public function testCookieLifetimeIsMutable()
    {
        $this->config->setCookieLifetime(20);
        $this->assertEquals(20, $this->config->getCookieLifetime());
    }

    public function testCookieLifetimeAltersIniSetting()
    {
        $this->config->setCookieLifetime(24);
        $this->assertEquals(24, ini_get('session.cookie_lifetime'));
    }

    public function testCookieLifetimeCanBeZero()
    {
        $this->config->setCookieLifetime(0);
        $this->assertEquals(0, ini_get('session.cookie_lifetime'));
    }

    public function testSettingInvalidCookieLifetimeRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie_lifetime; must be numeric');
        $this->config->setCookieLifetime('foobar_bogus');
    }

    public function testSettingInvalidCookieLifetimeRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie_lifetime; must be a positive integer or zero');
        $this->config->setCookieLifetime(-1);
    }

    // session.cookie_path

    public function testCookiePathDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cookie_path'), $this->config->getCookiePath());
    }

    public function testCookiePathIsMutable()
    {
        $this->config->setCookiePath('/foo');
        $this->assertEquals('/foo', $this->config->getCookiePath());
    }

    public function testCookiePathAltersIniSetting()
    {
        $this->config->setCookiePath('/bar');
        $this->assertEquals('/bar', ini_get('session.cookie_path'));
    }

    public function testSettingInvalidCookiePathRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie path');
        $this->config->setCookiePath(24);
    }

    public function testSettingInvalidCookiePathRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie path');
        $this->config->setCookiePath('foo');
    }

    public function testSettingInvalidCookiePathRaisesException3()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie path');
        $this->config->setCookiePath('D:\\WINDOWS\\System32\\drivers\\etc\\hosts');
    }

    // session.cookie_domain

    public function testCookieDomainDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cookie_domain'), $this->config->getCookieDomain());
    }

    public function testCookieDomainIsMutable()
    {
        $this->config->setCookieDomain('example.com');
        $this->assertEquals('example.com', $this->config->getCookieDomain());
    }

    public function testCookieDomainCanBeEmpty()
    {
        $this->config->setCookieDomain('');
        $this->assertEquals('', $this->config->getCookieDomain());
    }

    public function testCookieDomainAltersIniSetting()
    {
        $this->config->setCookieDomain('localhost');
        $this->assertEquals('localhost', ini_get('session.cookie_domain'));
    }

    public function testSettingInvalidCookieDomainRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cookie domain: must be a string');
        $this->config->setCookieDomain(24);
    }

    public function testSettingInvalidCookieDomainRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('does not match the expected structure for a DNS hostname');
        $this->config->setCookieDomain('D:\\WINDOWS\\System32\\drivers\\etc\\hosts');
    }

    // session.cookie_secure

    public function testCookieSecureDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cookie_secure'), $this->config->getCookieSecure());
    }

    public function testCookieSecureIsMutable()
    {
        $value = ! ini_get('session.cookie_secure');
        $this->config->setCookieSecure($value);
        $this->assertEquals($value, $this->config->getCookieSecure());
    }

    public function testCookieSecureAltersIniSetting()
    {
        $value = ! ini_get('session.cookie_secure');
        $this->config->setCookieSecure($value);
        $this->assertEquals($value, ini_get('session.cookie_secure'));
    }

    // session.cookie_httponly

    public function testCookieHttpOnlyDefaultsToIniSettings()
    {
        $this->assertSame((bool) ini_get('session.cookie_httponly'), $this->config->getCookieHttpOnly());
    }

    public function testCookieHttpOnlyIsMutable()
    {
        $value = ! ini_get('session.cookie_httponly');
        $this->config->setCookieHttpOnly($value);
        $this->assertEquals($value, $this->config->getCookieHttpOnly());
    }

    public function testCookieHttpOnlyAltersIniSetting()
    {
        $value = ! ini_get('session.cookie_httponly');
        $this->config->setCookieHttpOnly($value);
        $this->assertEquals($value, ini_get('session.cookie_httponly'));
    }

    // session.use_cookies

    public function testUseCookiesDefaultsToIniSettings()
    {
        $this->assertSame((bool) ini_get('session.use_cookies'), $this->config->getUseCookies());
    }

    public function testUseCookiesIsMutable()
    {
        $value = ! ini_get('session.use_cookies');
        $this->config->setUseCookies($value);
        $this->assertEquals($value, $this->config->getUseCookies());
    }

    public function testUseCookiesAltersIniSetting()
    {
        $value = ! ini_get('session.use_cookies');
        $this->config->setUseCookies($value);
        $this->assertEquals($value, (bool) ini_get('session.use_cookies'));
    }

    // session.use_only_cookies

    public function testUseOnlyCookiesDefaultsToIniSettings()
    {
        $this->assertSame((bool) ini_get('session.use_only_cookies'), $this->config->getUseOnlyCookies());
    }

    public function testUseOnlyCookiesIsMutable()
    {
        $value = ! ini_get('session.use_only_cookies');
        $this->config->setOption('use_only_cookies', $value);
        $this->assertEquals($value, (bool) $this->config->getOption('use_only_cookies'));
    }

    public function testUseOnlyCookiesAltersIniSetting()
    {
        $value = ! ini_get('session.use_only_cookies');
        $this->config->setOption('use_only_cookies', $value);
        $this->assertEquals($value, (bool) ini_get('session.use_only_cookies'));
    }

    // session.referer_check

    public function testRefererCheckDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.referer_check'), $this->config->getRefererCheck());
    }

    public function testRefererCheckIsMutable()
    {
        $this->config->setOption('referer_check', 'FOOBAR');
        $this->assertEquals('FOOBAR', $this->config->getOption('referer_check'));
    }

    public function testRefererCheckMayBeEmpty()
    {
        $this->config->setOption('referer_check', '');
        $this->assertEquals('', $this->config->getOption('referer_check'));
    }

    public function testRefererCheckAltersIniSetting()
    {
        $this->config->setOption('referer_check', 'BARBAZ');
        $this->assertEquals('BARBAZ', ini_get('session.referer_check'));
    }

    // session.entropy_file

    public function testSetEntropyFileErrorsOnInvalidPath()
    {
        $this->assertPhpLessThan71('session.entropy_file directive removed in PHP 7.1');
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid entropy_file provided');
        $this->config->setEntropyFile(__DIR__ . '/foobarboguspath');
    }

    public function testEntropyFileDefaultsToIniSettings()
    {
        $this->assertPhpLessThan71('session.entropy_file directive removed in PHP 7.1');
        $this->assertSame(ini_get('session.entropy_file'), $this->config->getEntropyFile());
    }

    public function testEntropyFileIsMutable()
    {
        $this->assertPhpLessThan71('session.entropy_file directive removed in PHP 7.1');
        $this->config->setEntropyFile(__FILE__);
        $this->assertEquals(__FILE__, $this->config->getEntropyFile());
    }

    public function testEntropyFileAltersIniSetting()
    {
        $this->assertPhpLessThan71('session.entropy_file directive removed in PHP 7.1');
        $this->config->setEntropyFile(__FILE__);
        $this->assertEquals(__FILE__, ini_get('session.entropy_file'));
    }

    /**
     * @requires PHP 7.1
     */
    public function testSetEntropyFileError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->getEntropyFile();
    }

    /**
     * @requires PHP 7.1
     */
    public function testGetEntropyFileError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->setEntropyFile(__FILE__);
    }

    // session.entropy_length

    public function testEntropyLengthDefaultsToIniSettings()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->assertSame(ini_get('session.entropy_length'), $this->config->getEntropyLength());
    }

    public function testEntropyLengthIsMutable()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->config->setEntropyLength(20);
        $this->assertEquals(20, $this->config->getEntropyLength());
    }

    public function testEntropyLengthAltersIniSetting()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->config->setEntropyLength(24);
        $this->assertEquals(24, ini_get('session.entropy_length'));
    }

    public function testEntropyLengthCanBeZero()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->config->setEntropyLength(0);
        $this->assertEquals(0, ini_get('session.entropy_length'));
    }

    public function testSettingInvalidEntropyLengthRaisesException()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid entropy_length; must be numeric');
        $this->config->setEntropyLength('foobar_bogus');
    }

    public function testSettingInvalidEntropyLengthRaisesException2()
    {
        $this->assertPhpLessThan71('session.entropy_length directive removed in PHP 7.1');
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid entropy_length; must be a positive integer or zero');
        $this->config->setEntropyLength(-1);
    }

    /**
     * @requires PHP 7.1
     */
    public function testGetEntropyLengthError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->getEntropyLength();
    }

    /**
     * @requires PHP 7.1
     */
    public function testSetEntropyLengthError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->setEntropyLength(0);
    }

    // session.cache_limiter

    public function cacheLimiters()
    {
        return [
            [''],
            ['nocache'],
            ['public'],
            ['private'],
            ['private_no_expire'],
        ];
    }

    public function testCacheLimiterDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cache_limiter'), $this->config->getCacheLimiter());
    }

    /**
     * @dataProvider cacheLimiters
     */
    public function testCacheLimiterIsMutable($cacheLimiter)
    {
        $this->config->setCacheLimiter($cacheLimiter);
        $this->assertEquals($cacheLimiter, $this->config->getCacheLimiter());
    }

    /**
     * @dataProvider cacheLimiters
     */
    public function testCacheLimiterAltersIniSetting($cacheLimiter)
    {
        $this->config->setCacheLimiter($cacheLimiter);
        $this->assertEquals($cacheLimiter, ini_get('session.cache_limiter'));
    }

    public function testSettingInvalidCacheLimiterRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cache limiter provided');
        $this->config->setCacheLimiter('foobar_bogus');
    }

    // session.cache_expire

    public function testCacheExpireDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.cache_expire'), $this->config->getCacheExpire());
    }

    public function testCacheExpireIsMutable()
    {
        $this->config->setCacheExpire(20);
        $this->assertEquals(20, $this->config->getCacheExpire());
    }

    public function testCacheExpireAltersIniSetting()
    {
        $this->config->setCacheExpire(24);
        $this->assertEquals(24, ini_get('session.cache_expire'));
    }

    public function testSettingInvalidCacheExpireRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cache_expire; must be numeric');
        $this->config->setCacheExpire('foobar_bogus');
    }

    public function testSettingInvalidCacheExpireRaisesException2()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid cache_expire; must be a positive integer');
        $this->config->setCacheExpire(-1);
    }

    // session.use_trans_sid

    public function testUseTransSidDefaultsToIniSettings()
    {
        $this->assertSame((bool) ini_get('session.use_trans_sid'), $this->config->getUseTransSid());
    }

    public function testUseTransSidIsMutable()
    {
        $value = ! ini_get('session.use_trans_sid');
        $this->config->setOption('use_trans_sid', $value);
        $this->assertEquals($value, (bool) $this->config->getOption('use_trans_sid'));
    }

    public function testUseTransSidAltersIniSetting()
    {
        $value = ! ini_get('session.use_trans_sid');
        $this->config->setOption('use_trans_sid', $value);
        $this->assertEquals($value, (bool) ini_get('session.use_trans_sid'));
    }

    // session.hash_function

    public function hashFunctions()
    {
        $hashFunctions = [0, 1] + hash_algos();
        $provider      = [];
        foreach ($hashFunctions as $function) {
            $provider[] = [$function];
        }
        return $provider;
    }

    public function testHashFunctionDefaultsToIniSettings()
    {
        $this->assertPhpLessThan71('session.hash_function directive removed in PHP 7.1');
        $this->assertSame(ini_get('session.hash_function'), $this->config->getHashFunction());
    }

    /**
     * @dataProvider hashFunctions
     */
    public function testHashFunctionIsMutable($hashFunction)
    {
        $this->assertPhpLessThan71('session.hash_function directive removed in PHP 7.1');
        $this->config->setHashFunction($hashFunction);
        $this->assertEquals($hashFunction, $this->config->getHashFunction());
    }

    /**
     * @dataProvider hashFunctions
     */
    public function testHashFunctionAltersIniSetting($hashFunction)
    {
        $this->assertPhpLessThan71('session.hash_function directive removed in PHP 7.1');
        $this->config->setHashFunction($hashFunction);
        $this->assertEquals($hashFunction, ini_get('session.hash_function'));
    }

    public function testSettingInvalidHashFunctionRaisesException()
    {
        $this->assertPhpLessThan71('session.hash_function directive removed in PHP 7.1');
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid hash function provided');
        $this->config->setHashFunction('foobar_bogus');
    }

    /**
     * @requires PHP 7.1
     */
    public function testGetHashFunctionError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->getHashFunction();
    }

    /**
     * @requires PHP 7.1
     */
    public function testSetHashFunctionError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->setHashFunction('foobar_bogus');
    }

    // session.hash_bits_per_character

    public function hashBitsPerCharacters()
    {
        return [
            [4],
            [5],
            [6],
        ];
    }

    public function testHashBitsPerCharacterDefaultsToIniSettings()
    {
        if (PHP_VERSION_ID >= 70100) {
            $this->markTestSkipped('session.hash_bits_per_character directive removed in PHP 7.1');
        }

        $this->assertSame(ini_get('session.hash_bits_per_character'), $this->config->getHashBitsPerCharacter());
    }

    /**
     * @dataProvider hashBitsPerCharacters
     */
    public function testHashBitsPerCharacterIsMutable($hashBitsPerCharacter)
    {
        $this->assertPhpLessThan71('session.hash_bits_per_character directive removed in PHP 7.1');
        $this->config->setHashBitsPerCharacter($hashBitsPerCharacter);
        $this->assertEquals($hashBitsPerCharacter, $this->config->getHashBitsPerCharacter());
    }

    /**
     * @dataProvider hashBitsPerCharacters
     */
    public function testHashBitsPerCharacterAltersIniSetting($hashBitsPerCharacter)
    {
        $this->assertPhpLessThan71('session.hash_bits_per_character directive removed in PHP 7.1');
        $this->config->setHashBitsPerCharacter($hashBitsPerCharacter);
        $this->assertEquals($hashBitsPerCharacter, ini_get('session.hash_bits_per_character'));
    }

    public function testSettingInvalidHashBitsPerCharacterRaisesException()
    {
        $this->assertPhpLessThan71('session.hash_bits_per_character directive removed in PHP 7.1');
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid hash bits per character provided');
        $this->config->setHashBitsPerCharacter('foobar_bogus');
    }

    /**
     * @requires PHP 7.1
     */
    public function testGetHashBitsPerCharacterError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->getHashBitsPerCharacter();
    }

    /**
     * @requires PHP 7.1
     */
    public function testSetHashBitsPerCharacterError()
    {
        $this->expectException('PHPUnit\Framework\Error\Deprecated');
        $this->config->setHashBitsPerCharacter(5);
    }

    // session.sid_length

    /**
     * @requires PHP 7.1
     */
    public function testSidLengthDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.sid_length'), $this->config->getSidLength());
    }

    /**
     * @requires PHP 7.1
     */
    public function testSidLengthIsMutable()
    {
        $this->config->setSidLength(40);
        $this->assertEquals(40, $this->config->getSidLength());
    }

    /**
     * @requires PHP 7.1
     */
    public function testSidLengthAltersIniSetting()
    {
        $this->config->setSidLength(40);
        $this->assertEquals(40, ini_get('session.sid_length'));
    }

    /**
     * @requires PHP 7.1
     */
    public function testSettingInvalidSidLengthRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid length provided');
        $this->config->setSidLength('foobar_bogus');
    }

    /**
     * @requires PHP 7.1
     */
    public function testSettingOutOfRangeSidLengthRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid length provided');
        $this->config->setSidLength(999);
    }

    // session.sid_bits_per_character

    public function sidSidPerCharacters()
    {
        return [
            [4],
            [5],
            [6],
        ];
    }

    /**
     * @requires PHP 7.1
     */
    public function testSidBitsPerCharacterDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('session.sid_bits_per_character'), $this->config->getSidBitsPerCharacter());
    }

    /**
     * @requires PHP 7.1
     * @dataProvider sidSidPerCharacters
     */
    public function testSidBitsPerCharacterIsMutable($sidBitsPerCharacter)
    {
        $this->config->setSidBitsPerCharacter($sidBitsPerCharacter);
        $this->assertEquals($sidBitsPerCharacter, $this->config->getSidBitsPerCharacter());
    }

    /**
     * @requires PHP 7.1
     * @dataProvider sidSidPerCharacters
     */
    public function testSidBitsPerCharacterAltersIniSetting($sidBitsPerCharacter)
    {
        $this->config->setSidBitsPerCharacter($sidBitsPerCharacter);
        $this->assertEquals($sidBitsPerCharacter, ini_get('session.sid_bits_per_character'));
    }

    /**
     * @requires PHP 7.1
     */
    public function testSettingInvalidSidBitsPerCharacterRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid sid bits per character provided');
        $this->config->setSidBitsPerCharacter('foobar_bogus');
    }

    /**
     * @requires PHP 7.1
     */
    public function testSettingOutOfBoundSidBitsPerCharacterRaisesException()
    {
        $this->expectException('Zend\Session\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid sid bits per character provided');
        $this->config->setSidBitsPerCharacter(999);
    }

    // url_rewriter.tags

    public function testUrlRewriterTagsDefaultsToIniSettings()
    {
        $this->assertSame(ini_get('url_rewriter.tags'), $this->config->getUrlRewriterTags());
    }

    public function testUrlRewriterTagsIsMutable()
    {
        $this->config->setUrlRewriterTags('a=href,form=action');
        $this->assertEquals('a=href,form=action', $this->config->getUrlRewriterTags());
    }

    public function testUrlRewriterTagsAltersIniSetting()
    {
        $this->config->setUrlRewriterTags('a=href,fieldset=');
        $this->assertEquals('a=href,fieldset=', ini_get('url_rewriter.tags'));
    }

    // remember_me_seconds

    public function testRememberMeSecondsDefaultsToTwoWeeks()
    {
        $this->assertEquals(1209600, $this->config->getRememberMeSeconds());
    }

    public function testRememberMeSecondsIsMutable()
    {
        $this->config->setRememberMeSeconds(604800);
        $this->assertEquals(604800, $this->config->getRememberMeSeconds());
    }

    // setOption

    /**
     * @dataProvider optionsProvider
     */
    public function testSetOptionSetsIniSetting($option, $getter, $value)
    {
        // Leaving out special cases.
        if ($option == 'remember_me_seconds' || $option == 'url_rewriter_tags') {
            $this->markTestSkipped('remember_me_seconds && url_rewriter_tags');
        }

        $this->config->setStorageOption($option, $value);
        $this->assertEquals(ini_get('session.' . $option), $value);
    }

    public function testSetOptionUrlRewriterTagsGetsMunged()
    {
        $value = 'a=href';
        $this->config->setStorageOption('url_rewriter_tags', $value);
        $this->assertEquals(ini_get('url_rewriter.tags'), $value);
    }

    public function testSetOptionRememberMeSecondsDoesNothing()
    {
        $this->markTestIncomplete('I have no idea how to test this.');
    }

    public function testSetOptionsThrowsExceptionOnInvalidKey()
    {
        $badKey = 'snarfblat';
        $value = 'foobar';

        $this->expectException('InvalidArgumentException');
        $this->config->setStorageOption($badKey, $value);
    }

    // setOptions

    /**
     * @dataProvider optionsProvider
     */
    public function testSetOptionsTranslatesUnderscoreSeparatedKeys($option, $getter, $value)
    {
        $options = [$option => $value];
        $this->config->setOptions($options);
        if ('getOption' == $getter) {
            $this->assertSame($value, $this->config->getOption($option));
        } else {
            $this->assertSame($value, $this->config->$getter());
        }
    }

    public function optionsProvider()
    {
        $commonOptions = [
            [
                'save_path',
                'getSavePath',
                __DIR__,
            ],
            [
                'name',
                'getName',
                'FOOBAR',
            ],
            'UserDefinedSaveHandler' => [
                'save_handler',
                'getOption',
                'files',
            ],
            [
                'gc_probability',
                'getGcProbability',
                42,
            ],
            [
                'gc_divisor',
                'getGcDivisor',
                3,
            ],
            [
                'gc_maxlifetime',
                'getGcMaxlifetime',
                180,
            ],
            [
                'serialize_handler',
                'getSerializeHandler',
                'php_binary',
            ],
            [
                'cookie_lifetime',
                'getCookieLifetime',
                180,
            ],
            [
                'cookie_path',
                'getCookiePath',
                '/foo/bar',
            ],
            [
                'cookie_domain',
                'getCookieDomain',
                'framework.zend.com',
            ],
            [
                'cookie_secure',
                'getCookieSecure',
                true,
            ],
            [
                'cookie_httponly',
                'getCookieHttpOnly',
                true,
            ],
            [
                'use_cookies',
                'getUseCookies',
                false,
            ],
            [
                'use_only_cookies',
                'getUseOnlyCookies',
                true,
            ],
            [
                'referer_check',
                'getRefererCheck',
                'foobar',
            ],
            [
                'cache_limiter',
                'getCacheLimiter',
                'private',
            ],
            [
                'cache_expire',
                'getCacheExpire',
                42,
            ],
            [
                'use_trans_sid',
                'getUseTransSid',
                true,
            ],
            [
                'url_rewriter_tags',
                'getUrlRewriterTags',
                'a=href',
            ],
        ];

        // These options are no longer present as of PHP 7.1.0
        if (PHP_VERSION_ID < 70100) {
            $commonOptions[] = [
                'entropy_file',
                'getEntropyFile',
                __FILE__,
            ];

            $commonOptions[] = [
                'entropy_length',
                'getEntropyLength',
                42,
            ];

            // The docs do not say this was removed for 7.1.0, but tests fail
            // on 7.1.0 due to this one.
            $commonOptions[] = [
                'hash_function',
                'getHashFunction',
                'md5',
            ];

            $commonOptions[] = [
                'hash_bits_per_character',
                'getOption',
                5,
            ];
        }

        // New options as of PHP 7.1.0
        if (PHP_VERSION_ID >= 70100) {
            $commonOptions[] = [
                'sid_length',
                'getSidLength',
                40,
            ];

            $commonOptions[] = [
                'sid_bits_per_character',
                'getSidBitsPerCharacter',
                5,
            ];
        }

        return $commonOptions;
    }

    public function testSetPhpSaveHandlerRaisesExceptionForAttemptsToSetUserModule()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid save handler specified ("user")');
        $this->config->setPhpSaveHandler('user');
    }

    public function testErrorSettingKnownSaveHandlerResultsInException()
    {
        $r = new ReflectionProperty($this->config, 'knownSaveHandlers');
        $r->setAccessible(true);
        $r->setValue($this->config, ['files', 'notreallyredis']);

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Error setting session save handler module "notreallyredis"');
        $this->config->setPhpSaveHandler('notreallyredis');
    }

    public function testProvidingNonSessionHandlerToSetPhpSaveHandlerResultsInException()
    {
        $handler = new stdClass();

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('("stdClass"); must implement SessionHandlerInterface');
        $this->config->setPhpSaveHandler($handler);
    }

    public function testProvidingValidKnownSessionHandlerToSetPhpSaveHandlerResultsInNoErrors()
    {
        $phpinfo = $this->getFunctionMock('Zend\Session\Config', 'phpinfo');
        $phpinfo
            ->expects($this->once())
            ->will($this->returnCallback(function () {
                echo "Registered save handlers => user files unittest";
            }));

        $sessionModuleName = $this->getFunctionMock('Zend\Session\Config', 'session_module_name');
        $sessionModuleName->expects($this->once());

        $this->assertSame($this->config, $this->config->setPhpSaveHandler('unittest'));
        $this->assertEquals('unittest', $this->config->getOption('save_handler'));
    }

    public function testCanProvidePathWhenUsingRedisSaveHandler()
    {
        $phpinfo = $this->getFunctionMock('Zend\Session\Config', 'phpinfo');
        $phpinfo
            ->expects($this->once())
            ->will($this->returnCallback(function () {
                echo "Registered save handlers => user files redis";
            }));

        $sessionModuleName = $this->getFunctionMock('Zend\Session\Config', 'session_module_name');
        $sessionModuleName
            ->expects($this->once())
            ->with($this->equalTo('redis'));

        $url = 'tcp://localhost:6379?auth=foobar&database=1';

        $this->config->setOption('save_handler', 'redis');
        $this->config->setOption('save_path', $url);

        $this->assertSame($url, $this->config->getOption('save_path'));
    }

    public function testNotCallLocateRegisteredSaveHandlersMethodIfSessionHandlerInterfaceWasPassed()
    {
        $phpinfo = $this->getFunctionMock('Zend\Session\Config', 'phpinfo');
        $phpinfo
            ->expects($this->never());

        $saveHandler = $this->createMock(SessionHandlerInterface::class);
        $this->config->setPhpSaveHandler($saveHandler);
    }
}
