#!/bin/bash


for file in config/*.txt;
do 
    ./server $file &
done