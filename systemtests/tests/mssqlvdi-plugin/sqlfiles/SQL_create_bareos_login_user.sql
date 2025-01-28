
--DROP LOGIN bareos;
--go 

--CREATE LOGIN bareos WITH PASSWORD = 'Sup3rS3crEt24';
--go

USE [$(myDB)]
go

DROP user IF EXISTS bareos ;

CREATE user bareos for login bareos;
