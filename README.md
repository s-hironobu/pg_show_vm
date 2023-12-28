# pg_show_vm

`pg_show_vm` displays the visibility map data for a specified relation, including its corresponding indexes and partitions.

While the `pg_class` view provides information on `relpages` and `relallvisible`, these values are approximations and can often be inaccurate. In contrast, pg_show_vm consistently offers accurate visibility map data.


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
postgres=# SELECT oid, relname, relpages, relallvisible, relallfrozen FROM pg_class WHERE relname LIKE 'pgbench_accounts%';
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

postgres=# 
postgres=# UPDATE pgbench_accounts SET abalance = abalance + 1 WHERE aid < 20000;
UPDATE 19999
postgres=# SELECT oid, relname, relpages, relallvisible, relallfrozen FROM pg_class WHERE relname LIKE 'pgbench_accounts%';
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

## Interface

#### Input

1. oid of the specified relation

#### Output

1. relid: oid of the specified relation
2. relplages: number of all pages
3. all_visible: number of all visible pages
4. all_frozen: number of all frozen pages
5. type: 0 - `relation`, 1 - `index`, 2 - `partition table`, 3 - `partition table's index`


## Change Log
 - 1 Jan, 2024: Version 1.0 Released.
