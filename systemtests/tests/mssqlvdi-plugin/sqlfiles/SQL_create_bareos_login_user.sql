USE [$(myDB)]
go

DROP user IF EXISTS bareos ;

CREATE user bareos for login bareos;
