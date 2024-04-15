# pg_show_vm

`pg_show_vm` and `pg_show_rel_vm` display the visibility map data for a specified relation, including its corresponding indexes and partitions.

While the `pg_class` view provides information on `relpages` and `relallvisible`, these values are approximations and can often be inaccurate.
In contrast, pg_show_vm consistently offers accurate visibility map data.


## Installation

You can install it to do the usual way shown below.

```
$ tar xvfj postgresql-16.0.tar.bz2
$ cd postgresql-16.0/contrib
$ git clone https://github.com/s-hironobu/pg_show_vm.git
$ cd pg_show_vm
$ make && make install
```

You must add the line shown below in your postgresql.conf.

```
shared_preload_libraries = 'pg_show_vm'
```

After starting your server, you must issue `CREATE EXTENSION` statement shown below.

```
postgres=# CREATE EXTENSION pg_show_vm;
```

## How to use

By issuing the following query, it shows the visibility map of the specified relation, in this case, `pgbench_accounts`.


```
postgres=# SELECT oid, relname, relpages, relallvisible FROM pg_class WHERE relname LIKE 'pgbench_accounts%';
  oid  |        relname        | relpages | relallvisible
-------+-----------------------+----------+---------------
 16391 | pgbench_accounts      |     1968 |          1968
 16407 | pgbench_accounts_pkey |      331 |             0
(2 rows)

postgres=# SELECT pg_show_vm('16391');
        pg_show_vm        
--------------------------
 (16391,1968,1968,1968,0)
 (16407,331,0,0,1)
(2 rows)

postgres=# SELECT * FROM pg_show_vm('16391');
 relid | relpages | all_visible | all_frozen | type 
-------+----------+-------------+------------+------
 16391 |     1968 |        1968 |       1968 |    0
 16407 |      331 |           0 |          0 |    1
(2 rows)

postgres=# UPDATE pgbench_accounts SET abalance = abalance + 1 WHERE aid < 20000;
UPDATE 19999
postgres=# SELECT oid, relname, relpages, relallvisible FROM pg_class WHERE relname LIKE 'pgbench_accounts%';
  oid  |        relname        | relpages | relallvisible
-------+-----------------------+----------+---------------
 16391 | pgbench_accounts      |     1968 |          1968
 16407 | pgbench_accounts_pkey |      331 |             0
(2 rows)

postgres=# SELECT * FROM pg_show_vm('16391');
 relid | relpages | all_visible | all_frozen | type 
-------+----------+-------------+------------+------
 16391 |     1968 |        1311 |       1311 |    0
 16407 |      331 |           0 |          0 |    1
(2 rows)
```

For easier use, the pg_show_rel_vm() function is introduced.
It allows you to use the relation's name directly, eliminating the need to look up its OID (Object Identifier).

```
postgres=# SELECT * FROM pg_show_rel_vm('pgbench_accounts', true, true);
 relid | relpages | all_visible | all_frozen | type
-------+----------+-------------+------------+------
 16391 |     1968 |        1311 |       1311 |    0
 16407 |      331 |           0 |          0 |    1
(2 rows)

postgres=# SELECT * FROM pg_show_rel_vm('pgbench_accounts', false, true);
 relid | relpages | all_visible | all_frozen | type
-------+----------+-------------+------------+------
 16391 |     1968 |        1311 |       1311 |    0
(1 row)
```


## Interface


### pg_show_vm(oid)

#### Input

1. The oid of the relation.

#### Output

1. relid: The oid of the specified relation.
2. relplages: The total number of all pages.
3. all_visible: the number of all visible pages.
4. all_frozen: the number of all frozen pages.
5. type: 0 - `relation`, 1 - `index`, 2 - `partition table`, 3 - `partition table's index`


### pg_show_rel_vm(relname TEXT, index BOOL, partition BOOL)

#### Input

1. relname: The name of the relation.
2. index: Whether to include information about indexes associated with the relation.
3. partition: Whether to include information about partitions associated with the relation.

#### Output

Same as `pg_show_vm(oid)`.

## Change Log
- 15 Apr, 2024: Added pg_show_rel_vm().
- 1 Jan, 2024: Version 1.0 Released.
