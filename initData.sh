#!/bin/bash


for file in config/*.txt;
do
    
    dbname="data/$(basename $(grep 'DB:' $file | cut -d: -f2))"
    sqlname="config/sqlscripts/$(basename $(grep 'SQL:' $file | cut -d: -f2))"
    
    sqlite3 $dbname ".read createTables.sql" ".read $sqlname" ".exit"
done
