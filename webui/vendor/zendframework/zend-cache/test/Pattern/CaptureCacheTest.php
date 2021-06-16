<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Pattern;

use Zend\Cache;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Pattern\CaptureCache<extended>
 */
class CaptureCacheTest extends CommonPatternTest
{
    // @codingStandardsIgnoreStart
    protected $_tmpCacheDir;
    protected $_umask;
    protected $_bufferedServerSuperGlobal;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_bufferedServerSuperGlobal = $_SERVER;
        $this->_umask = umask();

        $this->_tmpCacheDir = @tempnam(sys_get_temp_dir(), 'zend_cache_test_');
        if (! $this->_tmpCacheDir) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory-file: {$err['message']}");
        } elseif (! @unlink($this->_tmpCacheDir)) {
            $err = error_get_last();
            $this->fail("Can't remove temporary cache directory-file: {$err['message']}");
        } elseif (! @mkdir($this->_tmpCacheDir, 0777)) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory: {$err['message']}");
        }

        $this->_options = new Cache\Pattern\PatternOptions([
            'public_dir' => $this->_tmpCacheDir
        ]);
        $this->_pattern = new Cache\Pattern\CaptureCache();
        $this->_pattern->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        $_SERVER = $this->_bufferedServerSuperGlobal;

        $this->_removeRecursive($this->_tmpCacheDir);

        if ($this->_umask != umask()) {
            umask($this->_umask);
            $this->fail("Umask wasn't reset");
        }

        parent::tearDown();
    }

    // @codingStandardsIgnoreStart
    protected function _removeRecursive($dir)
    {
        // @codingStandardsIgnoreEnd
        if (file_exists($dir)) {
            $dirIt = new \DirectoryIterator($dir);
            foreach ($dirIt as $entry) {
                $fname = $entry->getFilename();
                if ($fname == '.' || $fname == '..') {
                    continue;
                }

                if ($entry->isFile()) {
                    unlink($entry->getPathname());
                } else {
                    $this->_removeRecursive($entry->getPathname());
                }
            }

            rmdir($dir);
        }
    }

    public function getCommonPatternNamesProvider()
    {
        return [
            ['capture'],
            ['Capture'],
        ];
    }

    public function testSetThrowsLogicExceptionOnMissingPublicDir()
    {
        $captureCache = new Cache\Pattern\CaptureCache();

        $this->expectException('Zend\Cache\Exception\LogicException');
        $captureCache->set('content', '/pageId');
    }

    public function testSetWithNormalPageId()
    {
        $this->_pattern->set('content', '/dir1/dir2/file');
        $this->assertFileExists($this->_tmpCacheDir . '/dir1/dir2/file');
        $this->assertSame(file_get_contents($this->_tmpCacheDir . '/dir1/dir2/file'), 'content');
    }

    public function testSetWithIndexFilename()
    {
        $this->_options->setIndexFilename('test.html');

        $this->_pattern->set('content', '/dir1/dir2/');
        $this->assertFileExists($this->_tmpCacheDir . '/dir1/dir2/test.html');
        $this->assertSame(file_get_contents($this->_tmpCacheDir . '/dir1/dir2/test.html'), 'content');
    }

    public function testGetThrowsLogicExceptionOnMissingPublicDir()
    {
        $captureCache = new Cache\Pattern\CaptureCache();

        $this->expectException('Zend\Cache\Exception\LogicException');
        $captureCache->get('/pageId');
    }

    public function testHasThrowsLogicExceptionOnMissingPublicDir()
    {
        $captureCache = new Cache\Pattern\CaptureCache();

        $this->expectException('Zend\Cache\Exception\LogicException');
        $captureCache->has('/pageId');
    }

    public function testRemoveThrowsLogicExceptionOnMissingPublicDir()
    {
        $captureCache = new Cache\Pattern\CaptureCache();

        $this->expectException('Zend\Cache\Exception\LogicException');
        $captureCache->remove('/pageId');
    }

    public function testGetFilenameWithoutPublicDir()
    {
        $captureCache = new Cache\Pattern\CaptureCache();
        $this->assertEquals(
            str_replace('/', DIRECTORY_SEPARATOR, '/index.html'),
            $captureCache->getFilename('/')
        );
        $this->assertEquals(
            str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test'),
            $captureCache->getFilename('/dir1/test')
        );
        $this->assertEquals(
            str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test.html'),
            $captureCache->getFilename('/dir1/test.html')
        );
        $this->assertEquals(
            str_replace('/', DIRECTORY_SEPARATOR, '/dir1/dir2/test.html'),
            $captureCache->getFilename('/dir1/dir2/test.html')
        );
    }

    public function testGetFilenameWithoutPublicDirAndNoPageId()
    {
        $_SERVER['REQUEST_URI'] = '/dir1/test.html';
        $captureCache = new Cache\Pattern\CaptureCache();
        $this->assertEquals(str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test.html'), $captureCache->getFilename());
    }

    public function testGetFilenameWithPublicDir()
    {
        $options = new Cache\Pattern\PatternOptions([
            'public_dir' => $this->_tmpCacheDir
        ]);

        $captureCache = new Cache\Pattern\CaptureCache();
        $captureCache->setOptions($options);

        $this->assertEquals(
            $this->_tmpCacheDir . str_replace('/', DIRECTORY_SEPARATOR, '/index.html'),
            $captureCache->getFilename('/')
        );
        $this->assertEquals(
            $this->_tmpCacheDir . str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test'),
            $captureCache->getFilename('/dir1/test')
        );
        $this->assertEquals(
            $this->_tmpCacheDir . str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test.html'),
            $captureCache->getFilename('/dir1/test.html')
        );
        $this->assertEquals(
            $this->_tmpCacheDir . str_replace('/', DIRECTORY_SEPARATOR, '/dir1/dir2/test.html'),
            $captureCache->getFilename('/dir1/dir2/test.html')
        );
    }

    public function testGetFilenameWithPublicDirAndNoPageId()
    {
        $_SERVER['REQUEST_URI'] = '/dir1/test.html';

        $options = new Cache\Pattern\PatternOptions([
            'public_dir' => $this->_tmpCacheDir
        ]);
        $captureCache = new Cache\Pattern\CaptureCache();
        $captureCache->setOptions($options);

        $this->assertEquals(
            $this->_tmpCacheDir . str_replace('/', DIRECTORY_SEPARATOR, '/dir1/test.html'),
            $captureCache->getFilename()
        );
    }
}
