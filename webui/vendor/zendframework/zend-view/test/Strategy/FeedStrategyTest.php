<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Strategy;

use PHPUnit\Framework\TestCase;
use Zend\EventManager\EventManager;
use Zend\EventManager\Test\EventListenerIntrospectionTrait;
use Zend\Feed\Writer\FeedFactory;
use Zend\Http\Request as HttpRequest;
use Zend\Http\Response as HttpResponse;
use Zend\View\Model\FeedModel;
use Zend\View\Model\ViewModel;
use Zend\View\Renderer\FeedRenderer;
use Zend\View\Strategy\FeedStrategy;
use Zend\View\ViewEvent;

class FeedStrategyTest extends TestCase
{
    use EventListenerIntrospectionTrait;

    public function setUp()
    {
        $this->markTestIncomplete('Re-enable tests after zend-feed has been updated to zend-servicemanager v3');
        $this->renderer = new FeedRenderer;
        $this->strategy = new FeedStrategy($this->renderer);
        $this->event    = new ViewEvent();
        $this->response = new HttpResponse();
    }

    public function testFeedModelSelectsFeedStrategy()
    {
        $this->event->setModel(new FeedModel());
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertSame($this->renderer, $result);
    }

    /**
     * @group #2410
     */
    public function testRssAcceptHeaderDoesNotSelectFeedStrategy()
    {
        $request = new HttpRequest();
        $request->getHeaders()->addHeaderLine('Accept', 'application/rss+xml');
        $this->event->setRequest($request);
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertNotSame($this->renderer, $result);
    }

    /**
     * @group #2410
     */
    public function testAtomAcceptHeaderDoesNotSelectFeedStrategy()
    {
        $request = new HttpRequest();
        $request->getHeaders()->addHeaderLine('Accept', 'application/atom+xml');
        $this->event->setRequest($request);
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertNotSame($this->renderer, $result);
    }

    public function testAcceptHeaderDoesNotSetFeedtype()
    {
        $this->event->setModel(new FeedModel());
        $request = new HttpRequest();
        $request->getHeaders()->addHeaderLine('Accept', 'application/atom+xml');
        $this->event->setRequest($request);
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertSame($this->renderer, $result);
        $this->assertNotSame('atom', $result->getFeedType());
    }

    public function testLackOfFeedModelOrAcceptHeaderDoesNotSelectFeedStrategy()
    {
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertNotSame($this->renderer, $result);
        $this->assertNull($result);
    }

    protected function assertResponseNotInjected()
    {
        $content = $this->response->getContent();
        $headers = $this->response->getHeaders();
        $this->assertEmpty($content);
        $this->assertFalse($headers->has('content-type'));
    }

    public function testNonMatchingRendererDoesNotInjectResponse()
    {
        $this->event->setResponse($this->response);

        // test empty renderer
        $this->strategy->injectResponse($this->event);
        $this->assertResponseNotInjected();

        // test non-matching renderer
        $renderer = new FeedRenderer();
        $this->event->setRenderer($renderer);
        $this->strategy->injectResponse($this->event);
        $this->assertResponseNotInjected();
    }

    public function testNonStringOrFeedResultDoesNotInjectResponse()
    {
        $this->event->setResponse($this->response);
        $this->event->setRenderer($this->renderer);
        $this->event->setResult($this->response);

        $this->strategy->injectResponse($this->event);
        $this->assertResponseNotInjected();
    }

    public function testMatchingRendererAndStringResultInjectsResponse()
    {
        $this->renderer->setFeedType('atom');
        $expected = '<?xml version="1.0"><root><content>content</content></root>';
        $this->event->setResponse($this->response);
        $this->event->setRenderer($this->renderer);
        $this->event->setResult($expected);

        $this->strategy->injectResponse($this->event);
        $content = $this->response->getContent();
        $headers = $this->response->getHeaders();
        $this->assertEquals($expected, $content);
        $this->assertTrue($headers->has('content-type'));
        $this->assertEquals('application/atom+xml', $headers->get('content-type')->getFieldValue());
    }

