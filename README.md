# itree 
`itree` is inspired by the great extension [LTREE](https://www.postgresql.org/docs/current/ltree.html), but is limited to positive integer segment values with a fixed 18 byte storage and max 2 byte segment values.

The `itree` complements `ltree` and can serve as an ID of a hierarchical entity, where ltree can serve as a label path.
## Features
This Postgres extension implements a data type `itree` for representing a hierarchical integer tree structure like: `1.1.30.65535`, where each segment is a positive int >= 1 and <= 65535. Some basic operators are provided initially, more can be added as needed.

### Operators
| Operator  | Description                                                       |
|-----------|-------------------------------------------------------------------|
| itree @> itree → boolean | Is left argument an ancestor of right (or equal as ltree)  |
| itree <@ itree → boolean | Is left argument a descendant of right (or equal as ltree) |
| itree \|\| itree -> itree  | concatenate 2 itree values|
| itree \|\| int -> itree  | concatenate itree and an int |
| itree \|\| text -> itree  | concatenate itree and a text tree|

### Functions
| Function                    | Description              | Example               |
|-----------------------------|--------------------------|-----------------------|
| ilevel(itree) -> integer    | number of levels         | ilevel('1.2.3') -> 3  |
| subpath ( itree, offset integer, len integer ) → itree | Returns subpath of itree starting at position offset, with length len. | subpath('1.2.3.4.5', 0, 2) → 1.2 |
| subitree ( itree, start integer, end integer ) → itree | Returns subpath of itree from position start to position end-1 (counting from 0).| subitree('1.2.3.4', 1, 2) → 2 |

### Data Structure
`itree` uses a fixed length 18 bytes with 2 control and 16 data bytes, which hold segments with variable length  from 1 to 2 bytes per segment.

Control bits 0-15 of 1 indicate a new segment, while `0` means the data byte is added to the previous segment. Segment value `0` is disallowed as it is interpreted as an end of the itree when control bit is 1. 

### Indexes
- B-tree over itree: <, <=, =, >=, >
- GIN index over(itree_gin_ops opclass): <, <=, =, >=, > <@, @> 

Example of creating a GIN index:
```sql
CREATE TABLE ref_data (id itree. label text, label_path ltree);

CREATE TABLE entity(id uuid, reference_id itree references ref_data(id));

CREATE INDEX itree_gin_idx ON itree_gin_test USING GIN (ref_id itree_gin_ops);
```

# Installation
## Dockerfile
1. Edit the sample Dockerfile and build it with docker:  
`docker build -t postgres-itree .`  
and run with:
`docker run -d --name postgres-itree-container -e POSTGRES_PASSWORD=mysecretpassword -p 5432:5432 postgres-itree`
2. or modify and use the sample `docker-compose.yml`:
`docker-compose up -d` 

# Contributing
For hacking ITREE you need to build Postgres from source:

## 1.Install Postgres
1. Download Postgres source  
`git clone https://github.com/postgres/postgres.git`

2. Install Prerequisites  
`sudo apt-get install build-essential libreadline-dev zlib1g-dev flex bison pkg-config libicu-dev`

3. Configure  
```bash
cd postgres
./configure --enable-debug --enable-cassert
```

4. Build  
```bash
make world-bin
sudo make install-world-bin
```

5. Init Postgres  
For dev purpose the current user will own and run the postgres server.  
`sudo chown -R $USER$ /usr/local/pgsql`  
`initdb -D /usr/local/pgsql/data`

6. Start the server with log to the console
`pg_ctl -D /usr/local/pgsql/data start`
You can add `PGDATA=/usr/local/pgsql/data` to your env and use only `pg_ctl start`

7. Login as the current user.  
`psql -d postgres`
You create a new database with the name of the current user and log with only `psql`.

## 2.Hacking itree
1. Checkout itree code  
`cd postgres/contrib/`  
`git clone https://github.com/chernia/itree.git`

3. Compile and install.

```bash
make clean
make CFLAGS="-std=c99 -g -O0"
make install
```
4. restart postgres 
`pg_ctl -D /usr/local/pgsql/data restart`

5. Run tests
`make installcheck`

## 3.Debug
### VSCODE

1. .vscode/launch.json
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Attach to PostgreSQL Backend",
            "type": "cppdbg",
            "request": "attach",
            "program": "/usr/local/pgsql/bin/postgres",
            "processId": "${command:pickProcess}",
            "MIMode": "gdb",
            "setupCommands": [
                {"text": "-enable-pretty-printing", "ignoreFailures": true},
                {"text": "sharedlibrary itree"},
                {"text": "break itree_in"},
                {"text": "handle SIGSEGV stop print"}
            ]
        }
    ]
}
```

2. Get the backend process id of the `psql` session
 ```sql
CREATE EXTENSION itree;
SELECT pg_backend_pid();

```

### Manual GDB
1. Load itree in the psql session
`select '1.2.3'::itree;`
2. Get <backend_pid> with: `select pg_backend_pid();`
3. Attach GDB 
4. Add a breakpoint
```bash
gdb /usr/local/pgsql/bin/postgres <backend_pid>
(gdb) sharedlibrary itree
(gdb) break itree_in
(gdb) handle SIGSEGV stop print
(gdb) continue
```
### Test
`make installcheck` will run the sdl/itree.sql and compare with expected/itree.out

# License
Itree is released under the MIT License.

```
Copyright 2025 Ivan Stoev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```