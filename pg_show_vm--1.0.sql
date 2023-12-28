/* pg_show_vm/pg_show_vm--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_show_vm" to load this file. \quit

-- Register functions.
CREATE OR REPLACE FUNCTION pg_show_vm(
       IN relid oid,
       OUT relid int,
       OUT relpages int,
       OUT all_visible int,
       OUT all_frozen int,
       OUT type int
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;


-- GRANT SELECT ON pg_show_vm TO PUBLIC;

-- Don't want this to be available to non-superusers.
--REVOKE ALL ON FUNCTION pg_show_vm() FROM PUBLIC;
