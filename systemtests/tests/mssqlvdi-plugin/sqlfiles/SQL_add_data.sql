USE [$(myDB)]
GO

SET IDENTITY_INSERT tests.samples ON;

INSERT INTO tests.samples(sample_id,sample_name) VALUES(10,'sample 10')

SET IDENTITY_INSERT tests.samples OFF;
