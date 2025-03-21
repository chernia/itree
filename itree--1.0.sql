-- Extension: itree

-- Step 1: Create a shell type
CREATE TYPE itree;

-- Step 2: Define the I/O and typmod functions
CREATE FUNCTION itree_in(cstring) RETURNS itree
    AS 'MODULE_PATHNAME', 'itree_in'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_out(itree) RETURNS cstring
    AS 'MODULE_PATHNAME', 'itree_out'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_typmod_in(cstring[]) RETURNS int4
    AS 'MODULE_PATHNAME', 'itree_typmod_in'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_typmod_out(int4) RETURNS cstring
    AS 'MODULE_PATHNAME', 'itree_typmod_out'
    LANGUAGE C IMMUTABLE STRICT;

-- Step 3: Complete the type definition
CREATE TYPE itree (
    INPUT = itree_in,
    OUTPUT = itree_out,
    STORAGE = plain,
    TYPMOD_IN = itree_typmod_in,
    TYPMOD_OUT = itree_typmod_out,
    INTERNALLENGTH = 16
);

-- Step 3: Define btree operators and their functions
-- Comparison operators
CREATE FUNCTION itree_lt(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_lt'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_le(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_le'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_eq(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_eq'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_ge(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_ge'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_gt(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_gt'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_cmp(itree, itree) RETURNS int4
    AS 'MODULE_PATHNAME', 'itree_cmp'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR < (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_lt,
    COMMUTATOR = >,
    NEGATOR = >=
);
CREATE OPERATOR <= (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_le,
    COMMUTATOR = >=,
    NEGATOR = >
);
CREATE OPERATOR = (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_eq,
    COMMUTATOR = =,
    NEGATOR = <>
);
CREATE OPERATOR >= (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_ge,
    COMMUTATOR = <=,
    NEGATOR = <
);
CREATE OPERATOR > (
    LEFTARG = itree,
    RIGHTARG = itree,
    PROCEDURE = itree_gt,
    COMMUTATOR = <,
    NEGATOR = <=
);

-- B-tree operator class
CREATE OPERATOR CLASS itree_btree_ops
    DEFAULT FOR TYPE itree USING btree AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >,
        FUNCTION 1 itree_cmp(itree, itree);

-- Step 4: Define operators and their functions
CREATE FUNCTION itree_is_descendant(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_is_descendant'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_is_ancestor(itree, itree) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_is_ancestor'
    LANGUAGE C IMMUTABLE STRICT;


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


-- Step 5: Define GIN support functionsCREATE FUNCTION itree_extract_value(itree, internal, internal) RETURNS internal
CREATE FUNCTION itree_extract_value(itree, internal, internal) RETURNS internal
    AS 'MODULE_PATHNAME', 'itree_extract_value'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_consistent(internal, smallint, itree, int, internal, internal, internal, internal) RETURNS bool
    AS 'MODULE_PATHNAME', 'itree_consistent'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_compare_partial(itree, itree, smallint) RETURNS int4
    AS 'MODULE_PATHNAME', 'itree_compare_partial'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_extract_query(itree, internal, smallint, internal, internal, internal, internal) RETURNS internal
    AS 'MODULE_PATHNAME', 'itree_extract_query'
    LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION itree_compare(itree, itree) RETURNS int4
    AS 'MODULE_PATHNAME', 'itree_compare'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR CLASS itree_gin_ops
    FOR TYPE itree USING gin AS
        OPERATOR 1 <@,
        OPERATOR 2 @>,
        FUNCTION 1 itree_consistent(internal, smallint, itree, int, internal, internal, internal, internal),
        FUNCTION 2 itree_extract_value(itree, internal, internal),
        FUNCTION 3 itree_compare_partial(itree, itree, smallint),
        FUNCTION 4 itree_extract_query(itree, internal, smallint, internal, internal, internal, internal),
        FUNCTION 5 itree_compare(itree, itree);