-- CREATE LOGIN bareos WITH PASSWORD = 'Sup3rS3crEt24';

USE [$(myDB)]
go

create user bareos for login bareos;
