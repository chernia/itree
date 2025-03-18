-- Install the extension
CREATE EXTENSION itree;

-- Test table
CREATE TABLE test_itree (id itree(14));

-- Basic tests
INSERT INTO test_itree VALUES ('1.2.3.4');
INSERT INTO test_itree VALUES ('1.2.300.4');
INSERT INTO test_itree VALUES ('255.65535.0');
INSERT INTO test_itree VALUES ('1');
INSERT INTO test_itree VALUES ('1.2');

-- Verify output
SELECT id, id::text FROM test_itree;

-- Edge cases
DO $$
BEGIN
    -- Too many segments
    BEGIN
        INSERT INTO test_itree VALUES ('1.2.3.4.5.6.7.8.9.10.11.12.13.14.15');
    EXCEPTION WHEN invalid_text_representation THEN
        RAISE NOTICE 'Caught expected error: too many segments';
    END;

    -- Negative segment
    BEGIN
        INSERT INTO test_itree VALUES ('1.-2.3');
    EXCEPTION WHEN invalid_text_representation THEN
        RAISE NOTICE 'Caught expected error: negative segment';
    END;

    -- Oversized segment
    BEGIN
        INSERT INTO test_itree VALUES ('1.70000.3');
    EXCEPTION WHEN invalid_text_representation THEN
        RAISE NOTICE 'Caught expected error: segment too large';
    END;
END;
$$;

-- Cleanup
DROP TABLE test_itree;