USE [$(myDB)]
GO

SET IDENTITY_INSERT production.brands ON;

INSERT INTO production.brands(brand_id,brand_name) VALUES(10,'Bareos SuperBike')

SET IDENTITY_INSERT production.brands OFF;
