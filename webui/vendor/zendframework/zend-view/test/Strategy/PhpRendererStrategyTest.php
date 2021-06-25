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
use Zend\Http\Response as HttpResponse;
use Zend\View\Renderer\PhpRenderer;
use Zend\View\Strategy\PhpRendererStrategy;
use Zend\View\ViewEvent;

class PhpRendererStrategyTest extends TestCase
{
    use EventListenerIntrospectionTrait;

    /** @var PhpRendererStrategy */
    private $strategy;

    public function setUp()
    {
        $this->renderer = new PhpRenderer;
        $this->strategy = new PhpRendererStrategy($this->renderer);
        $this->event    = new ViewEvent();
        $this->response = new HttpResponse();
    }

    public function testSelectRendererAlwaysSelectsPhpRenderer()
    {
        $result = $this->strategy->selectRenderer($this->event);
        $this->assertSame($this->renderer, $result);
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
        $renderer = new PhpRenderer();
        $this->event->setRenderer($renderer);
        $this->strategy->injectResponse($this->event);
        $this->assertResponseNotInjected();
    }

    public function testResponseContentSetToContentPlaceholderWhenResultAndArticlePlaceholderAreEmpty()
    {
        $this->renderer->placeholder('content')->set('Content');
        $event = new ViewEvent();
        $event->setResponse($this->response)
              ->setRenderer($this->renderer);

        $this->strategy->injectResponse($event);
        $content = $this->response->getContent();
        $this->assertEquals('Content', $content);
    }

    public function testResponseContentSetToArticlePlaceholderWhenResultIsEmptyAndBothArticleAndContentPlaceholdersSet()
    {
        $this->renderer->placeholder('article')->set('Article Content');
        $this->renderer->placeholder('content')->set('Content');
        $event = new ViewEvent();
        $event->setResponse($this->response)
              ->setRenderer($this->renderer);

        $this->strategy->injectResponse($event);
        $content = $this->response->getContent();
        $this->assertEquals('Article Content', $content);
    }

    public function testResponseContentSetToResultIfNotEmpty()
    {
        $this->renderer->placeholder('article')->set('Article Content');
        $this->renderer->placeholder('content')->set('Content');
        $event = new ViewEvent();
        $event->setResponse($this->response)
              ->setRenderer($this->renderer)
              ->setResult('Result Content');

        $this->strategy->injectResponse($event);
        $content = $this->response->getContent();
        $this->assertEquals('Result Content', $content);
    }

    public function testContentPlaceholdersIncludeContentAndArticleByDefault()
    {
        $this->assertEquals(['article', 'content'], $this->strategy->getContentPlaceholders());
    }

    public function testContentPlaceholdersListIsMutable()
    {
        $this->strategy->setContentPlaceholders(['foo', 'bar']);
        $this->assertEquals(['foo', 'bar'], $this->strategy->getContentPlaceholders());
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

    public function testInjectResponseWorksWithAnEventWithNoResponse()
    {
        $e = new ViewEvent();
        $e->setRenderer($this->strategy->getRenderer());

        $this->strategy->injectResponse($e);

        $this->assertNull($e->getResponse());
    }
}
