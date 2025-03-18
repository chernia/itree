-- Extension: itree

-- Step 1: Create a shell type
CREATE TYPE itree;

-- Step 2: Define the I/O and typmod functions
CREATE FUNCTION itree_in(cstring) RETURNS itree
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_out(itree) RETURNS cstring
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_typmod_in(cstring[]) RETURNS int
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

--not needed for a simple integer typmod, but later if we add more variables like max segment length
CREATE FUNCTION itree_typmod_out(int) RETURNS cstring
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

-- Step 3: Complete the type definition
CREATE TYPE itree (
    INPUT = itree_in,
    OUTPUT = itree_out,
    STORAGE = plain,
    TYPMOD_IN = itree_typmod_in,
    TYPMOD_OUT = itree_typmod_out,
    INTERNALLENGTH = 16
);

-- Step 4: Define operators and their functions
CREATE FUNCTION itree_is_descendant(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_is_ancestor(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <@ (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_is_descendant,
    COMMUTATOR = @>
);
CREATE OPERATOR @> (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_is_ancestor,
    COMMUTATOR = <@
);

--add equality operator to eliminate padding 0s
CREATE FUNCTION itree_eq(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE OPERATOR = (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_eq,
    COMMUTATOR = =
);
-- Step 5: Define GIN support functions
CREATE FUNCTION itree_extract_value(itree, internal, internal) RETURNS internal
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_consistent(internal, itree, smallint, int, int, internal) RETURNS bool
    AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

-- Step 6: Create the GIN operator class
CREATE OPERATOR CLASS itree_gin_ops
    FOR TYPE itree USING gin AS
        OPERATOR 1 <@,
        OPERATOR 2 @>,
        FUNCTION 1 itree_consistent(internal, itree, smallint, int, int, internal),
        FUNCTION 2 itree_extract_value(itree, internal, internal)
;