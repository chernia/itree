-- Drop and recreate extension for a clean slate
DROP EXTENSION IF EXISTS itree cascade;
CREATE EXTENSION itree;


--GIN operators
SELECT am.amname AS index_method,
       opf.opfname AS opfamily_name,
       amop.amopopr::regoperator AS opfamily_operator,
       amop.amopstrategy 
    FROM pg_am am, pg_opfamily opf, pg_amop amop
    WHERE opf.opfmethod = am.oid AND
          amop.amopfamily = opf.oid and am.amname ='gin' and opf.opfname = 'itree_gin_ops'
    ORDER BY index_method, opfamily_name, opfamily_operator;

--GIN support functions
SELECT
    opfname,
    amprocnum,
    proname
FROM pg_amproc
JOIN pg_proc ON pg_amproc.amproc = pg_proc.oid
JOIN pg_opfamily ON pg_amproc.amprocfamily = pg_opfamily.oid
WHERE opfname = 'itree_gin_ops'
ORDER BY amprocnum;

--BTREE
--operators
SELECT am.amname AS index_method,
       opf.opfname AS opfamily_name,
       amop.amopopr::regoperator AS opfamily_operator,
       amop.amopstrategy 
    FROM pg_am am, pg_opfamily opf, pg_amop amop
    WHERE opf.opfmethod = am.oid AND
          amop.amopfamily = opf.oid and am.amname ='btree' and opf.opfname = 'itree_btree_ops'
    ORDER BY index_method, opfamily_name, opfamily_operator;

SELECT
    opfname,
    amprocnum,
    proname
FROM pg_amproc
JOIN pg_proc ON pg_amproc.amproc = pg_proc.oid
JOIN pg_opfamily ON pg_amproc.amprocfamily = pg_opfamily.oid
WHERE opfname = 'itree_btree_ops'
ORDER BY amprocnum;

-- Test itree_out with NULL
SELECT NULL::itree; 
-- Expected: NULL

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

-- Test basic type creation and I/O
SELECT '1.2.3'::itree AS basic_input;
-- Expected: 1.2.3

SELECT '300.2'::itree AS two_byte_input;
-- Expected: 300.2

-- Test invalid inputs
DO $$
BEGIN
    PERFORM '0.1'::itree;  -- Should fail (zero segment)
    RAISE EXCEPTION 'Expected error not raised';
EXCEPTION
    WHEN invalid_text_representation THEN
        RAISE NOTICE 'Caught expected error: zero segment';
END;
$$;
-- Expected: NOTICE: Caught expected error: zero segment

DO $$
BEGIN
    PERFORM '70000'::itree;  -- Should fail (exceeds 65535)
    RAISE EXCEPTION 'Expected error not raised';
EXCEPTION
    WHEN invalid_text_representation THEN
        RAISE NOTICE 'Caught expected error: exceeds max value';
END;
$$;
-- Expected: NOTICE: Caught expected error: exceeds max value

-- Test equality operator
SELECT '1.2'::itree = '1.2'::itree AS eq_true;
-- Expected: t

SELECT '1.2'::itree = '1.3'::itree AS eq_false;
-- Expected: f

-- Test comparison operators (B-tree support)
SELECT '1.2'::itree < '1.2.3'::itree AS lt_true;
-- Expected: t

SELECT '1.2.3'::itree < '1.2'::itree AS lt_false;
-- Expected: f

SELECT '1.2'::itree <= '1.2'::itree AS le_true;
-- Expected: t

SELECT '1.2.3'::itree <= '1.2'::itree AS le_false;
-- Expected: f

SELECT '300'::itree > '2'::itree AS gt_true;
-- Expected: t

SELECT '2'::itree > '300'::itree AS gt_false;
-- Expected: f

SELECT '300'::itree >= '300'::itree AS ge_true;
-- Expected: t

SELECT '2'::itree >= '300'::itree AS ge_false;
-- Expected: f