    protected function getFeedData($type)
    {
        return [
            'copyright' => date('Y'),
            'date_created' => time(),
            'date_modified' => time(),
            'last_build_date' => time(),
            'description' => __CLASS__,
            'id' => 'http://framework.zend.com/',
            'language' => 'en_US',
            'feed_link' => [
                'link' => 'http://framework.zend.com/feed.xml',
                'type' => $type,
            ],
            'link' => 'http://framework.zend.com/feed.xml',
            'title' => 'Testing',
            'encoding' => 'UTF-8',
            'base_url' => 'http://framework.zend.com/',
            'entries' => [
                [
                    'content' => 'test content',
                    'date_created' => time(),
                    'date_modified' => time(),
                    'description' => __CLASS__,
                    'id' => 'http://framework.zend.com/1',
                    'link' => 'http://framework.zend.com/1',
                    'title' => 'Test 1',
                ],
                [
                    'content' => 'test content',
                    'date_created' => time(),
                    'date_modified' => time(),
                    'description' => __CLASS__,
                    'id' => 'http://framework.zend.com/2',
                    'link' => 'http://framework.zend.com/2',
                    'title' => 'Test 2',
                ],
            ],
        ];
    }

    public function testMatchingRendererAndFeedResultInjectsResponse()
    {
        $this->renderer->setFeedType('atom');
        $expected = FeedFactory::factory($this->getFeedData('atom'));
        $this->event->setResponse($this->response);
        $this->event->setRenderer($this->renderer);
        $this->event->setResult($expected);

        $this->strategy->injectResponse($this->event);
        $content = $this->response->getContent();
        $headers = $this->response->getHeaders();
        $this->assertEquals($expected->export('atom'), $content);
        $this->assertTrue($headers->has('content-type'));
        $this->assertEquals('application/atom+xml', $headers->get('content-type')->getFieldValue());
    }

    public function testResponseContentTypeIsBasedOnFeedType()
    {
        $this->renderer->setFeedType('rss');
        $expected = FeedFactory::factory($this->getFeedData('rss'));
        $this->event->setResponse($this->response);
        $this->event->setRenderer($this->renderer);
        $this->event->setResult($expected);

        $this->strategy->injectResponse($this->event);
        $content = $this->response->getContent();
        $headers = $this->response->getHeaders();
        $this->assertEquals($expected->export('rss'), $content);
        $this->assertTrue($headers->has('content-type'));
        $this->assertEquals('application/rss+xml', $headers->get('content-type')->getFieldValue());
    }

    public function testReturnsNullWhenUnableToSelectRenderer()
    {
        $model   = new ViewModel();
        $request = new HttpRequest();
        $this->event->setModel($model);
        $this->event->setRequest($request);
        $this->assertNull($this->strategy->selectRenderer($this->event));
    }

    public function testAttachesListenersAtExpectedPriorities()
    {
        $events = new EventManager();
        $this->strategy->attach($events);

        foreach (['renderer' => 'selectRenderer', 'response' => 'injectResponse'] as $event => $method) {
            $listeners        = $this->getListenersForEvent($event, $events, true);
            $expectedListener = [$this->strategy, $method];
            $expectedPriority = 1;
            $found            = false;
            foreach ($listeners as $priority => $listener) {
                if ($listener === $expectedListener
                    && $priority === $expectedPriority
                ) {
                    $found = true;
                    break;
                }
            }
            $this->assertTrue($found, 'Listener not found');
        }
    }

    public function testCanAttachListenersAtSpecifiedPriority()
    {
        $events = new EventManager();
        $this->strategy->attach($events, 100);

        foreach (['renderer' => 'selectRenderer', 'response' => 'injectResponse'] as $event => $method) {
            $listeners        = $this->getListenersForEvent($event, $events, true);
            $expectedListener = [$this->strategy, $method];
            $expectedPriority = 100;
            $found            = false;
            foreach ($listeners as $priority => $listener) {
                if ($listener === $expectedListener
                    && $priority === $expectedPriority
                ) {
                    $found = true;
                    break;
                }
            }
            $this->assertTrue($found, 'Listener not found');
        }
    }

    public function testDetachesListeners()
    {
        $events = new EventManager();
        $this->strategy->attach($events, 100);

        $listeners = iterator_to_array($this->getListenersForEvent('renderer', $events));
        $this->assertCount(1, $listeners);
        $listeners = iterator_to_array($this->getListenersForEvent('response', $events));
        $this->assertCount(1, $listeners);

        $this->strategy->detach($events, 100);
        $listeners = iterator_to_array($this->getListenersForEvent('renderer', $events));
        $this->assertCount(0, $listeners);
        $listeners = iterator_to_array($this->getListenersForEvent('response', $events));
        $this->assertCount(0, $listeners);
    }
}
