<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter\Compress;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Compress\Tar as TarCompression;
use Zend\Filter\Exception\ExtensionNotLoadedException;

class TarLoadArchiveTarTest extends TestCase
{
    public function testArchiveTarNotLoaded()
    {
        set_error_handler(function ($errno, $errstr) {
            // PEAR class uses deprecated constructor, which emits a deprecation error
            return true;
        }, E_DEPRECATED);
        if (class_exists('Archive_Tar')) {
            restore_error_handler();
            $this->markTestSkipped('PEAR Archive_Tar is present; skipping test that expects its absence');
        }
        restore_error_handler();

        try {
            $tar = new TarCompression;
            $this->fail('ExtensionNotLoadedException was expected but not thrown');
        } catch (ExtensionNotLoadedException $e) {
        }
    }
}
