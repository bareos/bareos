-- Inserting blob object
GO

SET IDENTITY_INSERT tests.filestreams ON;

INSERT INTO tests.filestreams (fstreams_id, fstream_name, fstream_binary)
VALUES
(NEWID(),
('sample_one',
  (SELECT * FROM OPENROWSET
  (BULK 'C:\Windows\explorer.exe',
  SINGLE_BLOB) AS fstream_binary)
)

SET IDENTITY_INSERT tests.filestreams OFF;
