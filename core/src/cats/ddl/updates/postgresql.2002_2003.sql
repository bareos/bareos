BEGIN; -- Necessary for Bareos core

--
-- We drop and recreate the stats table which was not used yet.
--
DROP TABLE DeviceStats;
CREATE TABLE DeviceStats (
    DeviceId          INTEGER     DEFAULT 0,
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    ReadTime          BIGINT      NOT NULL DEFAULT 0,
    WriteTime         BIGINT      NOT NULL DEFAULT 0,
    ReadBytes         BIGINT      DEFAULT 0,
    WriteBytes        BIGINT      DEFAULT 0,
    SpoolSize         BIGINT      DEFAULT 0,
    NumWaiting        SMALLINT    DEFAULT 0,
    NumWriters        SMALLINT    DEFAULT 0,
    MediaId           INTEGER     NOT NULL,
    VolCatBytes       BIGINT      DEFAULT 0,
    VolCatFiles       BIGINT      DEFAULT 0,
    VolCatBlocks      BIGINT      DEFAULT 0
);

--
-- We drop and recreate the stats table which was not used yet.
--
DROP TABLE JobStats;
CREATE TABLE JobStats (
    DeviceId          INTEGER     DEFAULT 0,
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    JobId             INTEGER     NOT NULL,
    JobFiles          INTEGER     DEFAULT 0,
    JobBytes          BIGINT      DEFAULT 0
);

CREATE TABLE TapeAlerts (
   DeviceId          INTEGER     DEFAULT 0,
   SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
   AlertFlags        BIGINT      DEFAULT 0
);

DROP TABLE CDImages;

UPDATE Version SET VersionId = 2003;
COMMIT;

set client_min_messages = fatal;

ANALYSE;
