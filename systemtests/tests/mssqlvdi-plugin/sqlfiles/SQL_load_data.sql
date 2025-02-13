USE [$(myDB)]
GO

SET IDENTITY_INSERT tests.samples ON;

INSERT INTO tests.samples(sample_id,sample_name) VALUES(1,'sample 1')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(2,'sample 2')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(3,'sample 3')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(4,'sample 4')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(5,'sample 5')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(6,'sample 6')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(7,'sample 7')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(8,'sample 8')
INSERT INTO tests.samples(sample_id,sample_name) VALUES(9,'sample 9')

SET IDENTITY_INSERT tests.samples OFF;
