<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Writer;

use Laminas\Db\Adapter\Adapter;
use Laminas\Log\Exception;
use Laminas\Log\Formatter\Db as DbFormatter;
use Traversable;

class Db extends AbstractWriter
{
    /**
     * Db adapter instance
     *
     * @var Adapter
     */
    protected $db;

    /**
     * Table name
     *
     * @var string
     */
    protected $tableName;

    /**
     * Relates database columns names to log data field keys.
     *
     * @var null|array
     */
    protected $columnMap;

    /**
     * Field separator for sub-elements
     *
     * @var string
     */
    protected $separator = '_';

    /**
     * Constructor
     *
     * We used the Adapter instead of Laminas\Db for a performance reason.
     *
     * @param Adapter|array|Traversable $db
     * @param string $tableName
     * @param array $columnMap
     * @param string $separator
     * @throws Exception\InvalidArgumentException
     */
    public function __construct($db, $tableName = null, array $columnMap = null, $separator = null)
    {
        if ($db instanceof Traversable) {
            $db = iterator_to_array($db);
        }

        if (is_array($db)) {
            parent::__construct($db);
            $separator = isset($db['separator']) ? $db['separator'] : null;
            $columnMap = isset($db['column']) ? $db['column'] : null;
            $tableName = isset($db['table']) ? $db['table'] : null;
            $db        = isset($db['db']) ? $db['db'] : null;
        }

        if (!$db instanceof Adapter) {
            throw new Exception\InvalidArgumentException('You must pass a valid Laminas\Db\Adapter\Adapter');
        }

        $tableName = (string) $tableName;
        if ('' === $tableName) {
            throw new Exception\InvalidArgumentException('You must specify a table name. Either directly in the constructor, or via options');
        }

        $this->db        = $db;
        $this->tableName = $tableName;
        $this->columnMap = $columnMap;

        if (!empty($separator)) {
            $this->separator = $separator;
        }

        if (!$this->hasFormatter()) {
            $this->setFormatter(new DbFormatter());
        }
    }

    /**
     * Remove reference to database adapter
     *
     * @return void
     */
    public function shutdown()
    {
        $this->db = null;
    }

    /**
     * Write a message to the log.
     *
     * @param array $event event data
     * @return void
     * @throws Exception\RuntimeException
     */
    protected function doWrite(array $event)
    {
        if (null === $this->db) {
            throw new Exception\RuntimeException('Database adapter is null');
        }

        $event = $this->formatter->format($event);

        // Transform the event array into fields
        if (null === $this->columnMap) {
            $dataToInsert = $this->eventIntoColumn($event);
        } else {
            $dataToInsert = $this->mapEventIntoColumn($event, $this->columnMap);
        }

        $statement = $this->db->query($this->prepareInsert($this->db, $this->tableName, $dataToInsert));
        $statement->execute($dataToInsert);
    }

    /**
     * Prepare the INSERT SQL statement
     *
     * @param  Adapter $db
     * @param  string $tableName
     * @param  array $fields
     * @return string
     */
    protected function prepareInsert(Adapter $db, $tableName, array $fields)
    {
        $keys = array_keys($fields);
        $sql = 'INSERT INTO ' . $db->platform->quoteIdentifier($tableName) . ' (' .
            implode(",", array_map([$db->platform, 'quoteIdentifier'], $keys)) . ') VALUES (' .
            implode(",", array_map([$db->driver, 'formatParameterName'], $keys)) . ')';

        return $sql;
    }

    /**
     * Map event into column using the $columnMap array
     *
     * @param  array $event
     * @param  array $columnMap
     * @return array
     */
    protected function mapEventIntoColumn(array $event, array $columnMap = null)
    {
        if (empty($event)) {
            return [];
        }

        $data = [];
        foreach ($event as $name => $value) {
            if (is_array($value)) {
                foreach ($value as $key => $subvalue) {
                    if (isset($columnMap[$name][$key])) {
                        if (is_scalar($subvalue)) {
                            $data[$columnMap[$name][$key]] = $subvalue;
                            continue;
                        }

                        $data[$columnMap[$name][$key]] = var_export($subvalue, true);
                    }
                }
            } elseif (isset($columnMap[$name])) {
                $data[$columnMap[$name]] = $value;
            }
        }
        return $data;
    }

    /**
     * Transform event into column for the db table
     *
     * @param  array $event
     * @return array
     */
    protected function eventIntoColumn(array $event)
    {
        if (empty($event)) {
            return [];
        }

        $data = [];
        foreach ($event as $name => $value) {
            if (is_array($value)) {
                foreach ($value as $key => $subvalue) {
                    if (is_scalar($subvalue)) {
                        $data[$name . $this->separator . $key] = $subvalue;
                        continue;
                    }

                    $data[$name . $this->separator . $key] = var_export($subvalue, true);
                }
            } else {
                $data[$name] = $value;
            }
        }
        return $data;
    }
}