-- Test hierarchical operators (GIN support)
SELECT '1.2.3'::itree <@ '1.2'::itree AS descendant_true;
-- Expected: t

SELECT '1.2'::itree <@ '1.2.3'::itree AS descendant_false;
-- Expected: f

SELECT '1.2'::itree <@ '1.2'::itree AS descendant_equal;
-- Expected: t

SELECT '300.2'::itree <@ '300'::itree AS descendant_2byte;
-- Expected: t

SELECT '1.2'::itree @> '1.2.3'::itree AS ancestor_true;
-- Expected: t

SELECT '1.2.3'::itree @> '1.2'::itree AS ancestor_false;
-- Expected: f

SELECT '1.2'::itree @> '1.2'::itree AS ancestor_equal;
-- Expected: t

SELECT '300'::itree @> '300.2'::itree AS ancestor_2byte;
-- Expected: t

SELECT ilevel('1.2.3'::itree) AS level_3;
-- Expected: 3

SELECT '1.2.3'::itree || '4.5.6'::itree AS concat_result;
-- Expected: 1.2.3.4.5.6

drop table if exists itree_pk;
-- Test B-tree index as primary key
CREATE TABLE itree_pk (
    id itree PRIMARY KEY
);

INSERT INTO itree_pk VALUES
    ('1'),
    ('1.2'),
    ('1.2.3'),
    ('2'),
    ('300'),
    ('300.2');

-- Test uniqueness
INSERT INTO itree_pk VALUES ('1.2')
ON CONFLICT DO NOTHING;
-- Expected: No duplicate inserted

SELECT id FROM itree_pk ORDER BY id;
-- Expected:
--  id    
-- -------
--  1
--  1.2
--  1.2.3
--  2
--  300
--  300.2

-- Test exact match
SELECT id FROM itree_pk WHERE id = '1.2'::itree;
-- Expected:
--  id  
-- -----
--  1.2

-- Test range query
SELECT id FROM itree_pk WHERE id > '1.2'::itree AND id < '300'::itree;
-- Expected:
--  id    
-- -------
--  1.2.3
--  2

-- Verify B-tree index usage
EXPLAIN SELECT id FROM itree_pk WHERE id = '1.2'::itree;
-- Expected: Index Scan using itree_pk_pkey

-- Test GIN index
drop table if exists itree_gin_test;
CREATE TABLE itree_gin_test (ref_id itree references itree_pk(id));

CREATE INDEX itree_gin_idx ON itree_gin_test USING GIN (ref_id itree_gin_ops);

INSERT INTO itree_gin_test VALUES 
    ('1'),
    ('1.2'),
    ('1.2.3'),
    ('2'),
    ('300'),
    ('300.2'),
    (null)
    ;

-- Force index usage
SET enable_seqscan = off;

-- Test descendants
SELECT ref_id FROM itree_gin_test WHERE ref_id <@ '1.2'::itree;
-- Expected:
--  id    
-- -------
--  1.2
--  1.2.3

-- Test ancestors
SELECT ref_id FROM itree_gin_test WHERE ref_id @> '1.2.3'::itree;
-- Expected:
--  id    
-- -------
--  1
--  1.2
--  1.2.3

-- Test 2-byte segment hierarchy
SELECT ref_id FROM itree_gin_test WHERE ref_id <@ '300'::itree;
-- Expected:
--  id    
-- -------
--  300
--  300.2

--Query is NULL
SELECT * FROM itree_gin_test WHERE ref_id @> NULL::itree;
-- Expected: No rows

SELECT * FROM itree_gin_test WHERE ref_id <@ NULL::itree;
-- Expected: No rows

-- Verify GIN index usage
EXPLAIN SELECT ref_id FROM itree_gin_test WHERE ref_id <@ '1.2'::itree;
-- Expected: Bitmap Index Scan on itree_gin_idx

-- Reset seqscan
SET enable_seqscan = on;