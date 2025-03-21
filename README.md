# Itree extension install
## Docker

# Itree Development

## Install Postgres
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
`sudo chown -R user /usr/local/pgsql`
`initdb -D /usr/local/pgsql/data`

6. Start the server - log to console
`pg_ctl -D /usr/local/pgsql/data start`

7. Login as `user`  
`psql -d postgres`
This logs the current user as a postgres superuser.

## ITREE
1. Checkout itree code
`git clone https://github.com/chernia/itree.git`

2. Link in contrib  
`cd postgres/contrib/`  
`ln -s path_to_itree itree`

3. Compile and install.
Since we are not using postgres dev libs, but source code compiled distribution owned by the current os user, we don't need sudo. If installing as root add sudo env var, like: `sudo PG_CONFIG=/usr/local/pgsql/bin/pg_config make install`

```bash
make clean
CFLAGS="-g -O0" make
make install
```
- restart postgres 
`pg_ctl -D /usr/local/pgsql/data restart`

### Debug
#### VSCODE

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

2. `psql -d postgres`
 ```sql
CREATE EXTENSION itree;
CREATE TABLE test4 (id itree(15));
SELECT pg_backend_pid();
INSERT INTO test4 VALUES ('1.2.3');
```

#### GDB
1. Load itree in the psql session
`select '1.2.3'::itree;`
2. Get <backend_pid> with: `select pg_backend_pid();`
3. Attach GDB 
```bash
gdb /usr/local/pgsql/bin/postgres <backend_pid>
(gdb) sharedlibrary itree
(gdb) break itree_in
(gdb) handle SIGSEGV stop print
(gdb) continue
```
### Test
- inpsect opclass operators and functions:
psql \dAc, \dAf, and \dAo
- sql/itree.sql