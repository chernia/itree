-- Install the extension
CREATE EXTENSION itree;

--max level 1 byte segments ok
SELECT '1.2.3.4.5.6.7.8.9.10.11.12.13.14'::itree;


-- more then max level ignored
SELECT '1.2.3.4.5.6.7.8.9.10.11.12.13.14.15'::itree;

-- more then 14 byte storage not allowed
select '1.2.3.4.5.6.7.8.9.10.11.12.13.14000'::itree;

--last empty segment is ignored
SELECT '1.2.3.4.5.6.7.8.9.10.11.12.13.14.'::itree;

--empty internal segment not allowed
SELECT '1..3'::itree;

--zero segment not allowed
SELECT '1.2.0'::itree;

--negative segment not allowed
SELECT '1.2.-3'::itree;

--2 byte segment
SELECT '1.256'::itree;


--TYPMOD


-- Test table
CREATE TABLE test_itree (id itree(14));

-- Cleanup
DROP TABLE test_itree;